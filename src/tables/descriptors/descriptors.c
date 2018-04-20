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
#include "descriptors.h"
#include "ts.h"

uint32_t descriptors_parse(const char *payload, uint8_t num_descriptors, 
		struct dentry *parent, struct demuxfs_data *priv)
{
	int ret;
	uint8_t n;
	uint32_t offset = 0;
	for (n=0; n<num_descriptors; ++n) {
		uint8_t descriptor_tag = payload[offset];
		uint8_t descriptor_length = payload[offset+1];
		struct descriptor *d = descriptors_find(descriptor_tag, priv);
		if (! d) {
			TS_WARNING("Invalid descriptor tag %#04x", descriptor_tag);
			offset += 2 + descriptor_length;
			continue;
		}
		TS_VERBOSE("Parsing descriptor %#04x-%s (#%d/%d)", 
				descriptor_tag, d->name, n+1, num_descriptors);
		ret = d->parser(&payload[offset], descriptor_length, parent, priv);
		if (ret < 0)
			TS_WARNING("Error parsing descriptor tag %#x: %s", descriptor_tag, 
					strerror(-ret));
		offset += 2 + descriptor_length;
	}
	return offset;
}

bool descriptor_is_parseable(struct dentry *dentry, uint8_t tag, int expected, int found)
{
	expected -= 2;
	if (found < expected) {
		TS_WARNING("Tag %#0x: descriptor size mismatch. Expected at least %d bytes, found %d",
				tag, expected, found);
		return false;
	}
	return true;
}

