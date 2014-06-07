#ifndef FORWARD_H
#define FORWARD_H

#include "pbuffer.h"
#include "conf.h"
#include "channels.h"

struct forward_header {
	char tag[MAX_TAG];
	int protocol;
	struct psockaddr src;
	struct psockaddr dst;
	pbuffer *payload;
};

void forward_message(struct channel *);

#endif /* FORWARD_H */
