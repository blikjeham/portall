#include <stdlib.h>
#include "channels.h"
#include "conf.h"
#include "logging.h"

extern int loglevel;

int main(int argc, char **argv)
{
	extern unsigned int idle;
	extern int nfds;
	extern struct channel *deque;
	struct channel *ready;

	loglevel = 0;

	deque = malloc(sizeof(struct channel));
	channel_init(deque);
	ready = malloc(sizeof(struct channel));
	channel_init(ready);

	idle = 0;
	nfds = 0;

	if ((get_config_files(argc, argv)) < 0) {
		return 1;
	}

	if (create_sockets() < 0)
		return 2;

	while(poll_events(deque, ready) >= 0) {
		/* do nothing yet */
		dispatch(ready, deque);
	};
	return 0;
}
