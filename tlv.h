#ifndef TLV_H
#define TLV_H

#include "pbuffer.h"

#define TLV_EXTEND 0x80

struct tlv {
	unsigned int type;
	unsigned int length;
	pbuffer *value;
};

enum t_types {
	T_PAYLOAD = 1,
	T_SRC,
	T_DST,
	T_PROTO,
	T_SPORT = 200,
	T_DPORT,
};

int tlv_to_buffer(struct tlv *, pbuffer *);
void buffer_to_tlv(pbuffer *, struct tlv *);
struct tlv *tlv_init();
#endif /* TLV_H */
