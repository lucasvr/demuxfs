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

struct segmentation_mode_0x01 {
	uint64_t start_time_ntp; /* 33-bit variable */
	uint64_t end_time_ntp;   /* 33-bit variable */
};

struct segmentation_mode_0x02_to_0x05 {
	uint32_t start_time;
	uint32_t duration;
	/* optional */
	uint16_t start_time_extension;
	uint16_t duration_extension;
};

struct segmentation_mode_other {
	/* XXX: is 512 big enough? */
	char reserved_bytes[512];
};

struct formatted_descriptor {
	uint8_t segmentation_mode;
	uint8_t segmentation_info_length;
	/* segmentation_mode structure */
};

/* BASIC_LOCAL_EVENT_DESCRIPTOR parser */
int descriptor_0xd0_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct dentry *dentry;
	struct formatted_descriptor f;

	if (! descriptor_is_parseable(parent, payload[0], 4, len))
		return -ENODATA;

	dentry = CREATE_DIRECTORY(parent, "BASIC_LOCAL_EVENT");
	f.segmentation_mode = payload[2] & 0x0f;
	f.segmentation_info_length = payload[3];
	CREATE_FILE_NUMBER(dentry, &f, segmentation_mode);
	CREATE_FILE_NUMBER(dentry, &f, segmentation_info_length);
	if (f.segmentation_mode == 0x00) {
		/* Empty */
	} else if (f.segmentation_mode == 0x01) {
		struct segmentation_mode_0x01 sm;
		if (! descriptor_is_parseable(parent, payload[0], 14, len)) {
			/* Can't continue to parse */
			return -ENODATA;
		}
		sm.start_time_ntp = CONVERT_TO_40(payload[4], payload[5], payload[6], payload[7], payload[8]);
		sm.end_time_ntp = CONVERT_TO_40(payload[9], payload[10], payload[11], payload[12], payload[13]);
		CREATE_FILE_NUMBER(dentry, &sm, start_time_ntp);
		CREATE_FILE_NUMBER(dentry, &sm, end_time_ntp);
	} else if (f.segmentation_mode >= 0x02 && f.segmentation_mode <= 0x05) {
		struct segmentation_mode_0x02_to_0x05 sm;
		if (! descriptor_is_parseable(parent, payload[0], 10, len)) {
			/* Can't continue to parse */
			return -ENODATA;
		}
		sm.start_time = CONVERT_TO_24(payload[4], payload[5], payload[6]);
		sm.duration = CONVERT_TO_24(payload[7], payload[8], payload[9]);
		CREATE_FILE_NUMBER(dentry, &sm, start_time);
		CREATE_FILE_NUMBER(dentry, &sm, duration);
		if (len >= 14) {
			sm.start_time_extension = CONVERT_TO_16(payload[10], payload[11]) >> 4;
			sm.duration_extension = CONVERT_TO_16(payload[12], payload[13]) >> 4;
			CREATE_FILE_NUMBER(dentry, &sm, start_time_extension);
			CREATE_FILE_NUMBER(dentry, &sm, duration_extension);
		}
	} else {
		uint32_t n_bytes = len-5;
		struct segmentation_mode_other sm;
		if (n_bytes > sizeof(sm.reserved_bytes)) {
			dprintf("Truncating reserved_bytes array");
			n_bytes = sizeof(sm.reserved_bytes);
		}
		memcpy(sm.reserved_bytes, &payload[4], n_bytes);
		CREATE_FILE_BIN(dentry, &sm, reserved_bytes, n_bytes);
	}
	/* 
	 * XXX: there's also a Component Tag loop at the very end of this descriptor 
	 * which we need to parse 
	 */
	return 0;
}

