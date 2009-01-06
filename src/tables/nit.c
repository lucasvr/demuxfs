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
#include "fsutils.h"
#include "xattr.h"
#include "hash.h"
#include "ts.h"
#include "descriptors.h"
#include "tables/psi.h"
#include "tables/nit.h"

static void nit_create_directory(struct nit_table *nit, struct dentry **version_dentry,
		struct demuxfs_data *priv)
{
	/* Create a directory named "NIT" and populate it with files */
	nit->dentry->name = strdup(FS_NIT_NAME);
	nit->dentry->mode = S_IFDIR | 0555;
	CREATE_COMMON(priv->root, nit->dentry);

	/* Create the versioned dir and update the Current symlink */
	*version_dentry = fsutils_create_version_dir(nit->dentry, nit->version_number);
	psi_populate((void **) &nit, *version_dentry);
}

int nit_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct nit_table *current_nit = NULL;
	struct nit_table *nit = (struct nit_table *) calloc(1, sizeof(struct nit_table));
	assert(nit);

	nit->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(nit->dentry);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) nit, payload, payload_len);
	if (ret < 0) {
		free(nit->dentry);
		free(nit);
		return ret;
	}

	/* Set hash key and check if there's already one version of this table in the hash */
	nit->dentry->inode = TS_PACKET_HASH_KEY(header, nit);
	current_nit = hashtable_get(priv->psi_tables, nit->dentry->inode);

	/* Check whether we should keep processing this packet or not */
	if (! nit->current_next_indicator || (current_nit && current_nit->version_number == nit->version_number)) {
		free(nit->dentry);
		free(nit);
		return 0;
	}
	
	dprintf("*** NIT parser: pid=%#x, table_id=%#x, current_nit=%p, nit->version_number=%#x, len=%d ***", 
			header->pid, nit->table_id, current_nit, nit->version_number, payload_len);

	/* TODO: check payload boundaries */

	/* Parse NIT specific bits */
	struct dentry *version_dentry;
	nit->reserved_4 = payload[8] >> 4;
	nit->network_descriptors_length = ((payload[8] << 8) | payload[9]) & 0x0fff;
	nit->num_descriptors = descriptors_count(&payload[10], nit->network_descriptors_length);
	nit_create_directory(nit, &version_dentry, priv);

	descriptors_parse(&payload[10], nit->num_descriptors, version_dentry, priv);

	uint8_t offset = 10 + nit->network_descriptors_length;
	nit->reserved_5 = payload[offset] >> 4;
	nit->transport_stream_loop_length = ((payload[offset] << 8) | payload[offset+1]) & 0x0fff;
	offset += 2;

	struct dentry *ts_dentry = CREATE_DIRECTORY(version_dentry, "TS_INFORMATION");
	uint16_t i = 0, info_index = 0;
	while (i < nit->transport_stream_loop_length) {
		char subdir[PATH_MAX];
		snprintf(subdir, sizeof(subdir), "%02d", ++info_index);
		struct dentry *info_dentry = CREATE_DIRECTORY(ts_dentry, subdir);

		struct nit_ts_data ts_data;
		ts_data.transport_stream_id = (payload[offset] << 8) | payload[offset+1];
		ts_data.original_network_id = (payload[offset+2] << 8) | payload[offset+3];
		ts_data.reserved_future_use = payload[offset+4] >> 4;
		ts_data.transport_descriptors_length = ((payload[offset+4] << 8) | payload[offset+5]) & 0x0fff;
		ts_data.num_descriptors = descriptors_count(&payload[offset+6], ts_data.transport_descriptors_length);
		CREATE_FILE_NUMBER(info_dentry, &ts_data, transport_stream_id);
		CREATE_FILE_NUMBER(info_dentry, &ts_data, original_network_id);
		CREATE_FILE_NUMBER(info_dentry, &ts_data, transport_descriptors_length);

		if (ts_data.original_network_id != nit->identifier)
			TS_WARNING("NIT: original_network_id(%#x) != network_id(%#x)", 
					ts_data.original_network_id, nit->identifier);

		descriptors_parse(&payload[offset+6], ts_data.num_descriptors, info_dentry, priv);
		i += 6 + ts_data.transport_descriptors_length;
		offset += 6 + ts_data.transport_descriptors_length;
	}

	hashtable_add(priv->psi_tables, nit->dentry->inode, nit);
	return 0;
}
