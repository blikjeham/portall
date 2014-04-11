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
		case T_PROTO:
			DB("Found protocol");
			break;
		}
	}
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
