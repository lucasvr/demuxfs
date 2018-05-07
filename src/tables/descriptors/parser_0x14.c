/* 
 * Copyright (c) 2008-2018, Lucas C. Villa Real <lucasvr@gobolinux.org>
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

struct formatted_descriptor {
	uint16_t association_tag;
	uint16_t use;
	uint8_t selector_length;
	uint32_t transaction_id;
	uint32_t timeout;
	char selector_byte[256];
	char private_data_byte[256];
};

/* ASSOCIATION_TAG_DESCRIPTOR parser */
int descriptor_0x14_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	uint8_t i, offset;
	struct dentry *dentry;
	struct formatted_descriptor f;

	if (! descriptor_is_parseable(parent, payload[0], 5, len))
		return -ENODATA;

	dentry = CREATE_DIRECTORY(parent, "Association_Tag_Descriptor");

	f.association_tag = CONVERT_TO_16(payload[2], payload[3]);
	f.use = CONVERT_TO_16(payload[4], payload[5]);
	CREATE_FILE_NUMBER(dentry, &f, association_tag);
	CREATE_FILE_NUMBER(dentry, &f, use);

	if (f.use == 0x0000) {
		if (len < 13) {
			TS_WARNING("cannot parse descriptor %#x: contents smaller than 13 bytes (%d)", 0x14, len);
			return -1;
		}
		f.selector_length = payload[6];
		f.transaction_id = CONVERT_TO_32(payload[7], payload[8], payload[9], payload[10]);
		f.timeout = CONVERT_TO_32(payload[11], payload[12], payload[13], payload[14]);
		CREATE_FILE_NUMBER(dentry, &f, selector_length);
		CREATE_FILE_NUMBER(dentry, &f, transaction_id);
		CREATE_FILE_NUMBER(dentry, &f, timeout);
		offset = 15;
	} else if (f.use == 0x0001) {
		f.selector_length = payload[6];
		CREATE_FILE_NUMBER(dentry, &f, selector_length);
		offset = 7;
	} else {
		f.selector_length = payload[6];
		if (len < 5 + f.selector_length) {
			TS_WARNING("cannot parse descriptor %#x: contents smaller than %d bytes (%d)",
					0x14, 5 + f.selector_length, len);
			return -1;
		}
		for (i=0; i<f.selector_length; ++i)
			f.selector_byte[i] = payload[7+i];
		f.selector_byte[i] = '\0';
		CREATE_FILE_NUMBER(dentry, &f, selector_length);
		CREATE_FILE_STRING(dentry, &f, selector_byte, XATTR_FORMAT_STRING);
		offset = 6 + i;
	}

	memset(f.private_data_byte, 0, sizeof(f.private_data_byte));
	for (i=offset; i<len; ++i)
		f.private_data_byte[i-offset] = payload[i];
	f.private_data_byte[i-offset] = '\0';
	CREATE_FILE_STRING(dentry, &f, private_data_byte, XATTR_FORMAT_STRING);

    return 0;
}

