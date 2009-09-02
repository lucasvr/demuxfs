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
#include "biop.h"
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
		/* TODO: Checksum */
	} else {
		/* TODO: CRC 32 */
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
		biop_create_module_info_dentries(subdir, mod->module_info);
	}

	CREATE_FILE_NUMBER(parent, dii, private_data_length);
	if (dii->private_data_length)
		CREATE_FILE_BIN(parent, dii, private_data_bytes, dii->private_data_length);
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

static bool dii_module_complete(struct dii_table *dii, int module, struct dentry *ddb_dentry)
{
	char mod_dir[64];
	struct dentry *mod_dentry;
	struct dii_module *mod = &dii->modules[module];

	if (mod->module_size == 0)
		return true;

	sprintf(mod_dir, "/module_%02d", mod->module_id);
	mod_dentry = fsutils_get_dentry(ddb_dentry, mod_dir);
	if (! mod_dentry || mod_dentry->size != mod->module_size)
		return false;

	return true;
}

static bool dii_download_complete(const struct ts_header *header, struct dii_table *dii, 
		struct demuxfs_data *priv)
{
	char buf[PATH_MAX];
	struct dentry *ddb_dentry;
	
	snprintf(buf, sizeof(buf), "/%s/%#04x", FS_DDB_NAME, header->pid);
	ddb_dentry = fsutils_get_dentry(priv->root, buf);
	if (! ddb_dentry) {
		/* XXX: maybe it's under another PID? */
		return false;
	}
	ddb_dentry = fsutils_get_current(ddb_dentry);
	assert(ddb_dentry);

	for (uint16_t i=0; i<dii->number_of_modules; ++i)
		if (! dii_module_complete(dii, i, ddb_dentry))
			return false;

	return true;
}

int dii_create_filesystem(const struct ts_header *header, struct dii_table *dii, 
	struct demuxfs_data *priv)
{
	char buf[PATH_MAX], mod_dir[64], block_dir[64];
	struct dentry *ddb_dentry, *dsmcc_dentry, *ait_dentry, *app_dentry = NULL;

	dprintf("*** Creating filesystem ***");
	dii->_filesystem_created = true;
	
	snprintf(buf, sizeof(buf), "/%s/%#04x", FS_DDB_NAME, header->pid);
	ddb_dentry = fsutils_get_dentry(priv->root, buf);
	assert(ddb_dentry);

	ddb_dentry = fsutils_get_current(ddb_dentry);
	assert(ddb_dentry);

	dsmcc_dentry = CREATE_DIRECTORY(priv->root, FS_DSMCC_NAME);
	assert(dsmcc_dentry);

	/* Try to get the application name from the AIT */
	snprintf(buf, sizeof(buf), "/%s", FS_AIT_NAME);
	ait_dentry = fsutils_get_dentry(priv->root, buf);
	if (ait_dentry) {
		ait_dentry = fsutils_get_current(ait_dentry);
		assert(ait_dentry);
		for (uint16_t i=1; i < UINT16_MAX; ++i) {
			sprintf(buf, "/Application_%02d", i);
			ait_dentry = fsutils_get_dentry(ait_dentry, buf);
			if (! ait_dentry)
				break;

			sprintf(buf, "/APPLICATION_NAME/APPLICATION_NAME_01/application_name");
			ait_dentry = fsutils_get_dentry(ait_dentry, buf);
			if (ait_dentry) {
				app_dentry = CREATE_DIRECTORY(dsmcc_dentry, ait_dentry->contents);
				break;
			}
		}
	}
	if (! app_dentry)
		app_dentry = CREATE_DIRECTORY(dsmcc_dentry, FS_UNNAMED_APPLICATION_NAME);

	/* For each module, get all of its blocks and expose their virtual filesystem */
	struct dentry stepfather_dentry;
	memset(&stepfather_dentry, 0, sizeof(stepfather_dentry));
	INIT_LIST_HEAD(&stepfather_dentry.children);

	for (uint16_t i=0; i<dii->number_of_modules; ++i) {
		struct dentry *mod_dentry, *block_dentry;
		struct dii_module *mod = &dii->modules[i];

		if (mod->module_size == 0)
			continue;

		sprintf(mod_dir, "/module_%02d", mod->module_id);
		mod_dentry = fsutils_get_dentry(ddb_dentry, mod_dir);
		assert(mod_dentry);
		
		int remaining = (mod->module_size % dii->block_size) ? 1 : 0;
		uint32_t block_count = mod->module_size / dii->block_size + remaining;
		uint32_t blocks_parsed = 0;

		char *download_data = malloc(block_count * dii->block_size);
		char *download_ptr = download_data;
		assert(download_data);

		for (uint16_t b=0; b < UINT16_MAX && blocks_parsed < block_count; ++b) {
			sprintf(block_dir, "/block_%02d.bin", b);
			block_dentry = fsutils_get_dentry(mod_dentry, block_dir);
			if (! block_dentry)
				continue;
			memcpy(download_ptr, block_dentry->contents, block_dentry->size);
			download_ptr += block_dentry->size;
			blocks_parsed++;
		}

		/* Parse blocks and create filesystem entries */
		uint32_t download_len = download_ptr-download_data+1;
		biop_create_filesystem_dentries(app_dentry, &stepfather_dentry, 
			download_data, download_len);
		free(download_data);
	}
	biop_reparent_orphaned_dentries(app_dentry, &stepfather_dentry);

	return 0;
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
		if (current_dii && !current_dii->_filesystem_created && dii_download_complete(header, current_dii, priv))
			dii_create_filesystem(header, current_dii, priv);
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

	if (dii->number_of_modules) {
		dii->modules = calloc(dii->number_of_modules, sizeof(struct dii_module));
		for (uint16_t i=0; i<dii->number_of_modules; ++i) {
			struct dii_module *mod = &dii->modules[i];
			mod->module_id = CONVERT_TO_16(payload[j], payload[j+1]);
			mod->module_size = CONVERT_TO_32(payload[j+2], payload[j+3], payload[j+4], payload[j+5]);
			mod->module_version = payload[j+6];
			mod->module_info_length = payload[j+7];
			j += 8;
			if (mod->module_info_length) {
				mod->module_info = calloc(1, sizeof(struct biop_module_info));
				j += biop_parse_module_info(mod->module_info, &payload[j], payload_len-j);
			}
		}
	}
	
	dii->private_data_length = CONVERT_TO_16(payload[j], payload[j+1]);
	if (dii->private_data_length) {
		dii->private_data_bytes = malloc(dii->private_data_length);
		for (uint16_t i=0; i<dii->private_data_length; ++i)
			dii->private_data_bytes[i] = payload[j+2+i];
	}
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
