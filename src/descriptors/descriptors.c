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
#include "descriptors.h"
#include "ts.h"

uint8_t descriptors_parse(const char *payload, uint8_t num_descriptors, 
		struct dentry *parent, struct demuxfs_data *priv)
{
	int ret;
	uint8_t n;
	uint8_t offset = 0;
	for (n=0; n<num_descriptors; ++n) {
		uint8_t descriptor_tag = payload[offset];
		uint8_t descriptor_length = payload[offset+1];
		struct descriptor *d = descriptors_find(descriptor_tag, priv);
		if (! d) {
			TS_WARNING("invalid descriptor tag %#x", descriptor_tag);
			offset += 2 + descriptor_length;
			continue;
		}
		dprintf("Calling parser for descriptor %#4x-%s (descriptor %d/%d)", 
				descriptor_tag, d->name, n+1, num_descriptors);
		ret = d->parser(&payload[offset+2], descriptor_length, parent, priv);
		if (ret < 0)
			TS_WARNING("error parsing descriptor tag %#x: %s", descriptor_tag, 
					strerror(-ret));
		offset += 2 + descriptor_length;
	}
	return offset;
}

int descriptors_count(const char *payload, uint16_t info_length)
{
	int num = 0;
	const char *p = payload;
	while (info_length > 0) {
		if (info_length < 2)
			return 0;
		if (info_length < 2 + p[1])
			return 0;
		info_length -= 2 + p[1];
		p += 2 + p[1];
		num++;
	}
	return num;
}

struct descriptor *descriptors_find(uint8_t tag, struct demuxfs_data *priv)
{
	struct descriptor *d = &priv->ts_descriptors[tag];
	return d->parser ? d : NULL;
}

#define ADD_DESCRIPTOR(dname,dtag,priv) \
	do { \
		struct descriptor *d = &priv->ts_descriptors[dtag]; \
		d->tag = dtag; \
		d->name = strdup(dname); \
		d->parser = descriptor_ ## dtag ## _parser; \
	} while(0)

void descriptors_destroy(struct descriptor *descriptor_list)
{
	if (descriptor_list) {
		for (int i=0; i<0xff+1; ++i)
			if (descriptor_list[i].name)
				free(descriptor_list[i].name);
		free(descriptor_list);
	}
}

struct descriptor *descriptors_init(struct demuxfs_data *priv)
{
	priv->ts_descriptors = (struct descriptor *) calloc(0xff+1, sizeof(struct descriptor));

