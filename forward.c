#include <string.h>

#include "forward.h"
#include "channels.h"
#include "tlv.h"
#include "conf.h"
#include "logging.h"

extern struct conf_tunnel *tunnel;

#define DB(fmt, args...) debug(3, "[fwrd]: " fmt, ##args)

struct channel *find_by_tag(char *tag)
{
	extern struct channel *deque;
	struct channel *channel;
	for_each_channel(deque, channel) {
		if (!strcmp(channel->tag, tag))
			return channel;
	}
	DB("Could not find channel with tag %s", tag);
	return NULL;
}

static struct channel *parse_tags(struct channel *channel)
{
	struct forward_header fh;
	tlv_parse_tags(channel->recv_buffer, &fh);

	if (fh.tag) {
		return find_by_tag(fh.tag);
	}
	DB("No tag found");
	return NULL;
}

/* generate tags, and return the tunnel */
static struct channel *generate_tags(struct channel *channel)
{
	pbuffer *b = channel->recv_buffer;
	struct tlv *tlv = tlv_init();

	struct channel *out = tunnel->channel;
	DB("Generating tags (%s)", channel->tag);

	tlv->type = T_TAG;
	tlv->length = strlen(channel->tag);
	pbuffer_add(tlv->value, channel->tag, tlv->length);
	tlv_to_buffer(tlv, out->send_buffer);
	pbuffer_clear(tlv->value);
	tlv->type = T_PAYLOAD;
	tlv->length = b->length;
	pbuffer_copy(tlv->value, b, b->length);
	pbuffer_shift(b, b->length);
	tlv_to_buffer(tlv, out->send_buffer);
	hexdump(3, out->send_buffer->data, out->send_buffer->length);
	return out;
}

void forward_message(struct channel *in)
{
	struct channel *out;

	DB("Start forwarding");
	if (in->flags & CHAN_TAGGED) {
		out = parse_tags(in);
	} else {
		out = generate_tags(in);
	}
	out->pf->events |= EV_OUTPUT;
}
