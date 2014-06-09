#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include "channels.h"
#include "list.h"
#include "logging.h"
#include "forward.h"

struct pollfd pf[MAX_CONN];
struct channel *channel_of_pf[MAX_CONN];
struct channel *deque;
uint nfds;
unsigned int idle;

#define DB(fmt, args...) debug(3, "[chan]: " fmt, ##args)
#define DBINFO(fmt, args...) debug(2, "[chan]: " fmt, ##args)
#define DBWARN(fmt, args...) debug(1, "[chan]: " fmt, ##args)
#define DBERR(fmt, args...) debug(0, "[chan]: " fmt, ##args)

static void set_ip(struct channel *channel, char *ip)
{
	if ((strstr(ip, ":")) != NULL) {
		channel->af = AF_INET6;
		channel->v6.sin6_family = channel->af;
		inet_pton(channel->af, ip, &(channel->v6.sin6_addr));
	} else {
		channel->af = AF_INET;
		channel->v4.sin_family = channel->af;
		inet_pton(channel->af, ip, &(channel->v4.sin_addr));
	}
}

/* convert the IP address to string */
static char *addrstr(struct psockaddr *addr)
{
	if (addr->af == AF_INET6) {
		inet_ntop(addr->af, &(addr->v6.sin6_addr),
			  addr->addrstr, INET6_ADDRSTRLEN);
	} else {
		inet_ntop(addr->af, &(addr->v4.sin_addr),
			  addr->addrstr, INET_ADDRSTRLEN);
	}
	return addr->addrstr;
}

static void set_port(struct channel *channel, uint16_t port)
{
	if (channel->af == AF_INET6)
		channel->v6.sin6_port = htons(port);
	else
		channel->v4.sin_port = htons(port);
}

static int udp_recv(struct channel *channel)
{
	size_t bytes = 0;
	pbuffer *b = channel->recv_buffer;
	struct sockaddr *src;
	socklen_t len;

	channel->src.af = channel->af;
	src = psockaddr_saddr(&channel->src);
	len = psockaddr_len(&channel->src);

	/* stretch buffer to encompass message */
	while ((bytes = recvfrom(channel->fd, pbuffer_end(b), pbuffer_unused(b),
				 MSG_PEEK,
				 src,
				 &len)) >= pbuffer_unused(b)) {
		if (bytes == -1) {
			perror("recvfrom()");
			return -1;
		}
		pbuffer_assure(b, (bytes * 2) | PBUFFER_MIN);
	}

	/* actually receive the message */
	bytes = recvfrom(channel->fd, pbuffer_end(b), pbuffer_unused(b), 0,
			 src, &len);

	if (bytes == 0) {
		channel->flags = CHAN_CLOSE;
		return 0;
	}
	b->length += bytes;
	addrstr(&channel->src);
	return bytes;
}

static int tcp_recv(struct channel *channel)
{
	size_t bytes = 0;
	pbuffer *b = channel->recv_buffer;

	/* stretch buffer to encompass message */
	while ((bytes = recv(channel->fd, pbuffer_end(b), pbuffer_unused(b),
			     MSG_PEEK))
	       >= pbuffer_unused(b)) {
		if (bytes == -1) {
			perror("recv()");
			return -1;
		}
		pbuffer_assure(b, (bytes * 2) | PBUFFER_MIN);
	}

	/* actually receive the message */
	bytes = recv(channel->fd, pbuffer_end(b), pbuffer_unused(b), 0);
	if (bytes <= 0) {
		channel->flags = CHAN_CLOSE;
		return bytes;
	}
	b->length += bytes;
	return bytes;
}

static int tcp_send(struct channel *channel)
{
	int ret;
	pbuffer *b = channel->send_buffer;

	if (!b || !b->length) {
		return -1;
	}

	DB("sending %zu bytes", b->length);

	if ((ret = send(channel->fd, b->data, b->length, 0)) < 0) {
		perror("send");
		return -1;
	}
	pbuffer_clear(b);
	channel->flags &= ~CHAN_SEND;

	return ret;
}

