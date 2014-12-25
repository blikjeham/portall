#include <stdlib.h>
#include "channels.h"
#include "conf.h"
#include "logging.h"

extern int loglevel;

int main(int argc, char **argv)
{
	extern struct timer *timers;
	loglevel = 0;

	channels_init();
	timers = timer_init();

	if ((get_config_files(argc, argv)) < 0) {
		return 1;
	}

	if (create_sockets() < 0)
		return 2;

	DISPATCHER;

	return 0;
}
