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
#include "hash.h"
#include "fifo.h"
#include "ts.h"
#include "tables/psi.h"
#include "dsm-cc/dsmcc.h"
#include "dsm-cc/dii.h"
#include "dsm-cc/ddb.h"
#include "dsm-cc/ait.h"

void dsmcc_create_download_data_header_dentries(struct dsmcc_download_data_header *data_header, struct dentry *parent)
{
	CREATE_FILE_NUMBER(parent, data_header, protocol_discriminator);
	CREATE_FILE_NUMBER(parent, data_header, dsmcc_type);
	CREATE_FILE_NUMBER(parent, data_header, message_id);
	CREATE_FILE_NUMBER(parent, data_header, download_id);
	CREATE_FILE_NUMBER(parent, data_header, adaptation_length);
	CREATE_FILE_NUMBER(parent, data_header, message_length);
	if (data_header->adaptation_length) {
		struct dsmcc_adaptation_header *adaptation_header = &data_header->dsmcc_adaptation_header;
		CREATE_FILE_NUMBER(parent, adaptation_header, adaptation_type);
		CREATE_FILE_BIN(parent, adaptation_header, adaptation_data_bytes, data_header->adaptation_length);
	}
}

void dsmcc_create_message_header_dentries(struct dsmcc_message_header *msg_header, struct dentry *parent)
{
	CREATE_FILE_NUMBER(parent, msg_header, protocol_discriminator);
	CREATE_FILE_NUMBER(parent, msg_header, dsmcc_type);
	CREATE_FILE_NUMBER(parent, msg_header, message_id);
	CREATE_FILE_NUMBER(parent, msg_header, transaction_id);
	CREATE_FILE_NUMBER(parent, msg_header, adaptation_length);
	CREATE_FILE_NUMBER(parent, msg_header, message_length);
	if (msg_header->adaptation_length) {
		struct dsmcc_adaptation_header *adaptation_header = &msg_header->dsmcc_adaptation_header;
		CREATE_FILE_NUMBER(parent, adaptation_header, adaptation_type);
		CREATE_FILE_BIN(parent, adaptation_header, adaptation_data_bytes, msg_header->adaptation_length);
	}
	if ((msg_header->transaction_id & 0x80000000) != 0x80000000)
		TS_WARNING("transaction_id originator != '10' (%#x)", msg_header->transaction_id);
}

void dsmcc_create_compatibility_descriptor_dentries(struct dsmcc_compatibility_descriptor *cd, struct dentry *parent)
{
	CREATE_FILE_NUMBER(parent, cd, compatibility_descriptor_length);
	CREATE_FILE_NUMBER(parent, cd, descriptor_count);
	for (uint16_t i=0; i<cd->descriptor_count; ++i) {
		struct dentry *subdir = CREATE_DIRECTORY(parent, "descriptor_%02d", i+1);
		CREATE_FILE_NUMBER(subdir, &cd->descriptors[i], descriptor_type);
		CREATE_FILE_NUMBER(subdir, &cd->descriptors[i], descriptor_length);
		CREATE_FILE_NUMBER(subdir, &cd->descriptors[i], specifier_type);
		CREATE_FILE_BIN(subdir, &cd->descriptors[i], specifier_data, 3);
		CREATE_FILE_NUMBER(subdir, &cd->descriptors[i], model);
		CREATE_FILE_NUMBER(subdir, &cd->descriptors[i], version);
		CREATE_FILE_NUMBER(subdir, &cd->descriptors[i], sub_descriptor_count);
		for (uint8_t k=0; k<cd->descriptors[i].sub_descriptor_count; ++k) {
			struct dentry *dentry = CREATE_DIRECTORY(subdir, "sub_descriptor_%02d", k+1);
			struct dsmcc_sub_descriptor *sub = &cd->descriptors[i].sub_descriptors[k];
			CREATE_FILE_NUMBER(dentry, sub, sub_descriptor_type);
			CREATE_FILE_NUMBER(dentry, sub, sub_descriptor_length);
			if (sub->sub_descriptor_length)
				CREATE_FILE_BIN(dentry, sub, additional_information, sub->sub_descriptor_length);
		}
	}
}

void dsmcc_free_compatibility_descriptors(struct dsmcc_compatibility_descriptor *cd)
{
	for (uint16_t n=0; n<cd->descriptor_count; ++n) {
		if (cd->descriptors[n].sub_descriptors)
			free(cd->descriptors[n].sub_descriptors);
		for (uint8_t k=0; k<cd->descriptors[n].sub_descriptor_count; ++k) {
			struct dsmcc_sub_descriptor *sub = &cd->descriptors[n].sub_descriptors[k];
			if (sub->additional_information)
				free(sub->additional_information);
		}
	}
	if (cd->descriptors)
		free(cd->descriptors);
}