	/* TS descriptors and their tag values, defined by ISO/IEC 13818-1 */
	ADD_DESCRIPTOR("VIDEO_STREAM_DESCRIPTOR",                  0x02, priv);
	ADD_DESCRIPTOR("AUDIO_STREAM_DESCRIPTOR",                  0x03, priv);
	ADD_DESCRIPTOR("HIERARCHY_DESCRIPTOR",                     0x04, priv);
	ADD_DESCRIPTOR("REGISTRATION_DESCRIPTOR",                  0x05, priv);
	ADD_DESCRIPTOR("DATA_STREAM_ALIGNMENT_DESCRIPTOR",         0x06, priv);
	ADD_DESCRIPTOR("TARGET_BACKGROUND_GRID_DESCRIPTOR",        0x07, priv);
	ADD_DESCRIPTOR("VIDEO_WINDOW_DESCRIPTOR",                  0x08, priv);
	ADD_DESCRIPTOR("CONDITIONAL_ACCESS_DESCRIPTOR",            0x09, priv);
	ADD_DESCRIPTOR("ISO_639_LANGUAGE_DESCRIPTOR",              0x0a, priv);
	ADD_DESCRIPTOR("SYSTEM_CLOCK_DESCRIPTOR",                  0x0b, priv);
	ADD_DESCRIPTOR("MULTIPLEX_BUFFER_UTILIZATION_DESCRIPTOR",  0x0c, priv);
	ADD_DESCRIPTOR("COPYRIGHT_DESCRIPTOR",                     0x0d, priv);
	ADD_DESCRIPTOR("MAXIMUM_BITRATE_DESCRIPTOR",               0x0e, priv);
	ADD_DESCRIPTOR("PRIVATE_DATA_INDICATOR_DESCRIPTOR",        0x0f, priv);
	ADD_DESCRIPTOR("SMOOTHING_BUFFER_DESCRIPTOR",              0x10, priv);
	ADD_DESCRIPTOR("STD_DESCRIPTOR",                           0x11, priv);
	ADD_DESCRIPTOR("IBP_DESCRIPTOR",                           0x12, priv);
	/* TS descriptors and their tag values, defined by ISO/IEC 13818-6 (0x13-0x1a) */
	ADD_DESCRIPTOR("CAROUSEL_ID_DESCRIPTOR",                   0x13, priv);
	ADD_DESCRIPTOR("ASSOCIATION_TAG_DESCRIPTOR",               0x14, priv);
	ADD_DESCRIPTOR("DEFERRED_ASSOCIATION_TAG_DESCRIPTOR",      0x15, priv);
	/* TS descriptors and their tag values, defined by ISO/IEC 13818-1 */
	ADD_DESCRIPTOR("MPEG-4_VIDEO_DESCRIPTOR",                  0x1b, priv);
	ADD_DESCRIPTOR("MPEG-4_AUDIO_DESCRIPTOR",                  0x1c, priv);
	ADD_DESCRIPTOR("IOD_DESCRIPTOR",                           0x1d, priv);
	ADD_DESCRIPTOR("SL_DESCRIPTOR",                            0x1e, priv);
	ADD_DESCRIPTOR("FMC_DESCRIPTOR",                           0x1f, priv);
	ADD_DESCRIPTOR("EXTERNAL_ES_ID_DESCRIPTOR",                0x20, priv);
	ADD_DESCRIPTOR("MUXCODE_DESCRIPTOR",                       0x21, priv);
	ADD_DESCRIPTOR("FMX_BUFFER_SIZE_DESCRIPTOR",               0x22, priv);
	ADD_DESCRIPTOR("MULTIPLEX_BUFFER_DESCRIPTOR",              0x23, priv);
	/* ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved */
	ADD_DESCRIPTOR("AVC_VIDEO_DESCRIPTOR",                     0x28, priv);
	ADD_DESCRIPTOR("AVC_TIMING_AND_HDR_DESCRIPTOR",            0x2a, priv);
	/* SBTVD */
	ADD_DESCRIPTOR("NETWORK_NAME_DESCRIPTOR",                  0x40, priv);
	ADD_DESCRIPTOR("SERVICE_LIST_DESCRIPTOR",                  0x41, priv);
	ADD_DESCRIPTOR("STUFFING_DESCRIPTOR",                      0x42, priv);
	ADD_DESCRIPTOR("BOUQUET_NAME_DESCRIPTOR",                  0x47, priv);
	ADD_DESCRIPTOR("SERVICE_DESCRIPTOR",                       0x48, priv);
	ADD_DESCRIPTOR("COUNTRY_AVAILABILITY_DESCRIPTOR",          0x49, priv);
	ADD_DESCRIPTOR("LINKAGE_DESCRIPTOR",                       0x4a, priv);
	ADD_DESCRIPTOR("NVOD_REFERENCE_DESCRIPTOR",                0x4b, priv);
	ADD_DESCRIPTOR("TIME_SHIFTED_SERVICE_DESCRIPTOR",          0x4c, priv);
	ADD_DESCRIPTOR("SHORT_EVENT_DESCRIPTOR",                   0x4d, priv);
	ADD_DESCRIPTOR("EXTENDED_EVENT_DESCRIPTOR",                0x4e, priv);
	ADD_DESCRIPTOR("TIME_SHIFTED_EVENT_DESCRIPTOR",            0x4f, priv);
	ADD_DESCRIPTOR("COMPONENT_DESCRIPTOR",                     0x50, priv);
	ADD_DESCRIPTOR("MOSAIC_DESCRIPTOR",                        0x51, priv);
	ADD_DESCRIPTOR("STREAM_IDENTIFIER_DESCRIPTOR",             0x52, priv);
	ADD_DESCRIPTOR("CA_IDENTIFIER_DESCRIPTOR",                 0x53, priv);
	ADD_DESCRIPTOR("CONTENT_DESCRIPTOR",                       0x54, priv);
	ADD_DESCRIPTOR("PARENTAL_RATING_DESCRIPTOR",               0x55, priv);
	ADD_DESCRIPTOR("LOCAL_TIME_OFFSET_DESCRIPTOR",             0x58, priv);
	ADD_DESCRIPTOR("PARTIAL_TRANSPORT_STREAM_DESCRIPTOR",      0x63, priv);
	ADD_DESCRIPTOR("AAC_AUDIO_DESCRIPTOR",                     0x7c, priv);
	/* 0x80 - 0xBF - Reserved for identification of companies */
	ADD_DESCRIPTOR("HIERARCHICAL_TRANSMISSION_DESCRIPTOR",     0xc0, priv);
	ADD_DESCRIPTOR("DIGITAL_COPY_CONTROL_DESCRIPTOR",          0xc1, priv);
	ADD_DESCRIPTOR("NETWORK_IDENTIFIER_DESCRIPTOR",            0xc2, priv);
	ADD_DESCRIPTOR("PARTIAL_TRANSPORT_STREAM_TIME_DESCRIPTOR", 0xc3, priv);
	ADD_DESCRIPTOR("AUDIO_COMPONENT_DESCRIPTOR",               0xc4, priv);
	ADD_DESCRIPTOR("HYPERLINK_DESCRIPTOR",                     0xc5, priv);
	ADD_DESCRIPTOR("TARGET_AREA_DESCRIPTOR",                   0xc6, priv);
	ADD_DESCRIPTOR("DATA_CONTENTS_DESCRIPTOR",                 0xc7, priv);
	ADD_DESCRIPTOR("VIDEO_DECODE_CONTROL_DESCRIPTOR",          0xc8, priv);
	ADD_DESCRIPTOR("DOWNLOAD_CONTENT_DESCRIPTOR",              0xc9, priv);
	ADD_DESCRIPTOR("CA_EMM_TS_DESCRIPTOR",                     0xca, priv);
	ADD_DESCRIPTOR("CA_CONTRACT_INFORMATION_DESCRIPTOR",       0xcb, priv);
	ADD_DESCRIPTOR("CA_SERVICE_DESCRIPTOR",                    0xcc, priv);
	ADD_DESCRIPTOR("TS_INFORMATION_DESCRIPTOR",                0xcd, priv);
	ADD_DESCRIPTOR("EXTENDED_BROADCASTER_DESCRIPTOR",          0xce, priv);
	ADD_DESCRIPTOR("LOGO_TRANSMISSION_DESCRIPTOR",             0xcf, priv);
	ADD_DESCRIPTOR("BASIC_LOCAL_EVENT_DESCRIPTOR",             0xd0, priv);
	ADD_DESCRIPTOR("REFERENCE_DESCRIPTOR",                     0xd1, priv);
	ADD_DESCRIPTOR("NODE_RELATION_DESCRIPTOR",                 0xd2, priv);
	ADD_DESCRIPTOR("SHORT_NODE_INFORMATION_DESCRIPTOR",        0xd3, priv);
	ADD_DESCRIPTOR("STC_REFERENCE_DESCRIPTOR",                 0xd4, priv);
	ADD_DESCRIPTOR("SERIES_DESCRIPTOR",                        0xd5, priv);
	ADD_DESCRIPTOR("EVENT_GROUP_DESCRIPTOR",                   0xd6, priv);
	ADD_DESCRIPTOR("SI_PARAMETER_DESCRIPTOR",                  0xd7, priv);
	ADD_DESCRIPTOR("BROADCASTER_NAME_DESCRIPTOR",              0xd8, priv);
	ADD_DESCRIPTOR("COMPONENT_GROUP_DESCRIPTOR",               0xd9, priv);
	ADD_DESCRIPTOR("SI_PRIME_TS_DESCRIPTOR",                   0xda, priv);
	ADD_DESCRIPTOR("BOARD_INFORMATION_DESCRIPTOR",             0xdb, priv);
	ADD_DESCRIPTOR("LDT_LINKAGE_DESCRIPTOR",                   0xdc, priv);
	ADD_DESCRIPTOR("CONNECTED_TRANSMISSION_DESCRIPTOR",        0xdd, priv);
	ADD_DESCRIPTOR("CONTENT_AVAILABILITY_DESCRIPTOR",          0xde, priv);
	ADD_DESCRIPTOR("SERVICE_GROUP_DESCRIPTOR",                 0xe0, priv);
	/* 0xe1 - 0xF6 - Not defined */
	ADD_DESCRIPTOR("CAROUSEL_COMPATIBLE_COMPOSITE_DESCRIPTOR", 0xf7, priv);
	ADD_DESCRIPTOR("CONDITIONAL_PLAYBACK_DESCRIPTOR",          0xf8, priv);
	ADD_DESCRIPTOR("TERRESTRIAL_DELIVERY_SYSTEM_DESCRIPTOR",   0xfa, priv);
	ADD_DESCRIPTOR("PARTIAL_RECEPTION_DESCRIPTOR",             0xfb, priv);
	ADD_DESCRIPTOR("EMERGENCY_INFORMATION_DESCRIPTOR",         0xfc, priv);
	ADD_DESCRIPTOR("DATA_COMPONENT_DESCRIPTOR",                0xfd, priv);
	ADD_DESCRIPTOR("SYSTEM_MANAGEMENT_DESCRIPTOR",             0xfe, priv);
	ADD_DESCRIPTOR("USER_PRIVATE_DESCRIPTOR",                  0xff, priv);
	return priv->ts_descriptors;
}
