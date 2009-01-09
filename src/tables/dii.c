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
#include "descriptors.h"
#include "stream_type.h"
#include "component_tag.h"
#include "tables/dsmcc.h"
#include "tables/psi.h"
#include "tables/dii.h"
#include "tables/pes.h"
#include "tables/pat.h"

static void dii_check_header(struct dii_table *dii)
{
}

static void dii_create_directory(const struct ts_header *header, struct dii_table *dii, 
		struct dentry **version_dentry, struct demuxfs_data *priv)
{
	/* Create a directory named "DII" at the root filesystem if it doesn't exist yet */
	struct dentry *dii_dir = CREATE_DIRECTORY(priv->root, FS_DII_NAME);

	/* Create a directory named "<dii_pid>" and populate it with files */
	asprintf(&dii->dentry->name, "%#04x", header->pid);
	dii->dentry->mode = S_IFDIR | 0555;
	CREATE_COMMON(dii_dir, dii->dentry);

	/* Create the versioned dir and update the Current symlink */
	*version_dentry = fsutils_create_version_dir(dii->dentry, dii->version_number);

	psi_populate((void **) &dii, *version_dentry);
	//dii_populate(dii, *version_dentry, priv);
}

int dii_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct dii_table *current_dii = NULL;
	struct dii_table *dii = (struct dii_table *) calloc(1, sizeof(struct dii_table));
	assert(dii);

	dii->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(dii->dentry);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) dii, payload, payload_len);
	if (ret < 0) {
		free(dii->dentry);
		free(dii);
		return 0;
	}
	dii_check_header(dii);

	/* Set hash key and check if there's already one version of this table in the hash */
	dii->dentry->inode = TS_PACKET_HASH_KEY(header, dii);
	current_dii = hashtable_get(priv->psi_tables, dii->dentry->inode);

	/* Check whether we should keep processing this packet or not */
	if (! dii->current_next_indicator || (current_dii && current_dii->version_number == dii->version_number)) {
		free(dii->dentry);
		free(dii);
		return 0;
	}

	dprintf("*** DII parser: pid=%#x, table_id=%#x, current_dii=%p, dii->version_number=%#x, len=%d ***", 
			header->pid, dii->table_id, current_dii, dii->version_number, payload_len);

	if (payload_len < 20) {
		dprintf("payload is too small");
		free(dii->dentry);
		free(dii);
		return 0;
	}

	/** DSM-CC Message Header */
	struct dsmcc_message_header *msg_header = &dii->dsmcc_message_header;
	int j = dsmcc_parse_message_header(msg_header, payload, 8);

	if (msg_header->protocol_discriminator != 0x11 ||
		msg_header->dsmcc_type != 0x03 ||
		msg_header->message_id != 0x1002) {
		free(dii->dentry);
		free(dii);
		return 0;
	}

	/** At this point we know for sure that this is a DII table */ 
	struct dentry *version_dentry = NULL;
	dii_create_directory(header, dii, &version_dentry, priv);

	dsmcc_create_message_header_dentries(msg_header, version_dentry);
	j += msg_header->adaptation_length;

	/** Parse DII bits */
	dii->download_id = CONVERT_TO_32(payload[j], payload[j+1], payload[j+2], payload[j+3]);
	dii->block_size = CONVERT_TO_16(payload[j+4], payload[j+5]);
	dii->window_size = payload[j+6];
	dii->ack_period = payload[j+7];
	dii->t_c_download_window = CONVERT_TO_32(payload[j+8], payload[j+9], payload[j+10], payload[j+11]);
	dii->t_c_download_scenario = CONVERT_TO_32(payload[j+12], payload[j+13], payload[j+14], payload[j+15]);

	CREATE_FILE_NUMBER(version_dentry, dii, download_id);
	CREATE_FILE_NUMBER(version_dentry, dii, block_size);
	CREATE_FILE_NUMBER(version_dentry, dii, window_size);
	CREATE_FILE_NUMBER(version_dentry, dii, ack_period);
	CREATE_FILE_NUMBER(version_dentry, dii, t_c_download_window);
	CREATE_FILE_NUMBER(version_dentry, dii, t_c_download_scenario);

	if (dii->block_size == 0)
		goto out;

	/** DSM-CC Compatibility Descriptor */
	struct dsmcc_compatibility_descriptor *cd = &dii->compatibility_descriptor;
	cd->compatibility_descriptor_length = CONVERT_TO_16(payload[j+16], payload[j+17]);
	cd->descriptor_count = CONVERT_TO_16(payload[j+18], payload[j+19]);
	j += 20;
	if (cd->descriptor_count)
		cd->descriptors = calloc(cd->descriptor_count, sizeof(struct dsmcc_descriptor_entry));
	for (uint16_t i=0; i<cd->descriptor_count; ++i) {
		cd->descriptors->descriptor_type = payload[j];
		cd->descriptors->descriptor_length = payload[j+1];
		cd->descriptors->specifier_type = payload[j+2];
		cd->descriptors->specifier_data[0] = payload[j+3];
		cd->descriptors->specifier_data[1] = payload[j+4];
		cd->descriptors->specifier_data[2] = payload[j+5];
		cd->descriptors->model = CONVERT_TO_16(payload[j+6], payload[j+7]);
		cd->descriptors->version = CONVERT_TO_16(payload[j+8], payload[j+9]);
		cd->descriptors->sub_descriptor_count = payload[j+10];
		if (cd->descriptors->sub_descriptor_count)
			cd->descriptors->sub_descriptors = calloc(cd->descriptors->sub_descriptor_count, 
					sizeof(struct dsmcc_sub_descriptor));
		j += 11;
		for (uint8_t k=0; k<cd->descriptors->sub_descriptor_count; ++k) {
			struct dsmcc_sub_descriptor *sub = &cd->descriptors->sub_descriptors[k];
			sub->sub_descriptor_type = payload[j];
			sub->sub_descriptor_length = payload[j+1];
			if (sub->sub_descriptor_length)
				sub->additional_information = malloc(sub->sub_descriptor_length);
			for (uint8_t l=0; l<sub->sub_descriptor_length; ++l)
				sub->additional_information[l] = payload[j+2+l];
			j += 2 + sub->sub_descriptor_length;
		}
	}
	dsmcc_create_compatibility_descriptor_dentries(cd, version_dentry);
	
	/** DII bits */
	dii->number_of_modules = CONVERT_TO_16(payload[j], payload[j+1]);
	CREATE_FILE_NUMBER(version_dentry, dii, number_of_modules);
	j += 2;

	if (dii->number_of_modules)
		dii->modules = calloc(dii->number_of_modules, sizeof(struct dii_module));
	for (uint16_t i=0; i<dii->number_of_modules; ++i) {
		char dir_name[64];
		sprintf(dir_name, "module_%02d", i+1);
		struct dii_module *mod = &dii->modules[i];
		struct dentry *subdir = CREATE_DIRECTORY(version_dentry, dir_name);

		mod->module_id = CONVERT_TO_16(payload[j], payload[j+1]);
		mod->module_size = CONVERT_TO_32(payload[j+2], payload[j+3], payload[j+4], payload[j+5]);
		mod->module_version = payload[j+6];
		mod->module_info_length = payload[j+7];
		if (mod->module_info_length)
			mod->module_info_bytes = malloc(mod->module_info_length);
		for (uint8_t k=0; k<mod->module_info_length; ++k)
			mod->module_info_bytes[k] = payload[j+8+k];

		CREATE_FILE_NUMBER(subdir, mod, module_id);
		CREATE_FILE_NUMBER(subdir, mod, module_size);
		CREATE_FILE_NUMBER(subdir, mod, module_version);
		CREATE_FILE_NUMBER(subdir, mod, module_info_length);
		if (mod->module_info_length)
			CREATE_FILE_BIN(subdir, mod, module_info_bytes, mod->module_info_length);

		j += 8 + mod->module_info_length;
	}
	
	dii->private_data_length = CONVERT_TO_16(payload[j], payload[j+1]);
	if (dii->private_data_length)
		dii->private_data_bytes = malloc(dii->private_data_length);
	for (uint16_t i=0; i<dii->private_data_length; ++i)
		dii->private_data_bytes[i] = payload[j+2+i];

	CREATE_FILE_NUMBER(version_dentry, dii, private_data_length);
	if (dii->private_data_length)
		CREATE_FILE_BIN(version_dentry, dii, private_data_bytes, dii->private_data_length);

	j += 2 + dii->private_data_length;
	
	if (dii->window_size != 0)
		TS_WARNING("dii->window_size != 0 (%#x)", dii->window_size);
	if (dii->ack_period != 0)
		TS_WARNING("dii->ack_period != 0 (%#x)", dii->ack_period);
	if (dii->t_c_download_window != 0)
		TS_WARNING("dii->t_c_download_window != 0 (%#x)", dii->t_c_download_window);

out:
	if (current_dii) {
		hashtable_del(priv->psi_tables, current_dii->dentry->inode);
		fsutils_migrate_children(current_dii->dentry, dii->dentry);
		fsutils_dispose_tree(current_dii->dentry);
		free(current_dii);
	}
	hashtable_add(priv->psi_tables, dii->dentry->inode, dii);

	return 0;
}
