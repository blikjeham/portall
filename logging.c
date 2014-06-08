#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include "logging.h"
#include "tlv.h"

int loglevel;

void debug(int level, const char *fmt, ...)
{
	if (loglevel >= level) {
		va_list va;
		struct timeval tv;
		char buf[256];

		gettimeofday(&tv, NULL);
		strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&tv.tv_sec));

		va_start(va, fmt);
		fprintf(stderr, "[%s.%-6ld] ", buf, tv.tv_usec);
		vfprintf(stderr, fmt, va);
		fprintf(stderr, "\n");
		va_end(va);
	}
}

static void print_hexline(const unsigned char *payload, int len, int offset)
{
	int i;
	int gap;
	const unsigned char *ch;

	fprintf(stderr, "%08x   ", offset);
	ch = payload;
	for (i=0; i<len; i++) {
		fprintf(stderr, "%02x ", *ch);
		ch++;
		if (i == 7)
			fprintf(stderr, " ");
	}

	if (len < 8)
		fprintf(stderr, " ");
	if (len < 16) {
		gap = 16 - len;
		for (i = 0; i < gap; i++) {
			fprintf(stderr, "   ");
		}
	}
	fprintf(stderr, "   ");
	ch = payload;
	for (i=0; i<len; i++) {
		if (isprint(*ch))
			fprintf(stderr, "%c", *ch);
		else
			fprintf(stderr, ".");
		ch++;
		if (i == 7)
			fprintf(stderr, " ");
	}
	fprintf(stderr, "\n");
	return;
}

void hexdump(int level, const unsigned char *payload, size_t len)
{
	if (loglevel < level)
		return;

	size_t len_rem = len;
	size_t line_width = 16;
	size_t line_len;
	int offset = 0;

	const unsigned char *ch = payload;

	if (len <= 0)
		return;

	if (len <= line_width) {
		print_hexline(ch, len, offset);
		return;
	}

	for ( ;; ) {
		line_len = len_rem;
		if (len_rem >= line_width)
			line_len = line_width;

		print_hexline(ch, line_len, offset);
		len_rem = len_rem - line_len;
		ch = ch + line_len;
		offset = offset + line_width;
		if (len_rem == 0) {
			break;
		}
	}
	return;
}

void decode_tlv(struct tlv *tlv)
{
	debug(3, "Type: %s (%d)", T_NAMES[tlv->type], tlv->type);
	debug(3, "Length: %d", tlv->length);
	debug(3, "Value:");
	hexdump(3, tlv->value->data, tlv->length);
}

void decode_tlv_buffer(pbuffer *buffer, size_t len)
{
	struct tlv *tlv = tlv_init();
	size_t bytes;
	char *start = buffer->data;
	size_t length = buffer->length;
	while (len > 0) {
		bytes = extract_torv(buffer, &tlv->type);
		len -= bytes;
		bytes = extract_torv(buffer, &tlv->length);
		len -= bytes;
		pbuffer_set(tlv->value, buffer->data, tlv->length);
		decode_tlv(tlv);
		pbuffer_safe_shift(buffer, tlv->length);
		len -= tlv->length;
	}
	buffer->data = start;
	buffer->length = length;
}
