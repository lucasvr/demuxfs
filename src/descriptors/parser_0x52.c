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

static bool stream_type_is_video(uint8_t stream_type)
{
	if (stream_type == 0x01 || stream_type == 0x02 || stream_type == 0x1b)
		return true;
	return false;
}

static bool stream_type_is_audio(uint8_t stream_type)
{
	switch (stream_type) {
		case 0x03:
		case 0x04:
		case 0x0f:
		case 0x11:
		case 0x81:
			return true;
		default:
			return false;
	}
}

static const char *resolv_stream_type(uint8_t stream_type)
{
	static const char *table[0xff+1];
	static bool initialized = false;

	if (! initialized) {
		table[0x00] = "ITU-T | ISO/IEC reserved";
		table[0x01] = "ISO/IEC 11172-2 Video - H.261 - MPEG-1 Video";
		table[0x02] = "ITU-T H.262 | ISO/IEC 13818-2 Video - MPEG-2 Video | 11172-2 constrained parameter video stream";
		table[0x03] = "ISO/IEC 11172 Audio - MPEG-1 Audio Layer 2 (MP2)";
		table[0x04] = "ISO/IEC 13818-3 Audio - MPEG-2 Audio Layer 3 (MP3)";
		table[0x05] = "ITU-T H.222.0 | ISO/IEC 13818-1 private sections";
		table[0x06] = "ITU-T H.222.0 | ISO/IEC 13818-1 PES packets with private data";
		table[0x07] = "ISO/IEC 13522 MHEG";
		table[0x08] = "ITU-T Rec. H.222.0|ISO/IEC 13818-1 Annex A DSMCC";
		table[0x09] = "ITU-T Rec. H.222.1";
		table[0x0A] = "ISO 13818-6 type A - DSM CC (Multi-protocol Encapsulation)";
		table[0x0B] = "ISO 13818-6 type B - DSM CC (DSM-CC U-N Messages)";
		table[0x0C] = "ISO 13818-6 type C - DSM CC (DSM-CC Stream Descriptors)";
		table[0x0D] = "ISO 13818-6 type D - DSM CC (DSM-CC Sections - anytype, including private data)";
		table[0x0E] = "ITU-T Rec. H.222.0|ISO/IEC 13818-1 auxiliary - DSM CC";
		table[0x0F] = "ISO/IEC 13818-7 Audio with ADTS transport syntax (AAC)";
		table[0x10] = "ISO/IEC 14496-2 Visual";
		table[0x11] = "ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3/AMD 1 (HE-AAC)";
		table[0x12] = "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets";
		table[0x13] = "ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC 14496- sections";
		table[0x14] = "ISO/IEC 13818-6 Synchronized Download Protocol";
		table[0x15] = "Metadata carried in PES packets";
		table[0x16] = "Metadata carried in metadata_sections";
		table[0x17] = "Metadata carried in ISO/IEC 13818-6 Data Carousel";
		table[0x18] = "Metadata carried in ISO/IEC 13818-6 Object Carousel";
		table[0x19] = "Metadata carried in ISO/IEC 13818-6 Synchronized Download Protocol";
		table[0x1A] = "IPMP stream (defined in ISO/IEC 13818-11, MPEG-2 IPMP)";
		table[0x1B] = "AVC video stream as defined in ITU-T Rec. H.264 | ISO/IEC 14496-10 Video";
		table[0x1C] = "ITU-T H.222.0 | ISO/IEC 13818-1 reserved";
		for (int i=0x1D; i <= 0x7D; ++i)
			table[i] = table[0x1C]; /* "ITU-T H.222.0 | ISO/IEC 13818-1 reserved" */
		table[0x7E] = "Data pipe";
		table[0x7F] = "IPMP stream";
		table[0x80] = "User private";
		for (int i=0x81; i <= 0xFF; ++i)
			table[i] = table[0x80]; /* "User private" */
		table[0x81] = "Dolby Digital Audio (AC3)"; /* Assigned by Intel in the ATSC standard */
		initialized = true;
	}
	return table[stream_type];
}


/* STREAM_IDENTIFIER_DESCRIPTOR parser */
int descriptor_0x52_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct pmt_stream *stream = NULL;
	struct formatted_descriptor f, *fptr = &f;
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
			/* Reserved */
			break;
	}
	struct dentry *dfile;
	char contents[strlen(s.component_name) + 16];
	sprintf(contents, "%s (%#x)", s.component_name, s.component_tag);
	fptr->component_tag = contents;
	CREATE_FILE_STRING(parent, fptr, component_tag, &dfile);

	if (wrong_tag)
		TS_WARNING("Tag %#x cannot be assigned to stream of type '%s'\n", 
				s.component_tag, resolv_stream_type(stream_type));

    return 0;
}

