#ifndef TLV_H
#define TLV_H

#include "pbuffer.h"
#include "forward.h"

#define TLV_EXTEND 0x80

struct tlv {
	unsigned int type;
	unsigned int length;
	pbuffer *value;
};

/* main tlv types */
enum t_types {
	T_TAG = 1,
	T_PROTOCOL,
	T_SRC, /* CONSTRUCT of psock_types */
	T_DST, /* CONSTRUCT of psock_types */
	T_PAYLOAD,
};

/* tlv types for psockaddr */
enum psock_types {
	PT_FAMILY = 1,
	PT_IPADDR,
	PT_PORT,
};

static inline void tlv_clear(struct tlv *tlv)
{
	tlv->type = 0;
	tlv->length = 0;
	pbuffer_clear(tlv->value);
}

void tlv_parse_tags(pbuffer *, struct forward_header *);
void tlv_generate_tags(struct forward_header *, pbuffer *);
int tlv_to_buffer(struct tlv *, pbuffer *);
void buffer_to_tlv(pbuffer *, struct tlv *);
struct tlv *tlv_init();
void tlv_free(struct tlv *);
#endif /* TLV_H */