static int channel_recv(struct channel *channel)
{
	int ret = 0;
	pbuffer *b = channel->recv_buffer;
	if (channel->on_recv) {
		ret = channel->on_recv(channel);
		if (ret > 0) {
			DB("received %u bytes from %s", ret,
			   psockaddr_string(&channel->src));
			hexdump(3, (unsigned char *)b->data, b->length);
			forward_message(channel);
		}
	} else {
		DB("No callback");
	}

	channel->flags &= ~CHAN_RECV;
	channel->pf->events |= EV_INPUT;

	return ret;
}

static int channel_send(struct channel *channel)
{
	int ret = 0;
	channel->flags &= ~CHAN_SEND;
	channel->pf->events &= ~EV_OUTPUT;
	if (channel->on_send)
		ret = channel->on_send(channel);
	return ret;
}

static int channel_accept(struct channel *channel)
{
	socklen_t len;
	struct channel *new = malloc(sizeof(struct channel));
	struct sockaddr *src;

	DB("Accepting channel");
	channel_init(new);
	new->src.af = new->af = channel->af;
	src = psockaddr_saddr(&new->src);
	len = psockaddr_len(&new->src);
	new->fd = accept(channel->fd, src, &len);

	if (new->fd < 0) {
		perror("accept()");
		return -1;
	}

	addrstr(&new->src);

	DB("New fd is %d, connected address is %s", new->fd,
	   psockaddr_string(&new->src));
	channel->flags &= ~CHAN_ACCEPT;
	new->flags = (channel->flags & CHAN_PERSIST);
	strncpy(new->tag, channel->tag, MAX_TAG);
	new->on_recv = tcp_recv;
	new->on_send = tcp_send;
	list_append(&channel->list, &new->list);
	pf[nfds].fd = new->fd;
	pf[nfds].events = EV_INPUT | EV_OUTPUT;
	new->pf = &pf[nfds];
	new->index = nfds;
	channel_of_pf[nfds] = new;
	nfds++;
	return 0;
}

/* Remove the given pf, and move the last pf to the now empty slot */
static void remove_pf(int index)
{
	int last = nfds - 1;
	struct channel *channel;
	if (index != last) {
		pf[index].fd = pf[last].fd;
		pf[index].events = pf[last].events;
		pf[index].revents = pf[last].revents;
		channel_of_pf[index] = channel_of_pf[last];
		channel = channel_of_pf[index];
		channel->index = index;
	}
	nfds--;
	return;
}

static void channel_free(struct channel *channel)
{
	pbuffer_free(channel->recv_buffer);
	pbuffer_free(channel->send_buffer);
	free(channel);
}

static int channel_close(struct channel *channel)
{
	int ret;
	DB("Closing channel");
	if (channel->on_close)
		ret = channel->on_close(channel);
	close(channel->fd);
	list_unlink(&channel->list);
	remove_pf(channel->index);
	channel_free(channel);
	return ret;
}

static struct channel *new_listener(struct channel *deque, char *ip,
			     uint16_t port, int mode)
{
	int new_sock = 0;
	int f_opt;
	int ret;

	struct channel *channel = malloc(sizeof(struct channel));
	channel_init(channel);

	set_ip(channel, ip);
	set_port(channel, port);

	if ((new_sock = socket(channel->af, mode, 0)) == -1) {
		perror("socket()");
		return NULL;
	}

	f_opt = fcntl(new_sock, F_GETFL, 0);
	f_opt |= O_NONBLOCK;
	if (fcntl(new_sock, F_SETFL, f_opt)) {
		perror("fcntl()");
		return NULL;
	}

	if (channel->af == AF_INET6) {
		ret = bind(new_sock, (struct sockaddr *)&channel->v6,
			   sizeof(channel->v6));
	} else {
		ret = bind(new_sock, (struct sockaddr *)&channel->v4,
			   sizeof(channel->v4));
	}

	if (ret < 0) {
		perror("bind()");
		return NULL;
	}

	list_append(&deque->list, &channel->list);
	channel->fd = new_sock;
	channel->flags = 0;
	pf[nfds].fd = channel->fd;
	pf[nfds].events = EV_INPUT | EV_OUTPUT;
	pf[nfds].revents = 0;
	channel_of_pf[nfds] = channel;
	channel->pf = &pf[nfds];
	channel->index = nfds;
	nfds++;
	return channel;
}

struct channel *new_udp_listener(struct channel *deque, char *ip, uint16_t port)
{
	struct channel *channel;
	if (!(channel = new_listener(deque, ip, port, SOCK_DGRAM)))
		return NULL;

