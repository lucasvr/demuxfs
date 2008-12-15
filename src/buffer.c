/* 
 * Copyright (c) 2008, Lucas C. Villa Real <lucasvr@gobolinux.org>
 * Copyright (c) 2008, Iuri Gomes Diniz <iuridiniz@gmail.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of GoboLinux nor the names of its contributors may
 * be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "demuxfs.h"
#include "byteops.h"
#include "buffer.h"

struct buffer *buffer_create(size_t size, bool pes_data)
{
	size_t max_size = BUFFER_MAX_SIZE;
	struct buffer *buffer;
	
	if (pes_data)
		max_size = 0xffff + 0x100;

	if (size > max_size) {
		dprintf("*** size (%d) > hard limit (%d)", size, max_size);
		return NULL;
	}

	buffer = (struct buffer *) malloc(sizeof(struct buffer));
	if (! buffer)
		return NULL;

	buffer->data = (char *) malloc(sizeof(char) * max_size);
	if (! buffer->data) {
		free(buffer);
		return NULL;
	}

	buffer->max_size = max_size;
	buffer->current_size = 0;
	buffer->holds_pes_data = pes_data;
	return buffer;
}

void buffer_destroy(struct buffer *buffer)
{
	if (buffer) {
		if (buffer->data)
			free(buffer->data);
		free(buffer);
	}
}

int buffer_append(struct buffer *buffer, const char *buf, size_t size)
{
	size_t to_write = size;

	if (! buffer || ! buf)
		return -EINVAL;
	
	if (! size)
		return buffer->current_size;

	if (buffer->current_size == 0) {
		if (size > buffer->max_size) {
			dprintf("*** size (%d) > hard limit (%d)", size, buffer->max_size);
			return 0;
		}
		to_write = size;
	} else if ((buffer->current_size + size) > buffer->max_size) {
		if (! buffer->holds_pes_data)
			to_write = buffer->max_size - buffer->current_size;
		else {
			char *ptr = (char *) realloc(buffer->data, buffer->current_size + size);
			if (! ptr)
				/* Couldn't realloc, so truncate data instead */
				to_write = buffer->max_size - buffer->current_size;
			else
				buffer->data = ptr;
		}
	}

	memcpy(&buffer->data[buffer->current_size], buf, to_write);
	buffer->current_size += to_write;

	return buffer->current_size;
}

bool buffer_contains_full_psi_section(struct buffer *buffer)
{
	uint16_t section_length;

	if (! buffer || ! buffer->data || buffer->current_size < 4)
		return false;

	section_length = CONVERT_TO_16(buffer->data[1], buffer->data[2]) & 0x0fff;
	if (buffer->current_size < (section_length + 3)) {
		if ((section_length + 3) > buffer->max_size) {
			dprintf("Bad section packet");
			buffer->current_size = 0;
		}
		return false;
	}
	buffer->current_size = section_length + 3;
	return true;
}

bool buffer_contains_full_pes_section(struct buffer *buffer)
{
	uint16_t section_length;

	if (! buffer || ! buffer->data || buffer->current_size < 6)
		return false;

	section_length = CONVERT_TO_16(buffer->data[4], buffer->data[5]);
	if (buffer->current_size < (section_length + 6 - 1))
		return false;

	buffer->current_size = section_length + 6;
	return true;
}

void buffer_reset_size(struct buffer *buffer)
{
	if (buffer)
		buffer->current_size = 0;
}
