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
	struct buffer *buffer;

	if (size > MAX_SECTION_SIZE && ! pes_data) {
		dprintf("*** size (%d) > hard limit (%d)", size, size);
		return NULL;
	}

	buffer = (struct buffer *) malloc(sizeof(struct buffer));
	if (! buffer)
		return NULL;

	buffer->data = (char *) malloc(sizeof(char) * size);
	if (! buffer->data) {
		free(buffer);
		return NULL;
	}

	buffer->max_size = size;
	buffer->current_size = 0;
	buffer->holds_pes_data = pes_data;
	return buffer;
}

void buffer_destroy(struct buffer *buffer)
{
	if (buffer) {
		if (buffer->data)
			free(buffer->data);
		buffer->data = NULL;
		free(buffer);
	}
}

int buffer_append(struct buffer *buffer, const char *buf, size_t size)
{
	size_t to_write = 0;

	if (! buffer || ! buf)
		return -EINVAL;
	
	if (! size)
		return buffer->current_size;

	if (buffer->current_size == 0) {
		if (size > buffer->max_size) {
			/* Reusing the slot */
			free(buffer->data);
			buffer->data = (char *) malloc(sizeof(char) * size);
			if (! buffer->data) {
				buffer->max_size = 0;
				free(buffer->data);
				return -ENOMEM;
			}
			buffer->max_size = size;
		}
		to_write = size;
	} else {
		if ((buffer->current_size + size > MAX_SECTION_SIZE) && ! buffer->holds_pes_data)
			to_write = MAX_SECTION_SIZE - buffer->current_size;
		else
			to_write = size;
		if (buffer->current_size + to_write > buffer->max_size) {
			char *ptr = (char *) realloc(buffer->data, buffer->current_size + to_write);
			if (! ptr) {
				dprintf("Error reallocating memory");
				return -ENOMEM;
			}
			buffer->data = ptr;
			buffer->max_size = buffer->current_size + to_write;
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
		if ((section_length + 3) > MAX_SECTION_SIZE) {
			dprintf("Bad section packet: curr_size=%d max_size=%d section_length=%d",
					buffer->current_size, buffer->max_size, section_length);
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
