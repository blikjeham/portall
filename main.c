#include <stdlib.h>
#include "channels.h"
#include "conf.h"

int main(int argc, char **argv)
{
	struct channel *ret;
	extern unsigned int idle;
	extern int nfds;
	extern struct channel *deque;
	deque = malloc(sizeof(struct channel));
	channel_init(deque);

	idle = 0;
	nfds = 0;

	if ((get_config_files(argc, argv)) < 0) {
		return 1;
	}

	if (create_sockets() < 0)
		return 2;

	while(poll_events(deque) >= 0) {
		/* do nothing yet */
		dispatch(deque);
	};
	return 0;
}
