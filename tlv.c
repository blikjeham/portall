#include <string.h>
#include "pbuffer.h"
#include "tlv.h"
#include "logging.h"

#define DB(fmt, args...) debug(3, "[tlv]: " fmt, ##args)
#define DBERR(fmt, args...) debug(0, "[tlv]: " fmt, ##args)

static unsigned int extract_num(pbuffer *b, size_t len)
{
	unsigned char holder;
	unsigned int number;
	if (b->length < len) {
		return 0;
	}
	pbuffer_extract(b, &holder, len);
	number = (unsigned int)holder;
	return number;
}

static unsigned int extract_uint(pbuffer *b)
{
	unsigned int number;
	if (b->length < sizeof(unsigned int))
		return 0;

	pbuffer_extract(b, &number, sizeof(unsigned int));
	return ntohl(number);
}

static void extract_ip(struct psockaddr *psa, pbuffer *b, size_t len)
{
	if (len == 4) {
	} else if (len == 16) {
	} else {
		DBERR("Cannot convert IP address (length != 4/16)");
	}
}

static void tlv_to_psockaddr(pbuffer *b, struct psockaddr *psa)
{
	return;
}

void tlv_parse_tags(pbuffer *b, struct forward_header *fh)
{
	struct tlv *tlv = tlv_init();
	while (b->length) {
		buffer_to_tlv(b, tlv);
		switch (tlv->type) {
		case T_TAG:
			if (tlv->value->length > MAX_TAG)
				return;
			strncpy(fh->tag, tlv->value->data, MAX_TAG);
			DB("Found tag: %s", fh->tag);
			break;
		case T_PROTOCOL:
			fh->protocol = extract_num(tlv->value, 1);
			break;
		case T_SRC:
			tlv_to_psockaddr(tlv->value, &fh->src);
			break;
		case T_DST:
			tlv_to_psockaddr(tlv->value, &fh->dst);
			break;
		case T_PAYLOAD:
			/* found payload */
			break;
		}
	}
}

void tlv_generate_tags(struct forward_header *fh, pbuffer *b)
{
	struct tlv *tlv = tlv_init();

	if (fh->tag) {
		tlv->type = T_TAG;
		tlv->length = strlen(fh->tag);
		pbuffer_add(tlv->value, fh->tag, tlv->length);
		tlv_to_buffer(tlv, b);
		tlv_clear(tlv);
		hexdump(3, b->data, b->length);
	}

	if (fh->protocol) {
		tlv->type = T_PROTOCOL;
		tlv->length = 1;
		pbuffer_add(tlv->value, &fh->protocol, 1);
		tlv_to_buffer(tlv, b);
		tlv_clear(tlv);
		hexdump(3, b->data, b->length);
	}

	if (fh->payload->length) {
		hexdump(3, b->data, b->length);
		tlv->type = T_PAYLOAD;
		tlv->length = fh->payload->length;
		pbuffer_copy(tlv->value, fh->payload, tlv->length);
		tlv_to_buffer(tlv, b);
		tlv_clear(tlv);
		hexdump(3, b->data, b->length);
	}
	tlv_free(tlv);
}

static unsigned int count_shift(unsigned int num)
{
	unsigned int count = 0;
	while (num > (TLV_EXTEND - 1)) {
		count++;
		num = num >> 7;
	}
	return count;
}

int tlv_to_buffer(struct tlv *tlv, pbuffer *buffer)
{
	char holder = 0;
	unsigned int origlen = tlv->length;
	unsigned int t_shift, l_shift;

	t_shift = count_shift(tlv->type);
	l_shift = count_shift(tlv->length);
	/* set type */
	while (t_shift > 0) {
		holder |= ((tlv->type >> (7*t_shift)) | TLV_EXTEND);
		pbuffer_add(buffer, &holder, 1);
		holder = 0;
		t_shift--;
	}
	holder |= tlv->type & (TLV_EXTEND - 1);
	pbuffer_add(buffer, &holder, 1);

	/* set length */
	while (l_shift > 0) {
		holder |= ((tlv->length >> (7*l_shift)) | TLV_EXTEND);
		pbuffer_add(buffer, &holder, 1);
		holder = 0;
		l_shift--;
	}
	holder = tlv->length;
	pbuffer_add(buffer, &holder, 1);

	pbuffer_add(buffer, tlv->value->data, origlen);
	return 0;
}

void buffer_to_tlv(pbuffer *buffer, struct tlv *tlv)
{
	unsigned int tmp = 0;
	unsigned int holder = 0;

	while ((holder = extract_num(buffer, 1))) {
		if (holder & TLV_EXTEND) {
			holder &= ~TLV_EXTEND;
			tmp |= holder;
			tmp = tmp << 7;
		} else {
			tmp |= holder;
			break;
		}
	}
	tlv->type = tmp;

	tmp = 0;
	while((holder = extract_num(buffer, 1))) {
		if (holder & TLV_EXTEND) {
			holder &= ~TLV_EXTEND;
			tmp |= holder;
			tmp = tmp << 7;
		} else {
			tmp |= holder;
			break;
		}
	}
	tlv->length = tmp;

	tlv->value = buffer;
}

struct tlv *tlv_init(void)
{
	struct tlv *tmp;
	tmp = malloc(sizeof(struct tlv));
	tmp->value = pbuffer_init();
	return tmp;
}

void tlv_free(struct tlv *tlv)
{
	pbuffer_free(tlv->value);
	free(tlv);
}
