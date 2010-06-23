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
#include "tables/psi.h"
#include "tables/pmt.h"
#include "descriptors.h"

struct aac_descriptor {
	uint8_t _profile_and_level;
	char *profile_and_level;
	uint8_t aac_type_flag:1;
	uint8_t reserved:7;
	uint8_t aac_type;
	char *additional_info;
};

/* AAC_AUDIO_DESCRIPTOR parser */
int descriptor_0x7c_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct dentry *dentry = CREATE_DIRECTORY(parent, "AAC_AUDIO");
	struct aac_descriptor d;
	int offset;

	if (! descriptor_is_parseable(parent, payload[0], 4, len))
		return -ENODATA;

	if (! DESCRIPTOR_COMES_FROM_PMT(priv))
		TS_WARNING("AAC_AUDIO_DESCRIPTOR found outside the PMT");

	d._profile_and_level = payload[2];
	d.aac_type_flag = (payload[3] >> 7) & 0x01;
	d.reserved = payload[3] & 0x7f;
	offset = 4;

	switch (d._profile_and_level) {
		case 0x00 ... 0x27: 
			asprintf(&d.profile_and_level, "Reserved [%#x]", d._profile_and_level);
			break;
		case 0x28:
			asprintf(&d.profile_and_level, "AAC Profile L1 [%#x]", d._profile_and_level);
			break;
		case 0x29:
			asprintf(&d.profile_and_level, "AAC Profile L2 [%#x]", d._profile_and_level);
			break;
		case 0x2a:
			asprintf(&d.profile_and_level, "AAC Profile L4 [%#x]", d._profile_and_level);
			break;
		case 0x2b:
			asprintf(&d.profile_and_level, "AAC Profile L5 [%#x]", d._profile_and_level);
			break;
		case 0x2c:
			asprintf(&d.profile_and_level, "High Efficiency AAC Profile L2 [%#x]", d._profile_and_level);
			break;
		case 0x2d:
			asprintf(&d.profile_and_level, "High Efficiency AAC Profile L3 [%#x]", d._profile_and_level);
			break;
		case 0x2e:
			asprintf(&d.profile_and_level, "High Efficiency AAC Profile L4 [%#x]", d._profile_and_level);
			break;
		case 0x2f:
			asprintf(&d.profile_and_level, "High Efficiency AAC Profile L5 [%#x]", d._profile_and_level);
			break;
		case 0x30 ... 0x7f:
			asprintf(&d.profile_and_level, "Reserved [%#x]", d._profile_and_level);
			break;
		case 0x80 ... 0xfd:
			asprintf(&d.profile_and_level, "Private use [%#x]", d._profile_and_level);
			break;
		case 0xfe:
			asprintf(&d.profile_and_level, "Audio profile not specified [%#x]", d._profile_and_level);
			break;
		case 0xff:
		default:
			asprintf(&d.profile_and_level, "Audio description is not available [%#x]", d._profile_and_level);
	}
	if (d.profile_and_level) {
		CREATE_FILE_STRING(dentry, &d, profile_and_level, XATTR_FORMAT_STRING_AND_NUMBER);
		free(d.profile_and_level);
	}
	CREATE_FILE_NUMBER(dentry, &d, aac_type_flag);

	if (d.aac_type_flag) {
		if (len < 5) {
			TS_WARNING("aac_type_flag is set but descriptor contains only %d bytes", len);
			return -ENODATA;
		}
		d.aac_type = payload[offset++];
		CREATE_FILE_NUMBER(dentry, &d, aac_type);
	}

	if (offset < len) {
		int infolen = len-offset;
		d.additional_info = malloc(sizeof(char) * infolen);
		if (! d.additional_info) {
			perror("malloc");
			return -ENOMEM;
		}
		memcpy(d.additional_info, &payload[offset], infolen);
		CREATE_FILE_BIN(dentry, &d, additional_info, infolen);
		free(d.additional_info);
	}

    return 0;
}

