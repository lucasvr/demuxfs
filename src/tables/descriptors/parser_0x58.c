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
#include "fsutils.h"
#include "byteops.h"
#include "xattr.h"
#include "ts.h"
#include "descriptors.h"

struct formatted_descriptor {
	char     country_code[16];
	uint8_t  country_region_id:6;
	uint8_t  reserved:1;
	uint8_t  local_time_offset_polarity:1;
	uint16_t local_time_offset;
	uint64_t time_of_change;
	uint16_t next_time_offset;
};

/* LOCAL_TIME_OFFSET_DESCRIPTOR parser */
int descriptor_0x58_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct formatted_descriptor f;

	if (! descriptor_is_parseable(parent, payload[0], 15, len))
		return -ENODATA;

	memset(&f, 0, sizeof(f));
	sprintf(f.country_code, "%c%c%c [0x%x%x%x]",
		payload[2], payload[3], payload[4],
		payload[2], payload[3], payload[4]);

	f.country_region_id = payload[5] >> 2;
	f.reserved = (payload[5] >> 6) & 0x01;
	f.local_time_offset_polarity = (payload[5] >> 7) & 0x01;
	f.local_time_offset = CONVERT_TO_16(payload[6], payload[7]);
	f.time_of_change = CONVERT_TO_40(payload[8], payload[9], payload[10], payload[11], payload[12]) & 0xffffffffff;
	f.next_time_offset = CONVERT_TO_16(payload[13], payload[14]);
	
	struct dentry *dentry = CREATE_DIRECTORY(parent, "LOCAL_TIME_OFFSET");
	CREATE_FILE_STRING(dentry, &f, country_code, XATTR_FORMAT_STRING_AND_NUMBER);
	CREATE_FILE_NUMBER(dentry, &f, country_region_id);
	CREATE_FILE_NUMBER(dentry, &f, local_time_offset_polarity);
	CREATE_FILE_NUMBER(dentry, &f, local_time_offset);
	CREATE_FILE_NUMBER(dentry, &f, time_of_change);
	CREATE_FILE_NUMBER(dentry, &f, next_time_offset);

    return 0;
}
