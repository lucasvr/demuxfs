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
#include "tables/psi.h"
#include "tables/tot.h"
#include "tables/pes.h"
#include "tables/pat.h"

static void tot_create_directory(const struct ts_header *header, struct tot_table *tot, 
		struct demuxfs_data *priv)
{
	/* Create a directory named "TOT" at the root filesystem if it doesn't exist yet */
	struct dentry *tot_dir = CREATE_DIRECTORY(priv->root, FS_TOT_NAME);

	/* Create a directory named "Current" and populate it with files */
	//asprintf(&tot->dentry->name, "%#04x", header->pid);
	asprintf(&tot->dentry->name, FS_CURRENT_NAME);
	tot->dentry->mode = S_IFDIR | 0555;
	CREATE_COMMON(tot_dir, tot->dentry);
	
	/* PSI header */
	CREATE_FILE_NUMBER(tot->dentry, tot, table_id);
	CREATE_FILE_NUMBER(tot->dentry, tot, section_syntax_indicator);
	CREATE_FILE_NUMBER(tot->dentry, tot, section_length);

	/* TOT members */
	CREATE_FILE_NUMBER(tot->dentry, tot, utc3_time);
	CREATE_FILE_NUMBER(tot->dentry, tot, descriptors_loop_length);
}

int tot_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	int num_descriptors;
	struct tot_table *current_tot = NULL;
	struct tot_table *tot = (struct tot_table *) calloc(1, sizeof(struct tot_table));
	assert(tot);
	
	tot->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(tot->dentry);
	
	/* Copy data up to the first loop entry */
	tot->table_id                 = payload[0];
	tot->section_syntax_indicator = (payload[1] >> 7) & 0x01;
	tot->reserved_1               = (payload[1] >> 6) & 0x01;
	tot->reserved_2               = (payload[1] >> 4) & 0x03;
	tot->section_length           = ((payload[1] << 8) | payload[2]) & 0x0fff;
	
	/* Parse TOT specific bits */
	tot->utc3_time = CONVERT_TO_40(payload[3], payload[4], payload[5], payload[6], payload[7]) & 0xffffffffff;
	tot->reserved_4 = payload[8] >> 4;
	tot->descriptors_loop_length = CONVERT_TO_16(payload[8], payload[9]) & 0x0fff;
	num_descriptors = descriptors_count(&payload[10], tot->descriptors_loop_length);

	/* Set hash key and check if there's already one version of this table in the hash */
	tot->dentry->inode = TS_PACKET_HASH_KEY(header, tot);
	current_tot = hashtable_get(priv->psi_tables, tot->dentry->inode);
	
//	dprintf("*** TOT parser: pid=%#x, table_id=%#x, current_tot=%p, len=%d ***", 
//		header->pid, tot->table_id, current_tot, payload_len);
	
	if (current_tot) {
		current_tot->section_length = tot->section_length;
		current_tot->utc3_time = tot->utc3_time;
		current_tot->descriptors_loop_length = tot->descriptors_loop_length;

		free(tot->dentry);
		free(tot);

		tot = current_tot;
		CREATE_FILE_NUMBER(tot->dentry, tot, utc3_time);
		descriptors_parse(&payload[10], num_descriptors, tot->dentry, priv);
	} else {
		tot_create_directory(header, tot, priv);
		descriptors_parse(&payload[10], num_descriptors, tot->dentry, priv);
		hashtable_add(priv->psi_tables, tot->dentry->inode, tot);
	}
	
	return 0;
}
