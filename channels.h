#ifndef CHANNELS_H
#define CHANNELS_H

#include <netinet/in.h>
#include <poll.h>

#include "list.h"

#define MAX_CONN 100

#define PROTO_TCP 1
#define PROTO_UDP 2

#define CHAN_CLOSE 0x01
#define CHAN_ACCEPT 0x02
#define CHAN_RECV 0x04
#define CHAN_SEND 0x08
#define CHAN_ALL (CHAN_CLOSE|CHAN_ACCEPT|CHAN_RECV|CHAN_SEND)

#define EV_HUP (POLLHUP)
#define EV_INPUT (POLLIN)
#define EV_OUTPUT (POLLOUT)
#define EV_ALL (EV_HUP|EV_INPUT|EV_OUTPUT)

union uaddr {
	struct sockaddr_in v4;
	struct sockaddr_in6 v6;
};

struct channel {
	int fd;
	int af;
	short int protocol;
	int flags;
	int accept;

	union uaddr address;

	struct list list;

	char *name;

	struct pollfd *pf;
	/* callback */
	int (*on_accept)(struct channel *);
	int (*on_recv)(struct channel *);
	int (*on_send)(struct channel *);
	int (*on_close)(struct channel *);
};

#define channel_of(ptr) containerof(ptr, struct channel, list)

#define for_each_channel(deque, ptr) for (ptr = channel_of(deque->list.next); \
					  ptr != deque; \
					  ptr = channel_of(ptr->list.next))

int new_tcp_listener(struct channel *, char *, uint16_t );

int dispatch(struct channel *);
int poll_events(struct channel *);

void channel_init(struct channel *);

#endif /* CHANNELS_H */
