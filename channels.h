#ifndef CHANNELS_H
#define CHANNELS_H

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>

#include "pbuffer.h"
#include "list.h"
#include "conf.h"

#define MAX_CONN 100

#define PROTO_TCP 1
#define PROTO_UDP 2

#define CHAN_CLOSE 0x01
#define CHAN_ACCEPT 0x02
#define CHAN_RECV 0x04
#define CHAN_SEND 0x08
#define CHAN_ALL (CHAN_CLOSE|CHAN_ACCEPT|CHAN_RECV|CHAN_SEND)
#define CHAN_TAGGED 0x10
#define CHAN_PERSIST (CHAN_TAGGED)

#define EV_HUP (POLLHUP)
#define EV_INPUT (POLLIN)
#define EV_OUTPUT (POLLOUT)
#define EV_ALL (EV_HUP|EV_INPUT|EV_OUTPUT)

union uaddr {
	struct sockaddr_in v4;
	struct sockaddr_in6 v6;
};

struct psockaddr {
	int af;
	socklen_t length;
	union {
		struct sockaddr_in v4;
		struct sockaddr_in6 v6;
	};
	char addrstr[INET6_ADDRSTRLEN];
};

struct channel {
	int fd;
	int af;
	short int protocol;
	int flags;
	int accept;
	int index;
	char tag[MAX_TAG];

	union {
		struct sockaddr_in v4;
		struct sockaddr_in6 v6;
	};
	struct psockaddr src;

	struct list list;

	char *name;

	struct pollfd *pf;
	/* callback */
	int (*on_accept)(struct channel *);
	int (*on_recv)(struct channel *);
	int (*on_send)(struct channel *);
	int (*on_close)(struct channel *);

	pbuffer *recv_buffer;
	pbuffer *send_buffer;
};

#define channel_of(ptr) containerof(ptr, struct channel, list)

#define for_each_channel(deque, ptr) for (ptr = channel_of(deque->list.next); \
					  ptr != deque; \
					  ptr = channel_of(ptr->list.next))

static inline char *psockaddr_string(struct psockaddr *psock)
{
	char *tmp = malloc(INET6_ADDRSTRLEN + 6);
	snprintf(tmp, INET6_ADDRSTRLEN + 6, "%s:%d", psock->addrstr,
		 psock->v6.sin6_port);
	return tmp;
}

static inline struct sockaddr *psockaddr_saddr(struct psockaddr *psock)
{
	if (psock->af == AF_INET6)
		return (struct sockaddr *)&psock->v6;
	else
		return (struct sockaddr *)&psock->v4;
}

static inline socklen_t psockaddr_len(struct psockaddr *psock)
{
	if (psock->af == AF_INET6)
		return sizeof(psock->v6);
	else
		return sizeof(psock->v4);
}

struct channel *new_udp_listener(struct channel *, char *, uint16_t );
struct channel *new_tcp_listener(struct channel *, char *, uint16_t );
struct channel *new_connecter(struct channel *, char *, uint16_t , int );
struct channel *connecter(struct channel *, char *, uint16_t );

char *addrstr(struct psockaddr *);
int dispatch(struct channel *, struct channel *);
int poll_events(struct channel *, struct channel *);

void channel_init(struct channel *);

#endif /* CHANNELS_H */
