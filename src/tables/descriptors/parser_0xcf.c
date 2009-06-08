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
#include "byteops.h"
#include "fsutils.h"
#include "xattr.h"
#include "ts.h"
#include "descriptors.h"

struct transmission_type_01 {
	uint16_t reserved_1:7;
	uint16_t logo_id:9;
	uint16_t reserved_2:4;
	uint16_t logo_version:12;
	uint16_t download_data_id;
};

struct transmission_type_02 {
	uint16_t reserved_1:7;
	uint16_t logo_id:9;
};

struct transmission_type_03 {
	char logo_char[256];
	uint8_t _len;
};

struct transmission_type_other {
	char reserved_future_use[256];
	uint8_t _len;
};

struct formatted_descriptor {
	uint8_t _logo_transmission_type;
	char logo_transmission_type[256];
	struct transmission_type_01 t1;
	struct transmission_type_02 t2;
	struct transmission_type_03 t3;
	struct transmission_type_other other;
};

static const char * transmission_type_meaning(uint8_t transmission_type)
{
	switch (transmission_type) {
		case 0x01:
			return "CDT transmission type 1";
		case 0x02:
			return "CDT transmission type 2";
		case 0x03:
			return "Simple logo type system";
		default:
			return "Reserved for future use";
	}
}

/* LOGO_TRANSMISSION_DESCRIPTOR parser */
int descriptor_0xcf_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct formatted_descriptor f;
	struct dentry *dentry = CREATE_DIRECTORY(parent, "LOGO_TRANSMISSION");
	
	if (! descriptor_is_parseable(parent, payload[0], 4, len))
		return -ENODATA;

	f._logo_transmission_type = payload[2];
	snprintf(f.logo_transmission_type, sizeof(f.logo_transmission_type), "%s [%#x]",
			transmission_type_meaning(f._logo_transmission_type), 
			f._logo_transmission_type);
	CREATE_FILE_STRING(dentry, &f, logo_transmission_type, XATTR_FORMAT_STRING_AND_NUMBER);

	if (f._logo_transmission_type == 0x01) {
		f.t1.reserved_1 = payload[3] >> 1;
		f.t1.logo_id = CONVERT_TO_16(payload[3], payload[4]) & 0x01ff;
		f.t1.reserved_2 = payload[5] >> 4;
		f.t1.logo_version = CONVERT_TO_16(payload[5], payload[6]) & 0x0fff;
		f.t1.download_data_id = CONVERT_TO_16(payload[7], payload[8]);
		CREATE_FILE_NUMBER(dentry, &f.t1, logo_id);
		CREATE_FILE_NUMBER(dentry, &f.t1, logo_version);
		CREATE_FILE_NUMBER(dentry, &f.t1, download_data_id);

	} else if (f._logo_transmission_type == 0x02) {
		f.t2.reserved_1 = payload[3] >> 1;
		f.t2.logo_id = CONVERT_TO_16(payload[3], payload[4]) & 0x01ff;
		CREATE_FILE_NUMBER(dentry, &f.t2, logo_id);

	} else if (f._logo_transmission_type == 0x03) {
		int i;
		for (i=0; i<len-1; ++i)
			f.t3.logo_char[i] = payload[3+i];
		f.t3._len = i;
		CREATE_FILE_BIN(dentry, &f.t3, logo_char, f.t3._len);

	} else {
		int i;
		for (i=0; i<len-1; ++i)
			f.other.reserved_future_use[i] = payload[3+i];
		f.other._len = i;
		CREATE_FILE_BIN(dentry, &f.other, reserved_future_use, f.other._len);
	}

    return 0;
}

