#ifndef TIMERS_H
#define TIMERS_H

#include <time.h>
#include <sys/time.h>
#include "channels.h"
#include "list.h"

extern struct timer *timers;

struct timer {
	int armed;
	struct timeval tv;
	struct channel *channel;
	struct list list;
	int (*on_fire)(struct timer *, struct timeval *);
};

#define timer_of(ptr) containerof(ptr, struct timer, list)

#define for_each_timer(deque, ptr) for (ptr = timer_of(deque->list.next); \
					ptr != deque;			\
					ptr = timer_of(ptr->list.next))

int keep_alive(struct timer *, struct timeval *);
int timer_fire(struct timer *, struct timeval *);
void timer_arm(struct timer *, int ,
	       int (*)(struct timer *, struct timeval *));
int timer_check(void);
struct timer *timer_init(void);

#endif /* TIMERS_H */