int descriptors_count(const char *payload, uint16_t info_length)
{
	int num = 0, len = (int) info_length;
	const char *p = payload;
	while (len > 0) {
		int count = len >= 2 ? p[1] + 2 : -1;
		if (count < 0)
			return 0;
		len -= count;
		p += count;
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
	int i;
	if (descriptor_list) {
		for (i=0; i<0xff+1; ++i)
			if (descriptor_list[i].name)
				free(descriptor_list[i].name);
		free(descriptor_list);
	}
}

struct descriptor *descriptors_init(struct demuxfs_data *priv)
{
	priv->ts_descriptors = (struct descriptor *) calloc(0xff+1, sizeof(struct descriptor));

	/* TS descriptors and their tag values, defined by ISO/IEC 13818-1 */
	ADD_DESCRIPTOR("Video_Stream_Descriptor",                  0x02, priv);
	ADD_DESCRIPTOR("Audio_Stream_Descriptor",                  0x03, priv);
	ADD_DESCRIPTOR("Hierarchy_Descriptor",                     0x04, priv);
	ADD_DESCRIPTOR("Registration_Descriptor",                  0x05, priv);
	ADD_DESCRIPTOR("Data_Stream_Alignment_Descriptor",         0x06, priv);
	ADD_DESCRIPTOR("Target_Background_Grid_Descriptor",        0x07, priv);
	ADD_DESCRIPTOR("Video_Window_Descriptor",                  0x08, priv);
	ADD_DESCRIPTOR("Conditional_Access_Descriptor",            0x09, priv);
	ADD_DESCRIPTOR("ISO_639_Language_Descriptor",              0x0a, priv);
	ADD_DESCRIPTOR("System_Clock_Descriptor",                  0x0b, priv);
	ADD_DESCRIPTOR("Multiplex_Buffer_Utilization_Descriptor",  0x0c, priv);
	ADD_DESCRIPTOR("Copyright_Descriptor",                     0x0d, priv);
	ADD_DESCRIPTOR("Maximum_Bitrate_Descriptor",               0x0e, priv);
	ADD_DESCRIPTOR("Private_Data_Indicator_Descriptor",        0x0f, priv);
	ADD_DESCRIPTOR("Smoothing_Buffer_Descriptor",              0x10, priv);
	ADD_DESCRIPTOR("STD_Descriptor",                           0x11, priv);
	ADD_DESCRIPTOR("IBP_Descriptor",                           0x12, priv);
	/* TS descriptors and their tag values, defined by ISO/IEC 13818-6 (0x13-0x1a) */
	ADD_DESCRIPTOR("Carousel_Id_Descriptor",                   0x13, priv);
	ADD_DESCRIPTOR("Association_Tag_Descriptor",               0x14, priv);
	ADD_DESCRIPTOR("Deferred_Association_Tag_Descriptor",      0x15, priv);
	/* TS descriptors and their tag values, defined by ISO/IEC 13818-1 */
	ADD_DESCRIPTOR("MPEG-4_Video_Descriptor",                  0x1b, priv);
	ADD_DESCRIPTOR("MPEG-4_Audio_Descriptor",                  0x1c, priv);
	ADD_DESCRIPTOR("IOD_Descriptor",                           0x1d, priv);
	ADD_DESCRIPTOR("SL_Descriptor",                            0x1e, priv);
	ADD_DESCRIPTOR("FMC_Descriptor",                           0x1f, priv);
	ADD_DESCRIPTOR("External_ES_Id_Descriptor",                0x20, priv);
	ADD_DESCRIPTOR("Muxcode_Descriptor",                       0x21, priv);
	ADD_DESCRIPTOR("FMX_Buffer_Size_Descriptor",               0x22, priv);
	ADD_DESCRIPTOR("Multiplex_Buffer_Descriptor",              0x23, priv);
	/* TS descriptors and their tag values, defined by ISO/IEC 13818-1 */
	ADD_DESCRIPTOR("Content_Labeling_Descriptor",              0x24, priv);
	ADD_DESCRIPTOR("Metadata_Pointer_Descriptor",              0x25, priv);
	ADD_DESCRIPTOR("Metadata_Descriptor",                      0x26, priv);
	ADD_DESCRIPTOR("Metadata_STD_Descriptor",                  0x27, priv);
	/* ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved */
	ADD_DESCRIPTOR("AVC_Video_Descriptor",                     0x28, priv);
	ADD_DESCRIPTOR("AVC_Timing_and_HDR_Descriptor",            0x2a, priv);
	ADD_DESCRIPTOR("MPEG-2_AAC_Audio_Descriptor",              0x2b, priv);
	ADD_DESCRIPTOR("FlexMuxTiming_Descriptor",                 0x2c, priv);
	/* SBTVD */
	ADD_DESCRIPTOR("Network_Name_Descriptor",                  0x40, priv);
	ADD_DESCRIPTOR("Service_List_Descriptor",                  0x41, priv);
	ADD_DESCRIPTOR("Stuffing_Descriptor",                      0x42, priv);
	ADD_DESCRIPTOR("Satellite_Delivery_System_Descriptor",     0x43, priv);
	ADD_DESCRIPTOR("Bouquet_Name_Descriptor",                  0x47, priv);
	ADD_DESCRIPTOR("Service_Descriptor",                       0x48, priv);
	ADD_DESCRIPTOR("Country_Availability_Descriptor",          0x49, priv);
	ADD_DESCRIPTOR("Linkage_Descriptor",                       0x4a, priv);
	ADD_DESCRIPTOR("NVOD_Reference_Descriptor",                0x4b, priv);
	ADD_DESCRIPTOR("Time_Shifted_Service_Descriptor",          0x4c, priv);
	ADD_DESCRIPTOR("Short_Event_Descriptor",                   0x4d, priv);
	ADD_DESCRIPTOR("Extended_Event_Descriptor",                0x4e, priv);
	ADD_DESCRIPTOR("Time_Shifted_Event_Descriptor",            0x4f, priv);
	ADD_DESCRIPTOR("Component_Descriptor",                     0x50, priv);
	ADD_DESCRIPTOR("Mosaic_Descriptor",                        0x51, priv);
	ADD_DESCRIPTOR("Stream_Identifier_Descriptor",             0x52, priv);
	ADD_DESCRIPTOR("CA_Identifier_Descriptor",                 0x53, priv);
	ADD_DESCRIPTOR("Content_Descriptor",                       0x54, priv);
	ADD_DESCRIPTOR("Parental_Rating_Descriptor",               0x55, priv);
	ADD_DESCRIPTOR("Local_Time_Offset_Descriptor",             0x58, priv);
	ADD_DESCRIPTOR("Partial_Transport_Stream_Descriptor",      0x63, priv);
	ADD_DESCRIPTOR("AAC_Audio_Descriptor",                     0x7c, priv);
	/* 0x80 - 0xBF - Reserved for identification of companies */
	ADD_DESCRIPTOR("FS_Metadata_Descriptor",                   0xa0, priv);
	ADD_DESCRIPTOR("Hierarchical_Transmission_Descriptor",     0xc0, priv);
	ADD_DESCRIPTOR("Digital_Copy_Control_Descriptor",          0xc1, priv);
	ADD_DESCRIPTOR("Network_Identifier_Descriptor",            0xc2, priv);
	ADD_DESCRIPTOR("Partial_Transport_Stream_Time_Descriptor", 0xc3, priv);
	ADD_DESCRIPTOR("Audio_Component_Descriptor",               0xc4, priv);
	ADD_DESCRIPTOR("Hyperlink_Descriptor",                     0xc5, priv);
	ADD_DESCRIPTOR("Target_Area_Descriptor",                   0xc6, priv);
	ADD_DESCRIPTOR("Data_Contents_Descriptor",                 0xc7, priv);
	ADD_DESCRIPTOR("Video_Decode_Control_Descriptor",          0xc8, priv);
	ADD_DESCRIPTOR("Download_Content_Descriptor",              0xc9, priv);
	ADD_DESCRIPTOR("CA_EMM_TS_Descriptor",                     0xca, priv);
	ADD_DESCRIPTOR("CA_Contract_Information_Descriptor",       0xcb, priv);
	ADD_DESCRIPTOR("CA_Service_Descriptor",                    0xcc, priv);
	ADD_DESCRIPTOR("TS_Information_Descriptor",                0xcd, priv);
	ADD_DESCRIPTOR("Extended_Broadcaster_Descriptor",          0xce, priv);
	ADD_DESCRIPTOR("Logo_Transmission_Descriptor",             0xcf, priv);
	ADD_DESCRIPTOR("Basic_Local_Event_Descriptor",             0xd0, priv);
	ADD_DESCRIPTOR("Reference_Descriptor",                     0xd1, priv);
	ADD_DESCRIPTOR("Node_Relation_Descriptor",                 0xd2, priv);
	ADD_DESCRIPTOR("Short_Node_Information_Descriptor",        0xd3, priv);
	ADD_DESCRIPTOR("STC_Reference_Descriptor",                 0xd4, priv);
	ADD_DESCRIPTOR("Series_Descriptor",                        0xd5, priv);
	ADD_DESCRIPTOR("Event_Group_Descriptor",                   0xd6, priv);
	ADD_DESCRIPTOR("SI_Parameter_Descriptor",                  0xd7, priv);
	ADD_DESCRIPTOR("Broadcaster_Name_Descriptor",              0xd8, priv);
	ADD_DESCRIPTOR("Component_Group_Descriptor",               0xd9, priv);
	ADD_DESCRIPTOR("SI_Prime_TS_Descriptor",                   0xda, priv);
	ADD_DESCRIPTOR("Board_Information_Descriptor",             0xdb, priv);
	ADD_DESCRIPTOR("LDT_Linkage_Descriptor",                   0xdc, priv);
	ADD_DESCRIPTOR("Connected_Transmission_Descriptor",        0xdd, priv);
	ADD_DESCRIPTOR("Content_Availability_Descriptor",          0xde, priv);
	ADD_DESCRIPTOR("Service_Group_Descriptor",                 0xe0, priv);
	/* 0xe1 - 0xF6 - Not defined */
	ADD_DESCRIPTOR("Carousel_Compatible_Composite_Descriptor", 0xf7, priv);
	ADD_DESCRIPTOR("Conditional_Playback_Descriptor",          0xf8, priv);
	ADD_DESCRIPTOR("Terrestrial_Delivery_System_Descriptor",   0xfa, priv);
	ADD_DESCRIPTOR("Partial_Reception_Descriptor",             0xfb, priv);
	ADD_DESCRIPTOR("Emergency_Information_Descriptor",         0xfc, priv);
	ADD_DESCRIPTOR("Data_Component_Descriptor",                0xfd, priv);
	ADD_DESCRIPTOR("System_Management_Descriptor",             0xfe, priv);
	ADD_DESCRIPTOR("User_Private_Descriptor",                  0xff, priv);
	return priv->ts_descriptors;
}
