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
#include "xattr.h"
#include "ts.h"
#include "descriptors.h"

struct formatted_descriptor {
	uint8_t still_picture_flag:1;
	uint8_t sequence_end_code_flag:1;
	uint8_t _video_encode_format:4;
	char video_encode_format[64];
	uint8_t reserved:2;
};

/* VIDEO_DECODE_CONTROL_DESCRIPTOR parser */
int descriptor_0xc8_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct dentry *dentry;
	struct formatted_descriptor f;
	char video_fmt[64];

	if (! descriptor_is_parseable(parent, payload[0], 3, len))
		return -ENODATA;

	dentry = CREATE_DIRECTORY(parent, "VIDEO_DECODE_CONTROL");
	f.still_picture_flag = (payload[2] & 0x80) ? 1 : 0;
	f.sequence_end_code_flag = (payload[2] & 0x40) ? 1 : 0;
	f._video_encode_format = (payload[2] >> 2) & 0x0f;
	f.reserved = payload[2] & 0x03;
	CREATE_FILE_NUMBER(dentry, &f, still_picture_flag);
	CREATE_FILE_NUMBER(dentry, &f, sequence_end_code_flag);

	switch (f._video_encode_format) {
		case 0: sprintf(video_fmt, "1080p");
				break;
		case 1: sprintf(video_fmt, "1080i");
				break;
		case 2: sprintf(video_fmt, "720p");
				break;
		case 3: sprintf(video_fmt, "480p");
				break;
		case 4: sprintf(video_fmt, "480i");
				break;
		case 5: sprintf(video_fmt, "240p");
				break;
		case 6: sprintf(video_fmt, "120p");
				break;
		default:
				sprintf(video_fmt, "Reserved");
	}
	sprintf(f.video_encode_format, "%s [%#x]", video_fmt, f._video_encode_format);
	CREATE_FILE_STRING(dentry, &f, video_encode_format, XATTR_FORMAT_STRING_AND_NUMBER);

    return 0;
}

