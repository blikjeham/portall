#ifndef FORWARD_H
#define FORWARD_H

#include "conf.h"
#include "channels.h"

struct forward_header {
	char tag[MAX_TAG];
	int protocol;
	struct psockaddr src;
	struct psockaddr dst;
};

void forward_message(struct channel *);

#endif /* FORWARD_H */
