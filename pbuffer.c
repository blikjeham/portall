/*
 * libpbuffer - dynamic buffer handling in C
 * Copyright (c) 2011 Wybe van der Ham
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdarg.h>
#include <arpa/inet.h>

#include "pbuffer.h"
#include "logging.h"

static size_t pbuffer_grow(pbuffer *buffer, size_t size)
{
	if (size < pbuffer_unused(buffer)) {
		return(buffer->allocated);
	}
	size_t newsize = (buffer->allocated*2) | PBUFFER_MIN;
	size_t offset = buffer->data - buffer->start;

	buffer->start = realloc(buffer->start, newsize);

	if (buffer->start == NULL) {
		printf("error reallocating memory.\n");
		return(0);
	}

	buffer->data = buffer->start + offset;

	buffer->allocated = newsize;

	/* Clear fresh memory */
	memset(pbuffer_end(buffer), 0, pbuffer_unused(buffer));
	return(newsize);
}

pbuffer *pbuffer_init(void)
{
	pbuffer *newbuffer;
	newbuffer = malloc(sizeof(pbuffer));
	newbuffer->length = 0;

	newbuffer->data = malloc(PBUFFER_MIN);
	newbuffer->allocated = PBUFFER_MIN;
	newbuffer->start = newbuffer->data;

	memset(newbuffer->data, 0, newbuffer->allocated);
	return(newbuffer);
}

void pbuffer_set(pbuffer *buffer, void *data, size_t size)
{
	pbuffer_assure(buffer, size);
	memcpy(buffer->data, data, size);
	buffer->length = size;
}

void pbuffer_sprintf(pbuffer *buffer, char *fmt, ...)
{
	int len;
	int needed;
	va_list va;
	len = buffer->allocated;
	va_start(va, fmt);

	while ((needed = vsnprintf(buffer->data, len, fmt, va)) > len)
		pbuffer_assure(buffer, needed);

	buffer->length += needed;

	va_end(va);
}

void pbuffer_add_sprintf(pbuffer *buffer, char *fmt, ...)
{
	int len;
	int needed;
	va_list va;
	len = pbuffer_unused(buffer);
	va_start(va, fmt);

	while ((needed = vsnprintf(pbuffer_end(buffer), len, fmt, va)) > len)
		pbuffer_assure(buffer, (buffer->length + needed));

	buffer->length += needed;

	va_end(va);
}

int pbuffer_strcpy(pbuffer *buffer, char *data)
{
	size_t size;

	size = strlen(data);
	pbuffer_assure(buffer, size);
	strncpy(buffer->data, data, size);
	memset(buffer->data+size, 0, buffer->allocated-size);
	buffer->length = size;
	return(0);
}

void pbuffer_add(pbuffer *buffer, void *data, size_t size)
{
	pbuffer_assure(buffer, size + buffer->length);
	memcpy(pbuffer_end(buffer), data, size);
	buffer->length += size;
}

void pbuffer_add_uint(pbuffer *buffer, unsigned int num)
{
	unsigned int tmp = htonl(num);
	pbuffer_add(buffer, &tmp, sizeof(unsigned int));
}

int pbuffer_strcat(pbuffer *buffer, char *data)
{
	size_t size;
	size = buffer->length;

	pbuffer_assure(buffer, size + strlen(data));
	strncpy((buffer->data + (size)), data, strlen(data));
	buffer->length = size + strlen(data);
	return(buffer->length);
}

void pbuffer_shift(pbuffer *buffer, size_t size)
{
	if (size < 0)
		return;
	pbuffer_assure(buffer, size);
	buffer->data = (buffer->data + size);
	buffer->length = (buffer->length - size);

	/* see if we can move the data */
	if (buffer->length < (buffer->data - buffer->start)) {
		memmove(buffer->start, buffer->data, buffer->length);
	}
}

void pbuffer_extract(pbuffer *buffer, void *dest, size_t len)
{
	if (len > buffer->length)
		return;

	memcpy(dest, buffer->data, len);
	pbuffer_shift(buffer, len);
}

int pbuffer_assure(pbuffer *buffer, size_t size)
{
	int ret = 0;

	if (pbuffer_unused(buffer) < size) {
		ret = pbuffer_grow(buffer, size);
	}
	if (ret < size) {
		ret = pbuffer_grow(buffer, (size * 2) | PBUFFER_MIN);
	}
	return(0);
}

int pbuffer_copy(pbuffer *dst, pbuffer *src, size_t len)
{
	pbuffer_add(dst, src->data, len);
	return 0;
}

void pbuffer_free(pbuffer *buffer)
{
	if (buffer) {
		free(buffer);
	}
}
