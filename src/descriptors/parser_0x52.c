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

struct stream_identifier_descriptor {
	uint8_t component_tag;
	char *component_name;
};

struct formatted_descriptor { 
	char *component_tag; 
};

/* STREAM_IDENTIFIER_DESCRIPTOR parser */
int descriptor_0x52_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct pmt_stream *stream = NULL;
	struct formatted_descriptor f;
	struct stream_identifier_descriptor s;
	uint8_t stream_type = 0;
	bool wrong_tag = false;

	if (! priv->shared_data)
		TS_WARNING("STREAM_IDENTIFIER_DESCRIPTOR found outside the PMT");
	else {
		stream = (struct pmt_stream *) priv->shared_data;
		stream_type = stream->stream_type_identifier;
	}

	s.component_name = "Unknown";
	s.component_tag = payload[0];
	switch (s.component_tag) {
		case 0x00:
			s.component_name = "Primary full-seg video ES";
			wrong_tag = (stream && stream_type_is_video(stream_type)) ? false : true;
			break;
		case 0x01 ... 0x0f:
			s.component_name = "Full-seg video ES";
			wrong_tag = (stream && stream_type_is_video(stream_type)) ? false : true;
			break;
		case 0x10:
			s.component_name = "Primary full-seg audio ES";
			wrong_tag = (stream && stream_type_is_audio(stream_type)) ? false : true;
			break;
		case 0x11 ... 0x2f:
			s.component_name = "Full-seg audio ES";
			wrong_tag = (stream && stream_type_is_audio(stream_type)) ? false : true;
			break;
		case 0x30:
			s.component_name = "Primary closed-caption";
			break;
		case 0x31 ... 0x37:
			s.component_name = "Closed-caption";
			break;
		case 0x38:
			s.component_name = "Primary text overpositioning";
			break;
		case 0x39 ... 0x3f:
			s.component_name = "Text overpositioning";
			break;
		case 0x40:
			s.component_name = "Primary data ES";
			break;
		case 0x41 ... 0x7f:
			s.component_name = "Other";
			break;
		case 0x80:
			s.component_name = "Primary one-seg data carousel";
			break;
		case 0x81:
			s.component_name = "Primary one-seg video stream";
			wrong_tag = (stream && stream_type_is_video(stream_type)) ? false : true;
			break;
		case 0x83:
		case 0x85:
			s.component_name = "Primary one-seg audio stream";
			wrong_tag = (stream && stream_type_is_audio(stream_type)) ? false : true;
			break;
		case 0x82:
		case 0x84:
		case 0x86 ... 0x8f:
			s.component_name = "Other one-seg";
			break;
		case 0x90 ... 0xff:
			s.component_name = "Reserved";
			break;
	}

	struct dentry *dentry;
	char contents[strlen(s.component_name) + 16];
	sprintf(contents, "%s [%#x]", s.component_name, s.component_tag);
	f.component_tag = contents;
	
	dentry = CREATE_DIRECTORY(parent, "STREAM_IDENTIFIER");
	CREATE_FILE_STRING(dentry, &f, component_tag, XATTR_FORMAT_STRING_AND_NUMBER);

	if (wrong_tag)
		TS_WARNING("Tag %#x cannot be assigned to stream of type '%s'", 
				s.component_tag, stream_type_to_string(stream_type));

    return 0;
}

