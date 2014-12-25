#include <string.h>

#include "forward.h"
#include "channels.h"
#include "tlv.h"
#include "conf.h"
#include "logging.h"

extern struct conf_tunnel *tunnel;

#define DB(fmt, args...) debug(3, "[fwrd]: " fmt, ##args)
#define DBERR(fmt, args...) debug(1, "[fwrd]: " fmt, ##args)

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
	struct channel *out = NULL;
	struct forward_header fh;
	pbuffer *b = channel->recv_buffer;
	decode_tlv_buffer(b, b->length);
	hexdump(3, (unsigned char *)b->data, b->length);
	tlv_parse_tags(b, &fh);

	if (fh.tag) {
		out = find_by_tag(fh.tag);
		if (fh.payload->length) {
			pbuffer_copy(out->send_buffer, fh.payload,
				     fh.payload->length);
		}
	} else {
		DBERR("The packet did not contain a tag; dropping");
	}
	return out;
}

/* generate tags, and return the tunnel */
static struct channel *generate_tags(struct channel *channel)
{
	struct forward_header fh;
	struct channel *out = tunnel->channel;

	DB("Generating tags (%s)", channel->tag);

	strncpy(fh.tag, channel->tag, MAX_TAG);
	fh.protocol = channel->protocol;
	fh.src = channel->src;
	fh.payload = channel->recv_buffer;
	tlv_generate_tags(&fh, out->send_buffer);
	hexdump(3, out->send_buffer->data, out->send_buffer->length);
	decode_tlv_buffer(out->send_buffer, out->send_buffer->length);
	pbuffer_clear(channel->recv_buffer);
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
	if (out) {
		queue_send(out);
	}
}
