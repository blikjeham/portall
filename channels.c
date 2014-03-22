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

struct pollfd pf[MAX_CONN];
struct channel *channel_of_pf[MAX_CONN];
struct channel *deque;
uint nfds;
unsigned int idle;

static void set_ip(struct channel *channel, char *ip)
{
	if ((strstr(ip, ":")) != NULL) {
		channel->af = AF_INET6;
		channel->address.v6.sin6_family = channel->af;
		inet_pton(channel->af, ip, &(channel->address.v6.sin6_addr));
	} else {
		channel->af = AF_INET;
		channel->address.v4.sin_family = channel->af;
		inet_pton(channel->af, ip, &(channel->address.v4.sin_addr));
	}
}

static void set_port(struct channel *channel, uint16_t port)
{
	if (channel->af == AF_INET6)
		channel->address.v6.sin6_port = htons(port);
	else
		channel->address.v4.sin_port = htons(port);
}

static int channel_accept(struct channel *channel)
{
	socklen_t len;
	struct channel *new = malloc(sizeof(struct channel));

	channel_init(new);
	if(channel->af == AF_INET6) {
		len = sizeof(struct sockaddr_in6);
		new->fd = accept(channel->fd, (struct sockaddr *)&new->address.v6,
				 &len);
	} else {
		len = sizeof(struct sockaddr_in);
		new->fd = accept(channel->fd, (struct sockaddr *)&new->address.v4,
				 &len);
	}
	if (new->fd < 0) {
		perror("accept()");
		return -1;
	}

	channel->flags &= ~CHAN_ACCEPT;
	new->flags = 0;
	list_append(&channel->list, &new->list);
	pf[nfds].fd = new->fd;
	pf[nfds].events = EV_INPUT | EV_OUTPUT;
	new->pf = &pf[nfds];
	new->index = nfds;
	channel_of_pf[nfds] = new;
	nfds++;
	return 0;
}

static int channel_recv(struct channel *channel)
{
	size_t bytes = 0;
	char buffer[256];
	bytes = recv(channel->fd, buffer, 256, 0);
	printf("received %zu bytes\n", bytes);
	channel->flags &= ~CHAN_RECV;
	channel->pf->events = 0;
	return bytes;
}

static int channel_send(struct channel *channel)
{
	channel->flags &= ~CHAN_SEND;
	channel->pf->events = 0;
	return 0;
}

static struct channel *channel_close(struct channel *channel)
{
	/* should properly close the channel */
	return channel_of(channel->list.next);
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
		ret = bind(new_sock, (struct sockaddr *)&channel->address.v6,
			   sizeof(channel->address.v6));
	} else {
		ret = bind(new_sock, (struct sockaddr *)&channel->address.v4,
			   sizeof(channel->address.v4));
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
	int proto = (mode == PROTO_TCP) ? SOCK_STREAM : SOCK_DGRAM;

	channel_init(channel);
	set_ip(channel, ip);
	set_port(channel, port);

	if ((ret = socket(channel->af, proto, 0)) == -1) {
		perror("socket()");
		return NULL;
	}
	channel->fd = ret;
	if (channel->af == AF_INET6) {
		ret = connect(channel->fd,
			      (struct sockaddr *)&channel->address.v6,
			      sizeof(struct sockaddr));
	} else {
		ret = connect(channel->fd,
			      (struct sockaddr *)&channel->address.v4,
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

int dispatch(struct channel *deque)
{
	struct channel *channel;

	for_each_channel(deque, channel) {
		printf("fd: %d flags: %02x\n", channel->fd, channel->flags);
		if (channel->flags & CHAN_CLOSE) {
			channel = channel_close(channel);
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

int poll_events(struct channel *deque)
{
	int i;
	int ret;
	struct channel *channel;

	if(!pf[0].fd || nfds <= 0)
		return 0;

	ret = poll(pf, nfds, 1000);

	if (ret <= 0) {
		idle++;
		printf("idle %d\n", idle);
		return 0;
	}

	for (i = 0; i < nfds; i++) {
		idle = 0;
		if (!pf[i].revents)
			continue;

		if ((channel = channel_of_pf[i]) == NULL) {
			printf("Cannot find channel for fd %d\n", pf[i].fd);
			continue;
		}

		if (pf[i].revents & EV_HUP) {
			channel->flags |= CHAN_CLOSE;
			continue;
		}
		if (pf[i].revents & EV_INPUT)
			channel->flags |= channel->accept ? CHAN_ACCEPT : CHAN_RECV;
		if (pf[i].revents & EV_OUTPUT)
			channel->flags |= CHAN_SEND;
	}

	return ret;
}

void channel_init(struct channel *channel)
{
	bzero(&channel->address.v6, sizeof(struct sockaddr_in6));
	list_init(&channel->list);
}
