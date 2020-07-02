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
#include "byteops.h"
#include "fsutils.h"
#include "xattr.h"
#include "hash.h"
#include "fifo.h"
#include "ts.h"
#include "tables/psi.h"
#include "dsm-cc/dsmcc.h"
#include "dsm-cc/ddb.h"
#include "dsm-cc/dii.h"

void ddb_free(struct ddb_table *ddb)
{
	struct dsmcc_download_data_header *data_header;
	
	if (ddb->dentry && ddb->dentry->name) {
		data_header = &ddb->dsmcc_download_data_header;
		dsmcc_free_download_data_header(data_header);
		fsutils_dispose_tree(ddb->dentry);
	} else if (ddb->dentry)
		/* Dentry has simply been calloc'ed */
		free(ddb->dentry);

	free(ddb);
}

static bool ddb_block_number_already_parsed(struct ddb_table *current_ddb, 
	uint16_t module_id, uint16_t block_number)
{
	struct dentry *current_dentry, *module_dentry, *block_dentry;
	char dname[64], fname[64];

	if (! current_ddb)
		return false;

	current_dentry = fsutils_get_current(current_ddb->dentry);
	if (current_dentry) {
		sprintf(dname, "module_%02d", module_id);
		sprintf(fname, "block_%02d.bin", block_number);
		module_dentry = fsutils_get_child(current_dentry, dname);
		if (module_dentry) {
			block_dentry = fsutils_get_child(module_dentry, fname);
			return block_dentry ? true : false;
		}
	}

	return false;
}

static void ddb_check_header(struct ddb_table *ddb)
{
}

static void ddb_create_directory(const struct ts_header *header, struct ddb_table *ddb, 
		struct dentry **version_dentry, struct demuxfs_data *priv)
{
	/* Create a directory named "DDB" at the root filesystem if it doesn't exist yet */
	struct dentry *ddb_dir = CREATE_DIRECTORY(priv->root, FS_DDB_NAME);

	/* Create a directory named "<ddb_pid>" and populate it with files */
	asprintf(&ddb->dentry->name, "%#04x", header->pid);
	ddb->dentry->mode = S_IFDIR | 0555;
	CREATE_COMMON(ddb_dir, ddb->dentry);
	
	/* Create the versioned dir and update the Current symlink */
	*version_dentry = fsutils_create_version_dir(ddb->dentry, ddb->version_number);
}

int ddb_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct ddb_table *current_ddb = NULL;
	struct ddb_table *ddb = (struct ddb_table *) calloc(1, sizeof(struct ddb_table));
	assert(ddb);
	
	ddb->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(ddb->dentry);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) ddb, payload, payload_len);
	if (ret < 0) {
		ddb_free(ddb);
		return 0;
	}
	ddb_check_header(ddb);
	
	/* Set hash key and check if there's already one version of this table in the hash */
	ddb->dentry->inode = TS_PACKET_HASH_KEY(header, ddb);
	current_ddb = hashtable_get(priv->psi_tables, ddb->dentry->inode);
	
	/* Check whether we should keep processing this packet or not */
	if (! ddb->current_next_indicator) {
		dprintf("ddb doesn't have current_next_indicator bit set, skipping it");
		ddb_free(ddb);
		return 0;
	}

	/** DSM-CC Download Data Header */
	struct dsmcc_download_data_header *data_header = &ddb->dsmcc_download_data_header;
	int len = dsmcc_parse_download_data_header(data_header, &payload[8]);
	int j = 8 + len;
	
	if (data_header->_dsmcc_type != 0x03 ||
		data_header->_message_id != 0x1003) {
		ddb_free(ddb);
		return 0;
	}

	if (data_header->message_length < 5) {
		// XXX: expose header in the fs?
		if (data_header->message_length)
			dprintf("skipping message with len=%d", data_header->message_length);
		ddb_free(ddb);
		return 0;
	}

	/** DDB bits */
	ddb->module_id = CONVERT_TO_16(payload[j], payload[j+1]);
	ddb->module_version = payload[j+2];
	ddb->reserved = payload[j+3];
	ddb->block_number = CONVERT_TO_16(payload[j+4], payload[j+5]);
	ddb->_block_data_size = data_header->message_length - data_header->adaptation_length - 6;
	if (! ddb->_block_data_size) {
		ddb_free(ddb);
		return 0;
	}
	if (ddb_block_number_already_parsed(current_ddb, ddb->module_id, ddb->block_number)) {
		ddb_free(ddb);
		return 0;
	}

//	TS_INFO("DDB parser: pid=%d, table_id=%#x, ddb->version_number=%#x, ddb->block_number=%d, module_id=%d, current=%p", 
//			header->pid, ddb->table_id, ddb->version_number, ddb->block_number, ddb->module_id, current_ddb);

	/* Create filesystem entries for this table */
	struct dentry *version_dentry = NULL;
	if (current_ddb)
		version_dentry = fsutils_get_current(current_ddb->dentry);
	else
		ddb_create_directory(header, ddb, &version_dentry, priv);

	uint16_t this_block_size = payload_len - (j+6) - 4;
	uint16_t this_block_start = j+6;
	if (this_block_size != ddb->_block_data_size)
		TS_WARNING("ddb->block_data_size=%d != this_block_size=%d", ddb->_block_data_size, this_block_size);

	/* Create individual block file */
	struct dentry *module_dir = CREATE_DIRECTORY(version_dentry, "module_%02d", ddb->module_id);
	struct dentry *block_dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	block_dentry->size = this_block_size;
	block_dentry->mode = S_IFREG | 0444;
	block_dentry->obj_type = OBJ_TYPE_FILE;
	block_dentry->contents = malloc(this_block_size);
	memcpy(block_dentry->contents, &payload[this_block_start], this_block_size);
	asprintf(&block_dentry->name, "block_%02d.bin", ddb->block_number);
	CREATE_COMMON(module_dir, block_dentry);
	xattr_add(block_dentry, XATTR_FORMAT, XATTR_FORMAT_BIN, strlen(XATTR_FORMAT_BIN), false);
	
	if (current_ddb)
		ddb_free(ddb);
	else
		hashtable_add(priv->psi_tables, ddb->dentry->inode, ddb, (hashtable_free_function_t) ddb_free);

	return 0;
}
