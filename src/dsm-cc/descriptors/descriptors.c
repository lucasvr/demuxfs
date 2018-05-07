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
#include "descriptors.h"
#include "ts.h"

uint32_t dsmcc_descriptors_parse(const char *payload, uint8_t num_descriptors, 
		struct dentry *parent, struct demuxfs_data *priv)
{
	int ret;
	uint8_t n;
	uint32_t offset = 0;
	for (n=0; n<num_descriptors; ++n) {
		uint8_t descriptor_tag = payload[offset];
		uint8_t descriptor_length = payload[offset+1];
		struct dsmcc_descriptor *d = dsmcc_descriptors_find(descriptor_tag, priv);
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

bool dsmcc_descriptor_is_parseable(struct dentry *dentry, uint8_t tag, int expected, int found)
{
	if (found == expected)
		return true;
	else if (found < expected) {
		TS_WARNING("Tag %#0x could not be parsed: descriptor size mismatch (expected %d "
				"bytes, found %d)", tag, expected, found);
		return false;
	} else {
		TS_WARNING("Tag %#0x: descriptor size mismatch (expected %d bytes, found %d)", 
				tag, expected, found);
		return true;
	}
}

struct dsmcc_descriptor *dsmcc_descriptors_find(uint8_t tag, struct demuxfs_data *priv)
{
	struct dsmcc_descriptor *d = &priv->dsmcc_descriptors[tag];
	return d->parser ? d : NULL;
}

#define ADD_DESCRIPTOR(dname,dtag,priv) \
	do { \
		struct dsmcc_descriptor *d = &priv->dsmcc_descriptors[dtag]; \
		d->tag = dtag; \
		d->name = strdup(dname); \
		d->parser = dsmcc_descriptor_ ## dtag ## _parser; \
	} while(0)

struct dsmcc_descriptor *dsmcc_descriptors_init(struct demuxfs_data *priv)
{
	uint8_t tag;
	priv->dsmcc_descriptors = (struct dsmcc_descriptor *) calloc(0xff+1, sizeof(struct dsmcc_descriptor));

	/* DSM-CC descriptors and their tag values, defined by ABNT 15606-3 */
	ADD_DESCRIPTOR("Application_Descriptor",             0x00, priv);
	ADD_DESCRIPTOR("Type_Descriptor",                    0x01, priv);
	ADD_DESCRIPTOR("Name_Descriptor",                    0x02, priv);
	ADD_DESCRIPTOR("Info_Descriptor",                    0x03, priv);
	ADD_DESCRIPTOR("Module_Link_Descriptor",             0x04, priv);
	ADD_DESCRIPTOR("CRC-32_Descriptor",                  0x05, priv);
	ADD_DESCRIPTOR("Location_Descriptor",                0x06, priv);
	ADD_DESCRIPTOR("Estimated_Download_Time_Descriptor", 0x07, priv);
	ADD_DESCRIPTOR("Compression_Type_Descriptor",        0xc2, priv);
	for (tag=0x80; tag<=0xbf; ++tag) {
		struct dsmcc_descriptor *d = &priv->dsmcc_descriptors[tag];
		d->tag = tag;
		d->name = strdup("Reserved_For_Broadcasters");
		d->parser = dsmcc_descriptor_broadcaster_parser;
	}
	return priv->dsmcc_descriptors;
}

void dsmcc_descriptors_destroy(struct dsmcc_descriptor *descriptor_list)
{
	int i;
	if (descriptor_list) {
		for (i=0; i<0xff+1; ++i)
			if (descriptor_list[i].name)
				free(descriptor_list[i].name);
		free(descriptor_list);
	}
}
