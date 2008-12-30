/* 
 * Copyright (c) 2008, Lucas C. Villa Real <lucasvr@gobolinux.org>
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
#include "fsutils.h"
#include "xattr.h"
#include "ts.h"
#include "descriptors.h"

struct system_clock_descriptor {
	uint8_t external_clock_reference_indicator:1;
	uint8_t reserved_1:1;
	uint8_t clock_accuracy_integer:6;
	uint8_t clock_accuracy_exponent:3;
	uint8_t reserved_2:5;
};

/* SYSTEM_CLOCK_DESCRIPTOR parser */
int descriptor_0x0b_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	if (! descriptor_is_parseable(parent, 0x0b, 2, len))
		return -ENODATA;

	struct system_clock_descriptor s;
	s.external_clock_reference_indicator = (payload[0] >> 7) & 0x01;
	s.reserved_1 = (payload[0] >> 6) & 0x01;
	s.clock_accuracy_integer = payload[0] & 0x3f;
	s.clock_accuracy_exponent = (payload[1] >> 5) & 0x07;
	s.reserved_2 = payload[1] & 0x1f;

	struct dentry *subdir = CREATE_DIRECTORY(parent, "SYSTEM_CLOCK");
	CREATE_FILE_NUMBER(subdir, &s, external_clock_reference_indicator);
	//CREATE_FILE_NUMBER(subdir, &s, reserved_1);
	CREATE_FILE_NUMBER(subdir, &s, clock_accuracy_integer);
	CREATE_FILE_NUMBER(subdir, &s, clock_accuracy_exponent);
	//CREATE_FILE_NUMBER(subdir, &s, reserved_2);

    return 0;
}

