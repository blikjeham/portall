#include <time.h>
#include <sys/time.h>
#include "logging.h"
#include "timer.h"
#include "tlv.h"

struct timer *timers;

#define DB(fmt, args...) debug(3, "[timer]: " fmt, ##args)

int on_alive(struct timer *timer, struct timeval *now)
{
	return 0;
}

int keep_alive(struct timer *timer, struct timeval *now)
{
	struct channel *channel = timer->channel;
	struct tlv *tlv = tlv_init();
	DB("Sending keepalive");
	tlv->type = T_COMMAND;
	tlv->length = 1;
	pbuffer_add_byte(tlv->value, CT_KEEPALIVE);
	tlv_to_buffer(tlv, channel->send_buffer);
	queue_send(channel);
	timer_arm(timer, 5, keep_alive);
	return 0;
}

int timer_fire(struct timer *timer, struct timeval *now)
{
	if (!timer || !timer->armed)
		return 0;

	if (timer->tv.tv_sec > now->tv_sec)
		return 0;

	/* disarm timer and execute the callback */
	timer->armed = 0;
	if (timer->on_fire)
		return timer->on_fire(timer, now);

	return 0;
}

void timer_arm(struct timer *timer, int timeout,
	       int (*callback)(struct timer *, struct timeval *))
{
	if ((!timer) || (timeout < 0))
		return;

	gettimeofday(&timer->tv, NULL);
	timer->tv.tv_sec += timeout;
	timer->on_fire = callback;
	timer->armed = 1;
	if (!list_is_linked(&timer->list)) {
		list_append(&timers->list, &timer->list);
	}
}

int timer_check(void)
{
	int ret = 0;
	struct timeval now;
	struct timer *timer;

	gettimeofday(&now, NULL);
	for_each_timer(timers, timer) {
		timer_fire(timer, &now);
	}
	return ret;
}

struct timer *timer_init(void)
{
	struct timer *timer = malloc(sizeof(struct timer));
	timer->armed = 0;
	list_init(&timer->list);
	return timer;
}