	channel->protocol = PROTO_UDP;
	channel->accept = 0;
	channel->on_recv = udp_recv;
	return channel;
}

struct channel *new_tcp_listener(struct channel *deque, char *ip, uint16_t port)
{
	struct channel *channel;

	if (!(channel = new_listener(deque, ip, port, SOCK_STREAM)))
		return NULL;

	if (listen(channel->fd, 0) < 0) {
		perror("listen()");
		return NULL;
	}

	channel->protocol = PROTO_TCP;
	channel->accept = 1;
	return channel;
}

struct channel *new_connecter(struct channel *deque, char *ip, uint16_t port,
			      int mode)
{
	struct channel *channel = malloc(sizeof(struct channel));
	int ret = 0;
	int proto;

	channel_init(channel);
	set_ip(channel, ip);
	set_port(channel, port);

	if (mode == PROTO_TCP) {
		proto = SOCK_STREAM;
		channel->on_recv = tcp_recv;
		channel->on_send = tcp_send;
	} else {
		proto = SOCK_DGRAM;
		channel->on_recv = udp_recv;
	}

	if ((ret = socket(channel->af, proto, 0)) == -1) {
		perror("socket()");
		return NULL;
	}
	channel->fd = ret;
	if (channel->af == AF_INET6) {
		ret = connect(channel->fd,
			      (struct sockaddr *)&channel->v6,
			      sizeof(struct sockaddr));
	} else {
		ret = connect(channel->fd,
			      (struct sockaddr *)&channel->v4,
			      sizeof(struct sockaddr));
	}

	if (ret < 0) {
		perror("connect()");
		return NULL;
	}

	list_append(&deque->list, &channel->list);
	channel->flags = 0;
	pf[nfds].fd = channel->fd;
	pf[nfds].events = EV_INPUT | EV_OUTPUT;
	pf[nfds].revents = 0;
	channel_of_pf[nfds] = channel;
	channel->pf = &pf[nfds];
	channel->index = nfds;
	nfds++;
	return channel;
}

/* Dispatch the ready queue and put channels back on the dequeue */
int dispatch(struct channel *ready, struct channel *deque)
{
	struct channel *channel;

	while ((channel = channel_of(ready->list.next)) != ready) {

		/* placing channel on deque */
		list_unlink(&channel->list);
		list_append(&deque->list, &channel->list);

		DB("fd%d flags: %02x", channel->fd, channel->flags);

		if (channel->flags & CHAN_CLOSE) {
			channel_close(channel);
			continue;
		}

		if (channel->flags & CHAN_ACCEPT)
			channel_accept(channel);
		if (channel->flags & CHAN_SEND)
			channel_send(channel);
		if (channel->flags & CHAN_RECV)
			channel_recv(channel);
	}
	return 0;
}

/* Poll for events and place the channels with events on the ready queue */
int poll_events(struct channel *deque, struct channel *ready)
{
	int i;
	int ret;
	struct channel *channel;

	if(!pf[0].fd || nfds <= 0)
		return 0;

	ret = poll(pf, nfds, 1000);

	if (ret <= 0) {
		idle++;
		DB("idle %d", idle);
		return 0;
	}

	for (i = 0; i < nfds; i++) {
		idle = 0;
		if (!pf[i].revents)
			continue;

		DB("events for %d (fd %d): %d", i, pf[i].fd, pf[i].revents);

		if ((channel = channel_of_pf[i]) == NULL) {
			DBWARN("Cannot find channel for fd %d", pf[i].fd);
			continue;
		}

		DB("Found channel %p with fd: %d", channel, channel->fd);
		list_unlink(&channel->list);
		list_append(&ready->list, &channel->list);

		if (pf[i].revents & EV_HUP) {
			channel->flags |= CHAN_CLOSE;
			continue;
		}
		if (pf[i].revents & EV_INPUT)
			channel->flags |= (channel->accept ? CHAN_ACCEPT : CHAN_RECV);
		if (pf[i].revents & EV_OUTPUT)
			channel->flags |= CHAN_SEND;
	}

	return ret;
}

void channel_init(struct channel *channel)
{
	bzero(&channel->v6, sizeof(struct sockaddr_in6));
	list_init(&channel->list);
	channel->recv_buffer = pbuffer_init();
	channel->send_buffer = pbuffer_init();
}
