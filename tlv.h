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
	T_COMMAND, /* CONSTRUCT of ct_types */
	T_NUM,
};

/* tlv types for psockaddr */
enum psock_types {
	PT_FAMILY = 1,
	PT_IPADDR,
	PT_PORT,
	PT_NUM,
};

/* tlv types for command */
enum ct_types {
	CT_KEEPALIVE = 1,
	CT_ALIVE,
	CT_NUM,
};

extern const char *T_NAMES[T_NUM];
extern const char *PT_NAMES[PT_NUM];
extern const char *CT_NAMES[CT_NUM];

static inline void tlv_clear(struct tlv *tlv)
{
	tlv->type = 0;
	tlv->length = 0;
	pbuffer_clear(tlv->value);
}
unsigned char extract_byte(pbuffer *);
unsigned int extract_su(pbuffer *, size_t );
char *extract_ip(struct psockaddr *, pbuffer *, size_t );
void tlv_parse_tags(pbuffer *, struct forward_header *);
void tlv_generate_tags(struct forward_header *, pbuffer *);
int tlv_to_buffer(struct tlv *, pbuffer *);
void buffer_to_tlv(pbuffer *, struct tlv *);
size_t extract_torv(pbuffer *, unsigned int *);

struct tlv *tlv_init();
void tlv_free(struct tlv *);
#endif /* TLV_H */
