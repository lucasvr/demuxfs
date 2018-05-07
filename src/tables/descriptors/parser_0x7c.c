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
	struct dentry *dentry = CREATE_DIRECTORY(parent, "AAC_Audio_Descriptor");
	struct aac_descriptor d;
	int offset;

	if (! descriptor_is_parseable(parent, payload[0], 4, len))
		return -ENODATA;

	if (! DESCRIPTOR_COMES_FROM_PMT(priv))
		TS_WARNING("AAC_Audio_Descriptor found outside the PMT");

	d._profile_and_level = payload[2];
	d.aac_type_flag = (payload[3] >> 7) & 0x01;
	d.reserved = payload[3] & 0x7f;
	offset = 4;

	switch (d._profile_and_level) {
		case 0x00 ... 0x0e: 
			asprintf(&d.profile_and_level, "Reserved [%#x]", d._profile_and_level);
			break;
		case 0x0f:
			asprintf(&d.profile_and_level, "No audio profile and level defined for the " \
				"associated MPEG-4 audio stream [%#x]", d._profile_and_level);
			break;
		case 0x10:
			asprintf(&d.profile_and_level, "Main profile, level 1 [%#x]", d._profile_and_level);
			break;
		case 0x11:
			asprintf(&d.profile_and_level, "Main profile, level 2 [%#x]", d._profile_and_level);
			break;
		case 0x12:
			asprintf(&d.profile_and_level, "Main profile, level 3 [%#x]", d._profile_and_level);
			break;
		case 0x13:
			asprintf(&d.profile_and_level, "Main profile, level 4 [%#x]", d._profile_and_level);
			break;
		case 0x14 ... 0x17:
			asprintf(&d.profile_and_level, "Reserved [%#x]", d._profile_and_level);
			break;
		case 0x18:
			asprintf(&d.profile_and_level, "Scalable profile, level 1 [%#x]", d._profile_and_level);
			break;
		case 0x19:
			asprintf(&d.profile_and_level, "Scalable profile, level 2 [%#x]", d._profile_and_level);
			break;
		case 0x1a:
			asprintf(&d.profile_and_level, "Scalable profile, level 3 [%#x]", d._profile_and_level);
			break;
		case 0x1b:
			asprintf(&d.profile_and_level, "Scalable profile, level 4 [%#x]", d._profile_and_level);
			break;
		case 0x1c ... 0x1f:
			asprintf(&d.profile_and_level, "Reserved [%#x]", d._profile_and_level);
			break;
		case 0x20:
			asprintf(&d.profile_and_level, "Speech profile, level 1 [%#x]", d._profile_and_level);
			break;
		case 0x21:
			asprintf(&d.profile_and_level, "Speech profile, level 2 [%#x]", d._profile_and_level);
			break;
		case 0x22 ... 0x27:
			asprintf(&d.profile_and_level, "Reserved [%#x]", d._profile_and_level);
			break;
		case 0x28:
			asprintf(&d.profile_and_level, "Synthesis profile, level 1 [%#x]", d._profile_and_level);
			break;
		case 0x29:
			asprintf(&d.profile_and_level, "Synthesis profile, level 2 [%#x]", d._profile_and_level);
			break;
		case 0x2a:
			asprintf(&d.profile_and_level, "Synthesis profile, level 3 [%#x]", d._profile_and_level);
			break;
		case 0x2b ... 0x2f:
			asprintf(&d.profile_and_level, "Reserved [%#x]", d._profile_and_level);
			break;
		case 0x30:
			asprintf(&d.profile_and_level, "High quality audio profile, level 1 [%#x]", d._profile_and_level);
			break;
		case 0x31:
			asprintf(&d.profile_and_level, "High quality audio profile, level 2 [%#x]", d._profile_and_level);
			break;
		case 0x32:
			asprintf(&d.profile_and_level, "High quality audio profile, level 3 [%#x]", d._profile_and_level);
			break;
		case 0x33:
			asprintf(&d.profile_and_level, "High quality audio profile, level 4 [%#x]", d._profile_and_level);
			break;
		case 0x34:
			asprintf(&d.profile_and_level, "High quality audio profile, level 5 [%#x]", d._profile_and_level);
			break;
		case 0x35:
			asprintf(&d.profile_and_level, "High quality audio profile, level 6 [%#x]", d._profile_and_level);
			break;
		case 0x36:
			asprintf(&d.profile_and_level, "High quality audio profile, level 7 [%#x]", d._profile_and_level);
			break;
		case 0x37:
			asprintf(&d.profile_and_level, "High quality audio profile, level 8 [%#x]", d._profile_and_level);
			break;
		case 0x38:
			asprintf(&d.profile_and_level, "Low delay audio profile, level 1 [%#x]", d._profile_and_level);
			break;
		case 0x39:
			asprintf(&d.profile_and_level, "Low delay audio profile, level 2 [%#x]", d._profile_and_level);
			break;
		case 0x3a:
			asprintf(&d.profile_and_level, "Low delay audio profile, level 3 [%#x]", d._profile_and_level);
			break;
		case 0x3b:
			asprintf(&d.profile_and_level, "Low delay audio profile, level 4 [%#x]", d._profile_and_level);
			break;
		case 0x3c:
			asprintf(&d.profile_and_level, "Low delay audio profile, level 5 [%#x]", d._profile_and_level);
			break;
		case 0x3d:
			asprintf(&d.profile_and_level, "Low delay audio profile, level 6 [%#x]", d._profile_and_level);
			break;
		case 0x3e:
			asprintf(&d.profile_and_level, "Low delay audio profile, level 7 [%#x]", d._profile_and_level);
			break;
		case 0x3f:
			asprintf(&d.profile_and_level, "Low delay audio profile, level 8 [%#x]", d._profile_and_level);
			break;
		case 0x40:
			asprintf(&d.profile_and_level, "Natural audio profile, level 1 [%#x]", d._profile_and_level);
			break;
		case 0x41:
			asprintf(&d.profile_and_level, "Natural audio profile, level 2 [%#x]", d._profile_and_level);
			break;
		case 0x42:
			asprintf(&d.profile_and_level, "Natural audio profile, level 3 [%#x]", d._profile_and_level);
			break;
		case 0x43:
			asprintf(&d.profile_and_level, "Natural audio profile, level 4 [%#x]", d._profile_and_level);
			break;
		case 0x44 ... 0x47:
			asprintf(&d.profile_and_level, "Reserved [%#x]", d._profile_and_level);
			break;
		case 0x48:
			asprintf(&d.profile_and_level, "Mobile audio internetworking profile, level 1 [%#x]", d._profile_and_level);
			break;
		case 0x49:
			asprintf(&d.profile_and_level, "Mobile audio internetworking profile, level 2 [%#x]", d._profile_and_level);
			break;
		case 0x4a:
			asprintf(&d.profile_and_level, "Mobile audio internetworking profile, level 3 [%#x]", d._profile_and_level);
			break;
		case 0x4b:
			asprintf(&d.profile_and_level, "Mobile audio internetworking profile, level 4 [%#x]", d._profile_and_level);
			break;
		case 0x4c:
			asprintf(&d.profile_and_level, "Mobile audio internetworking profile, level 5 [%#x]", d._profile_and_level);
			break;
		case 0x4d:
			asprintf(&d.profile_and_level, "Mobile audio internetworking profile, level 6 [%#x]", d._profile_and_level);
			break;
		case 0x4e ... 0x4f:
			asprintf(&d.profile_and_level, "Reserved [%#x]", d._profile_and_level);
			break;
		case 0x50:
			asprintf(&d.profile_and_level, "AAC profile, level 1 [%#x]", d._profile_and_level);
			break;
		case 0x51:
			asprintf(&d.profile_and_level, "AAC profile, level 2 [%#x]", d._profile_and_level);
			break;
		case 0x52:
			asprintf(&d.profile_and_level, "AAC profile, level 3 [%#x]", d._profile_and_level);
			break;
		case 0x53:
			asprintf(&d.profile_and_level, "AAC profile, level 4 [%#x]", d._profile_and_level);
			break;
		case 0x54 ... 0x57:
			asprintf(&d.profile_and_level, "Reserved [%#x]", d._profile_and_level);
			break;
		case 0x58:
			asprintf(&d.profile_and_level, "High efficiency AAC profile, level 2 [%#x]", d._profile_and_level);
			break;
		case 0x59:
			asprintf(&d.profile_and_level, "High efficiency AAC profile, level 3 [%#x]", d._profile_and_level);
			break;
		case 0x5a:
			asprintf(&d.profile_and_level, "High efficiency AAC profile, level 4 [%#x]", d._profile_and_level);
			break;
		case 0x5b:
			asprintf(&d.profile_and_level, "High efficiency AAC profile, level 5 [%#x]", d._profile_and_level);
			break;
		case 0x5c ... 0x5f:
			asprintf(&d.profile_and_level, "Reserved [%#x]", d._profile_and_level);
			break;
		case 0x60:
			asprintf(&d.profile_and_level, "High efficiency AAC v2 profile, level 2 [%#x]", d._profile_and_level);
			break;
		case 0x61:
			asprintf(&d.profile_and_level, "High efficiency AAC v2 profile, level 3 [%#x]", d._profile_and_level);
			break;
		case 0x62:
			asprintf(&d.profile_and_level, "High efficiency AAC v2 profile, level 4 [%#x]", d._profile_and_level);
			break;
		case 0x63:
			asprintf(&d.profile_and_level, "High efficiency AAC v2 profile, level 5 [%#x]", d._profile_and_level);
			break;
		case 0x64 ... 0xfe:
			asprintf(&d.profile_and_level, "Reserved [%#x]", d._profile_and_level);
			break;
		case 0xff:
		default:
			asprintf(&d.profile_and_level, "Audio profile and level not specified [%#x]", d._profile_and_level);
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