int dsmcc_parse_compatibility_descriptors(struct dsmcc_compatibility_descriptor *cd,
		const char *payload, int index)
{
	int i = index;

	cd->compatibility_descriptor_length = CONVERT_TO_16(payload[i], payload[i+1]);
	if (cd->compatibility_descriptor_length < 2) {
		cd->descriptor_count = 0;
		return index + cd->compatibility_descriptor_length + 1;
	}
	
	cd->descriptor_count = CONVERT_TO_16(payload[i+2], payload[i+3]);
	i += 4;
	if (cd->descriptor_count)
		cd->descriptors = calloc(cd->descriptor_count, sizeof(struct dsmcc_descriptor_entry));
	for (uint16_t n=0; n<cd->descriptor_count; ++n) {
		cd->descriptors[n].descriptor_type = payload[i];
		cd->descriptors[n].descriptor_length = payload[i+1];
		cd->descriptors[n].specifier_type = payload[i+2];
		cd->descriptors[n].specifier_data[0] = payload[i+3];
		cd->descriptors[n].specifier_data[1] = payload[i+4];
		cd->descriptors[n].specifier_data[2] = payload[i+5];
		cd->descriptors[n].model = CONVERT_TO_16(payload[i+6], payload[i+7]);
		cd->descriptors[n].version = CONVERT_TO_16(payload[i+8], payload[i+9]);
		cd->descriptors[n].sub_descriptor_count = payload[i+10];
		if (cd->descriptors[n].sub_descriptor_count)
			cd->descriptors[n].sub_descriptors = calloc(cd->descriptors[n].sub_descriptor_count, 
					sizeof(struct dsmcc_sub_descriptor));
		
		i += 11;
		for (uint8_t k=0; k<cd->descriptors[n].sub_descriptor_count; ++k) {
			struct dsmcc_sub_descriptor *sub = &cd->descriptors[n].sub_descriptors[k];
			sub->sub_descriptor_type = payload[i];
			sub->sub_descriptor_length = payload[i+1];
			i += 2;
			if (sub->sub_descriptor_length)
				sub->additional_information = malloc(sub->sub_descriptor_length);
			for (uint8_t l=0; l<sub->sub_descriptor_length; ++l) {
				sub->additional_information[l] = payload[i+2+l];
				i++;
			}
		}
	}
	return i;
}

void dsmcc_free_message_header(struct dsmcc_message_header *msg_header)
{
	struct dsmcc_adaptation_header *adaptation_header = &msg_header->dsmcc_adaptation_header;
	if (adaptation_header->adaptation_data_bytes)
		free(adaptation_header->adaptation_data_bytes);
}

int dsmcc_parse_message_header(struct dsmcc_message_header *msg_header, 
		const char *payload, int index)
{
	int i = index;
	msg_header->protocol_discriminator = payload[i];
	msg_header->dsmcc_type = payload[i+1];
	msg_header->message_id = CONVERT_TO_16(payload[i+2], payload[i+3]);
	msg_header->transaction_id = CONVERT_TO_32(payload[i+4], payload[i+5], payload[i+6], payload[i+7]);
	msg_header->reserved = payload[i+8];
	msg_header->adaptation_length = payload[i+9];
	msg_header->message_length = CONVERT_TO_16(payload[i+10], payload[i+11]);
	i += 12;

	if (msg_header->adaptation_length) {
		struct dsmcc_adaptation_header *adaptation_header = &msg_header->dsmcc_adaptation_header;
		adaptation_header->adaptation_type = payload[i++];
		adaptation_header->adaptation_data_bytes = malloc(msg_header->adaptation_length);
		for (uint16_t j=0; j<msg_header->adaptation_length; ++j, ++i)
			adaptation_header->adaptation_data_bytes[j] = payload[i+j];
	}
	return i;
}

void dsmcc_free_download_data_header(struct dsmcc_download_data_header *data_header)
{
	struct dsmcc_adaptation_header *adaptation_header = &data_header->dsmcc_adaptation_header;
	if (adaptation_header->adaptation_data_bytes)
		free(adaptation_header->adaptation_data_bytes);
}

int dsmcc_parse_download_data_header(struct dsmcc_download_data_header *data_header,
		const char *payload, int index)
{
	int i = index;
	data_header->protocol_discriminator = payload[i];
	data_header->dsmcc_type = payload[i+1];
	data_header->message_id = CONVERT_TO_16(payload[i+2], payload[i+3]);
	data_header->download_id = CONVERT_TO_32(payload[i+4], payload[i+5], payload[i+6], payload[i+7]);
	data_header->reserved = payload[i+8];
	data_header->adaptation_length = payload[i+9];
	data_header->message_length = CONVERT_TO_16(payload[i+10], payload[i+11]);
	i += 12;

	if (data_header->adaptation_length) {
		struct dsmcc_adaptation_header *adaptation_header = &data_header->dsmcc_adaptation_header;
		adaptation_header->adaptation_type = payload[i++];
		adaptation_header->adaptation_data_bytes = malloc(data_header->adaptation_length);
		for (uint16_t j=0; j<data_header->adaptation_length; ++j, ++i)
			adaptation_header->adaptation_data_bytes[j] = payload[i+j];
	}
	return i;
}

int dsmcc_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	uint8_t table_id = payload[0];

	if (table_id == TS_AIT_TABLE_ID)
		return ait_parse(header, payload, payload_len, priv);
	else if (table_id == TS_DII_TABLE_ID)
		return dii_parse(header, payload, payload_len, priv);
	else if (table_id == TS_DDB_TABLE_ID)
		return ddb_parse(header, payload, payload_len, priv);

	return 0;
}

