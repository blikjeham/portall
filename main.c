#include <stdlib.h>
#include "channels.h"


int main(int argc, char **argv)
{
	int ret;
	extern unsigned int idle;
	extern int nfds;
	struct channel *deque = malloc(sizeof(struct channel));
	channel_init(deque);

	idle = 0;
	nfds = 0;

	if ((ret = new_tcp_listener(deque, "127.0.0.1", 7000)) <= 0)
		return 1;

	while(poll_events(deque) >= 0) {
		/* do nothing yet */
		dispatch(deque);
	};
	return 0;
}
