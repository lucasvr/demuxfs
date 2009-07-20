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
#include "list.h"
#include "ts.h"
#include "tables/psi.h"
#include "dsm-cc/dsmcc.h"
#include "dsm-cc/dii.h"
#include "dsm-cc/dsi.h"
#include "dsm-cc/descriptors/descriptors.h"

static void dii_free(struct dii_table *dii)
{
	/* Free DSM-CC compatibility descriptors */
	dsmcc_free_compatibility_descriptors(&dii->compatibility_descriptor);
	/* Free DSM-CC message header */
	dsmcc_free_message_header(&dii->dsmcc_message_header);
	/* Free the dentry and its subtree */
	if (dii->dentry && dii->dentry->name)
		fsutils_dispose_tree(dii->dentry);
	/* Free the dii table structure */
	free(dii);
}

static void dii_check_header(struct dii_table *dii)
{
	if (dii->section_syntax_indicator == 0) {
		/* Checksum */
		TS_WARNING("DII contains a Checksum");
	} else {
		/* CRC 32 */
		TS_WARNING("DII contains a CRC-32");
	}
	if (dii->section_length > 4093)
		TS_WARNING("section_length exceeds 4093 bytes (%d)", dii->section_length);
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
	*version_dentry = fsutils_create_version_dir(dii->dentry, dii->download_id);

	psi_populate((void **) &dii, *version_dentry);
}

static void dii_create_dentries(struct dentry *parent, struct dii_table *dii, struct demuxfs_data *priv)
{
	struct dsmcc_message_header *msg_header = &dii->dsmcc_message_header;
	struct dsmcc_compatibility_descriptor *cd = &dii->compatibility_descriptor;

	dsmcc_create_message_header_dentries(msg_header, parent);
	if (dii->private_data_length)
		dsmcc_create_compatibility_descriptor_dentries(cd, parent);

	CREATE_FILE_NUMBER(parent, dii, download_id);
	CREATE_FILE_NUMBER(parent, dii, block_size);
	CREATE_FILE_NUMBER(parent, dii, window_size);
	CREATE_FILE_NUMBER(parent, dii, ack_period);
	CREATE_FILE_NUMBER(parent, dii, t_c_download_window);
	CREATE_FILE_NUMBER(parent, dii, t_c_download_scenario);

	if (dii->window_size != 0)
		TS_WARNING("window_size != 0 (%d)", dii->window_size);
	if (dii->ack_period != 0)
		TS_WARNING("ack_period != 0 (%d)", dii->ack_period);
	if (dii->t_c_download_window != 0)
		TS_WARNING("t_c_download_window != 0 (%d)", dii->t_c_download_window);
	
	CREATE_FILE_NUMBER(parent, dii, number_of_modules);
	for (uint16_t i=0; i<dii->number_of_modules; ++i) {
		struct dentry *subdir = CREATE_DIRECTORY(parent, "module_%02d", i+1);
		struct dii_module *mod = &dii->modules[i];
		CREATE_FILE_NUMBER(subdir, mod, module_id);
		CREATE_FILE_NUMBER(subdir, mod, module_size);
		CREATE_FILE_NUMBER(subdir, mod, module_version);
		CREATE_FILE_NUMBER(subdir, mod, module_info_length);
		if (mod->module_info_length) {
			uint8_t len;
			uint16_t offset = 0;
			while (offset < mod->module_info_length) {
				len = dsmcc_descriptors_parse(&mod->module_info_bytes[offset], 1, subdir, priv);
				offset += len + 2;
			}
		}
	}

	CREATE_FILE_NUMBER(parent, dii, private_data_length);
#if 0
	if (dii->private_data_length) {
		uint8_t len;
		uint16_t offset = 0;
		while (offset < dii->private_data_length) {
			len = dsmcc_descriptors_parse(&dii->private_data_bytes[offset], 1, parent, priv);
			offset += len + 2;
		}
	}
#endif
}

