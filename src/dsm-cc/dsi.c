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
#include "dsm-cc/dsi.h"
#include "dsm-cc/descriptors/descriptors.h"

static void dsi_free(struct dsi_table *dsi)
{
	/* Free DSM-CC compatibility descriptors */
	dsmcc_free_compatibility_descriptors(&dsi->compatibility_descriptor);
	/* Free DSM-CC message header */
	dsmcc_free_message_header(&dsi->dsmcc_message_header);
	/* Free Private data */
//	if (dsi->private_data_bytes)
//		free(dsi->private_data_bytes);
	/* Free the dentry and its subtree */
	if (dsi->dentry && dsi->dentry->name)
		fsutils_dispose_tree(dsi->dentry);
	/* Free the dsi table structure */
	free(dsi);
}

static void dsi_create_directory(const struct ts_header *header, struct dsi_table *dsi, 
		struct dentry **version_dentry, struct demuxfs_data *priv)
{
	/* Create a directory named "DSI" at the root filesystem if it doesn't exist yet */
	struct dentry *dsi_dir = CREATE_DIRECTORY(priv->root, FS_DSI_NAME);

	/* Create a directory named "<dsi_pid>" and populate it with files */
	asprintf(&dsi->dentry->name, "%#04x", header->pid);
	dsi->dentry->mode = S_IFDIR | 0555;
	CREATE_COMMON(dsi_dir, dsi->dentry);

	/* Create the versioned dir and update the Current symlink */
	*version_dentry = fsutils_create_version_dir(dsi->dentry, dsi->version_number);

	psi_populate((void **) &dsi, *version_dentry);
}

static void dsi_create_dentries(struct dentry *parent, struct dsi_table *dsi, struct demuxfs_data *priv)
{
	struct dsmcc_message_header *msg_header = &dsi->dsmcc_message_header;
	struct dsmcc_compatibility_descriptor *cd = &dsi->compatibility_descriptor;

	dsmcc_create_message_header_dentries(msg_header, parent);
	if (dsi->private_data_length)
		dsmcc_create_compatibility_descriptor_dentries(cd, parent);

	CREATE_FILE_NUMBER(parent, dsi, private_data_length);
//	CREATE_FILE_BIN(parent, dsi, private_data_bytes, dsi->private_data_length);
}

int dsi_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct dsi_table *dsi, *current_dsi = NULL;
	
	if (payload_len < 20) {
		dprintf("payload is too small (%d)", payload_len);
		return 0;
	}
	
	dsi = (struct dsi_table *) calloc(1, sizeof(struct dsi_table));
	assert(dsi);
	dsi->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(dsi->dentry);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) dsi, payload, payload_len);
	if (ret < 0) {
		dprintf("error parsing PSI packet");
		dsi_free(dsi);
		return 0;
	}

	/** DSM-CC Message Header */
	struct dsmcc_message_header *msg_header = &dsi->dsmcc_message_header;
	int j = dsmcc_parse_message_header(msg_header, payload, 8);
	
	/** 
	 * At this point we know for sure that this is a DSI table.
	 *
	 * However, since the dentry inode is generated based on the PID and
	 * table_id, that will certainly match with any DII tables previously
	 * added to the hash table, as the DSI can arrive in the same PID.
	 *
	 * For that reason we need to modify the inode number, and we
	 * do that by setting the 1st bit from the 7th byte, which is not
	 * used by the TS_PACKET_HASH_KEY macro.
	 * */ 
	dsi->dentry->inode = TS_PACKET_HASH_KEY(header, dsi) | 0x1000000;

	current_dsi = hashtable_get(priv->psi_tables, dsi->dentry->inode);
	if (! dsi->current_next_indicator || (current_dsi && current_dsi->version_number == dsi->version_number)) {
		dsi_free(dsi);
		return 0;
	}

	dprintf("*** DSI parser: pid=%#x, table_id=%#x, dsi->version_number=%#x, transaction_nr=%#x ***", 
			header->pid, dsi->table_id, dsi->version_number, msg_header->transaction_id & ~0x80000000);

	/* Create filesystem entries for this table */
	struct dentry *version_dentry = NULL;
	dsi_create_directory(header, dsi, &version_dentry, priv);
	dsi_create_dentries(version_dentry, dsi, priv);

	/** Parse DSI bits */

	/* server_id must contain 20 entries filled up with 0xff */
	memcpy(dsi->server_id, &payload[j], 20);
	CREATE_FILE_BIN(version_dentry, dsi, server_id, 20);
	j += 20;

	/** DSM-CC Compatibility Descriptor. There must be no entries in this loop. */
	struct dsmcc_compatibility_descriptor *cd = &dsi->compatibility_descriptor;
	cd->compatibility_descriptor_length = CONVERT_TO_16(payload[j], payload[j+1]);
	if (cd->compatibility_descriptor_length)
		TS_WARNING("DSM-CC compatibility descriptor has length != 0 (%d) (payload[%d],[%d] = %d,%d), payload_len=%d",
			cd->compatibility_descriptor_length, j, j+1, payload[j], payload[j+1], payload_len);
	CREATE_FILE_NUMBER(version_dentry, cd, compatibility_descriptor_length);

	/* Private data (Group Info Indication) */
	dsi->private_data_length = CONVERT_TO_16(payload[j+2], payload[j+3]);
	if ((payload_len-j-8) != dsi->private_data_length)
		TS_WARNING("DSI private_data_length=%d, end of data=%d", 
			dsi->private_data_length, payload_len-j-8);
	CREATE_FILE_NUMBER(version_dentry, dsi, private_data_length);

	/* Force data length to be within the payload boundaries */
	dsi->private_data_length = payload_len-j-8;
	j += 4;

	struct dsi_group_info_indication *gii = &dsi->group_info_indication;
	gii->number_of_groups = CONVERT_TO_16(payload[j], payload[j+1]);
	CREATE_FILE_NUMBER(version_dentry, gii, number_of_groups);
	j += 2;

	if (gii->number_of_groups) {
		gii->dsi_group_info = calloc(gii->number_of_groups, sizeof(struct dsi_group_info));
		for (uint16_t i=0; i<gii->number_of_groups; ++i) {
			struct dentry *group_dentry = CREATE_DIRECTORY(version_dentry, "GroupInfo_%02d", i+1);
			struct dsi_group_info *group_info = &gii->dsi_group_info[i];
			int len;

			group_info->group_id = CONVERT_TO_32(payload[j], payload[j+1],
				payload[j+2], payload[j+3]);
			group_info->group_size = CONVERT_TO_32(payload[j+4], payload[j+5],
				payload[j+6], payload[j+7]);
			CREATE_FILE_NUMBER(group_dentry, group_info, group_id);
			CREATE_FILE_NUMBER(group_dentry, group_info, group_size);

			// GroupCompatibility()
			len = dsmcc_parse_compatibility_descriptors(&group_info->group_compatibility,
				&payload[j+8], j+8);
			dsmcc_create_compatibility_descriptor_dentries(&group_info->group_compatibility,
				group_dentry);
			j += 8 + len;
		}
	}

	gii->private_data_length = CONVERT_TO_16(payload[j], payload[j+1]);
	dprintf("private_data_length=%d", gii->private_data_length);
	
	if (current_dsi) {
		hashtable_del(priv->psi_tables, current_dsi->dentry->inode);
		fsutils_migrate_children(current_dsi->dentry, dsi->dentry);
		fsutils_dispose_tree(current_dsi->dentry);
		free(current_dsi);
	}
	hashtable_add(priv->psi_tables, dsi->dentry->inode, dsi);

	return 0;
}
