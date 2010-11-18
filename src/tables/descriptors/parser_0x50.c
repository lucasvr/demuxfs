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
#include "byteops.h"
#include "descriptors.h"

struct formatted_descriptor {
	uint8_t reserved:4;
	uint8_t stream_content:4;
	uint8_t component_type;
	uint8_t component_tag;
	char ISO_639_language_code[4];
	char *component_description;
	char *text;
};

/* COMPONENT_DESCRIPTOR parser */
int descriptor_0x50_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct dentry *dentry;
	struct formatted_descriptor f;

	if (! descriptor_is_parseable(parent, payload[0], 6, len))
		return -ENODATA;

	memset(&f, 0, sizeof(f));
	f.reserved       = (payload[2] >> 4) & 0x0f;
	f.stream_content = (payload[2] & 0x0f);
	f.component_type =  payload[3];
	f.component_tag  =  payload[4];
	memcpy(&f.ISO_639_language_code, &payload[5], 3);

	dentry = CREATE_DIRECTORY(parent, "Component_Descriptor");
	CREATE_FILE_NUMBER(dentry, &f, stream_content);
	CREATE_FILE_NUMBER(dentry, &f, component_type);
	CREATE_FILE_NUMBER(dentry, &f, component_tag);
	CREATE_FILE_STRING(dentry, &f, ISO_639_language_code, XATTR_FORMAT_STRING);

	f.component_description = NULL;

	if (f.stream_content == 0x01 || f.stream_content == 0x05) {
		const char *video_type[] = {
			"MPEG2 video",
			"H264/AVC video",
		};
		int v_index = f.stream_content == 0x01 ? 0 : 1;

		const char *resolutions[] = {
			"480i (525i)",
			"480p (525p)",
			"1080i (1125i)",
			"720p (750p)",
			"240p",
			"1080p (1125p)",
		};
		int resolution = f.component_type & 0x0f;
		int r_index = ((f.component_type >> 4) & 0x0f) == 0 ? 0 :
					  ((f.component_type >> 4) & 0x0f) - 0x0a + 1;

		switch (resolution) {
			case 0x01:
				asprintf(&f.component_description, "%s %s, 4:3 aspect ratio", 
					video_type[v_index], resolutions[r_index]);
				break;
			case 0x02:
				asprintf(&f.component_description, "%s %s, 16:9 aspect ratio with pan vector", 
					video_type[v_index], resolutions[r_index]);
				break;
			case 0x03:
				asprintf(&f.component_description, "%s %s, 16:9 aspect ratio without a pan vector", 
					video_type[v_index], resolutions[r_index]);
				break;
			case 0x04:
				asprintf(&f.component_description, "%s %s, 16:9 aspect ratio", 
					video_type[v_index], resolutions[r_index]);
				break;
			default:
				asprintf(&f.component_description, "Reserved for future use");
		}

	} else if (f.stream_content == 0x02 || f.stream_content == 0x06) {
		const char *audio_type[] = {
			"MPEG2 AAC",
			"HE-AAC MPEG4",
			"AAC MPEG4",
		};
		int a_index = f.stream_content == 0x02 ? 0 : 
			f.component_type < 0x51 ? 1 : 2;

		if (f.component_type == 0x01 || (f.component_type == 0x51 && f.stream_content == 0x06))
			asprintf(&f.component_description, "%s audio, 1/0 mode (single mono)", audio_type[a_index]);
		else if (f.component_type == 0x02 || (f.component_type == 0x52 && f.stream_content == 0x06))
			asprintf(&f.component_description, "%s audio, 1/0 + 1/0 mode (dual mono)", audio_type[a_index]);
		else if (f.component_type == 0x03 || (f.component_type == 0x53 && f.stream_content == 0x06))
			asprintf(&f.component_description, "%s audio, 2/0 mode (stereo)", audio_type[a_index]);
		else if (f.component_type == 0x04 || (f.component_type == 0x54 && f.stream_content == 0x06))
			asprintf(&f.component_description, "%s audio, 2/1 mode", audio_type[a_index]);
		else if (f.component_type == 0x05 || (f.component_type == 0x55 && f.stream_content == 0x06))
			asprintf(&f.component_description, "%s audio, 3/0 mode", audio_type[a_index]);
		else if (f.component_type == 0x06 || (f.component_type == 0x56 && f.stream_content == 0x06))
			asprintf(&f.component_description, "%s audio, 2/2 mode", audio_type[a_index]);
		else if (f.component_type == 0x07 || (f.component_type == 0x57 && f.stream_content == 0x06))
			asprintf(&f.component_description, "%s audio, 3/1 mode", audio_type[a_index]);
		else if (f.component_type == 0x08 || (f.component_type == 0x58 && f.stream_content == 0x06))
			asprintf(&f.component_description, "%s audio, 3/2 mode", audio_type[a_index]);
		else if (f.component_type == 0x09 || (f.component_type == 0x59 && f.stream_content == 0x06))
			asprintf(&f.component_description, "%s audio, 3/2 mode + LFE", audio_type[a_index]);
		else if (f.component_type == 0x49 || (f.component_type == 0x9f && f.stream_content == 0x06))
			asprintf(&f.component_description, "%s pure audio description for the visually impaired", 
				audio_type[a_index]);
		else if (f.component_type == 0x41 || (f.component_type == 0xa0 && f.stream_content == 0x06))
			asprintf(&f.component_description, "%s audio with audio boost for the hearing impaired", 
				audio_type[a_index]);
		else if ((f.component_type == 0x42 || f.component_type == 0xa1) && f.stream_content == 0x06)
			asprintf(&f.component_description, "%s mixed audio description for the hearing impaired",
				audio_type[a_index]);
 		else if (f.component_type == 0x43 && f.stream_content == 0x06)
			asprintf(&f.component_description, "HE-AAC MPEG4 v2 audio, 1/0 mode (mono)");
		else if (f.component_type == 0x44 && f.stream_content == 0x06)
			asprintf(&f.component_description, "HE-AAC MPEG4 v2 audio, 2/0 mode (stereo)");
		else if (f.component_type == 0x45 && f.stream_content == 0x06)
			asprintf(&f.component_description, "HE-AAC MPEG4 v2 pure audio description for the visually impaired");
		else if (f.component_type == 0x46 && f.stream_content == 0x06)
			asprintf(&f.component_description, "HE-AAC MPEG4 v2 audio with audio boost for the hearing impaired");
		else if (f.component_type == 0x47 && f.stream_content == 0x06)
			asprintf(&f.component_description, "HE-AAC MPEG4 v2 mixed audio description for the visually impaired");
		else if (f.component_type >= 0xb0 && f.component_type <= 0xfe)
			asprintf(&f.component_description, "User defined");
		else
			asprintf(&f.component_description, "Reserved for future use");

	} else {
		asprintf(&f.component_description, "Reserved for future use");
	}

	if (f.component_description) {
		CREATE_FILE_STRING(dentry, &f, component_description, XATTR_FORMAT_STRING);
		free(f.component_description);
	}

	if (len > 8) {
		f.text = strndup(&payload[7], len-8);
		CREATE_FILE_STRING(dentry, &f, text, XATTR_FORMAT_STRING);
		free(f.text);
	}

    return 0;
}