int dii_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct dii_table *dii, *current_dii = NULL;
	
	if (payload_len < 20) {
		dprintf("payload is too small (%d)", payload_len);
		return 0;
	}
	
	dii = (struct dii_table *) calloc(1, sizeof(struct dii_table));
	assert(dii);

	dii->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(dii->dentry);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) dii, payload, payload_len);
	if (ret < 0) {
		dprintf("error parsing PSI packet");
		dii_free(dii);
		return 0;
	}
	dii_check_header(dii);

	/** DSM-CC Message Header */
	struct dsmcc_message_header *msg_header = &dii->dsmcc_message_header;
	int j = dsmcc_parse_message_header(msg_header, payload, 8);
	
	/* Check whether we should keep processing this packet or not */
	if (msg_header->protocol_discriminator != 0x11 || msg_header->_dsmcc_type != 0x03) {
		dprintf("protocol_discriminator=%#x, dsmcc_type=%#x: not a U-N message, bailing out", 
				msg_header->protocol_discriminator,
				msg_header->_dsmcc_type);
		dii_free(dii);
		return 0;
	}

	if (msg_header->_message_id == 0x1006) {
		/* DSM-CC Download Server Initiate. Proceed with the DSI parser. */
		dii_free(dii);
		return dsi_parse(header, payload, payload_len, priv);
	} else if (msg_header->_message_id != 0x1002) {
		dii_free(dii);
		return 0;
	}

	/** At this point we know for sure that this is a DII table */ 
	dii->dentry->inode = TS_PACKET_HASH_KEY(header, dii);
	current_dii = hashtable_get(priv->psi_tables, dii->dentry->inode);
	if (! dii->current_next_indicator || (current_dii && current_dii->version_number == dii->version_number)) {
		dii_free(dii);
		return 0;
	}

	dprintf("*** DII parser: pid=%#x, table_id=%#x, dii->version_number=%#x, transaction_nr=%#x ***", 
			header->pid, dii->table_id, dii->version_number, msg_header->transaction_id & ~0x80000000);

	/** Parse DII bits */
	dii->download_id = CONVERT_TO_32(payload[j], payload[j+1], payload[j+2], payload[j+3]);
	dii->block_size = CONVERT_TO_16(payload[j+4], payload[j+5]);
	dii->window_size = payload[j+6];
	dii->ack_period = payload[j+7];
	dii->t_c_download_window = CONVERT_TO_32(payload[j+8], payload[j+9], payload[j+10], payload[j+11]);
	dii->t_c_download_scenario = CONVERT_TO_32(payload[j+12], payload[j+13], payload[j+14], payload[j+15]);

	if (dii->block_size == 0) {
		dii_free(dii);
		return 0;
	}

	/** DSM-CC Compatibility Descriptor */
	struct dsmcc_compatibility_descriptor *cd = &dii->compatibility_descriptor;
	j = dsmcc_parse_compatibility_descriptors(cd, payload, j+16);
	
	/** DII bits */
	dii->number_of_modules = CONVERT_TO_16(payload[j], payload[j+1]);
	j += 2;
	if (dii->number_of_modules)
		dii->modules = calloc(dii->number_of_modules, sizeof(struct dii_module));
	for (uint16_t i=0; i<dii->number_of_modules; ++i) {
		struct dii_module *mod = &dii->modules[i];
		mod->module_id = CONVERT_TO_16(payload[j], payload[j+1]);
		mod->module_size = CONVERT_TO_32(payload[j+2], payload[j+3], payload[j+4], payload[j+5]);
		mod->module_version = payload[j+6];
		mod->module_info_length = payload[j+7];
		if (mod->module_info_length)
			mod->module_info_bytes = malloc(mod->module_info_length);
		for (uint8_t k=0; k<mod->module_info_length; ++k)
			mod->module_info_bytes[k] = payload[j+8+k];
		j += 8 + mod->module_info_length;
	}
	
	dii->private_data_length = CONVERT_TO_16(payload[j], payload[j+1]);
	if (dii->private_data_length)
		dii->private_data_bytes = malloc(dii->private_data_length);
	for (uint16_t i=0; i<dii->private_data_length; ++i)
		dii->private_data_bytes[i] = payload[j+2+i];

	j += 2 + dii->private_data_length;

	/* Create filesystem entries for this table */
	struct dentry *version_dentry = NULL;
	dii_create_directory(header, dii, &version_dentry, priv);
	dii_create_dentries(version_dentry, dii, priv);

	if (current_dii) {
		hashtable_del(priv->psi_tables, current_dii->dentry->inode);
		fsutils_migrate_children(current_dii->dentry, dii->dentry);
		fsutils_dispose_tree(current_dii->dentry);
		free(current_dii);
	}
	hashtable_add(priv->psi_tables, dii->dentry->inode, dii);

	return 0;
}
