/* 
 * Copyright (c) 2008-2010, Lucas C. Villa Real <lucasvr@gobolinux.org>
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
#include "fsutils.h"
#include "xattr.h"
#include "ts.h"
#include "descriptors.h"

struct smoothing_buffer_descriptor {
	uint8_t reserved_1;
	uint32_t sb_leak_rate;
	uint8_t reserved_2;
	uint32_t sb_size;
};

/* SMOOTHING_BUFFER_DESCRIPTOR parser */
int descriptor_0x10_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct dentry *subdir = CREATE_DIRECTORY(parent, "SMOOTHING_BUFFER");

	if (! descriptor_is_parseable(parent, payload[0], 9, len))
		return -ENODATA;

	struct smoothing_buffer_descriptor s;
	s.reserved_1 = (payload[2] >> 6) & 0x03;
	s.sb_leak_rate = CONVERT_TO_24(payload[2], payload[3], payload[4]) & 0x003fffff;
	s.reserved_2 = (payload[5] >> 6) & 0x03;
	s.sb_size = CONVERT_TO_24(payload[5], payload[6], payload[7]) & 0x003fffff;

	//CREATE_FILE_NUMBER(subdir, &s, reserved_1);
	CREATE_FILE_NUMBER(subdir, &s, sb_leak_rate);
	//CREATE_FILE_NUMBER(subdir, &s, reserved_2);
	CREATE_FILE_NUMBER(subdir, &s, sb_size);

    return 0;
}

