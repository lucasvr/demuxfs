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

/* PAT stuff */
static void pat_dump(struct pat_table *pat)
{
	psi_dump_header((struct psi_common_header *) pat);
}

static void pat_populate(struct pat_table *pat, struct dentry *parent, 
		struct demuxfs_data *priv)
{
	/* "Programs" directory */
	struct dentry *dentry = NULL;
	CREATE_DIRECTORY(parent, "Programs", &dentry);

	/* For each program, create a symlink which points to an entry with the same name in the PMT */
	for (uint16_t i=0; i<pat->num_programs; ++i) {
		char name[32], target[32];
		snprintf(name, sizeof(name), "%#04x", pat->programs[i].program_number);
		snprintf(target, sizeof(target), "../../%#04x", pat->programs[i].pid);
		CREATE_SYMLINK(dentry, name, target, NULL);
	}

	/* Append new parsers to the list of known PIDs */
	write_lock();
	for (uint16_t i=0; i<pat->num_programs; ++i) {
		uint16_t pid = pat->programs[i].pid;
		uint16_t program_number = pat->programs[i].program_number;
		hashtable_add(priv->psi_parsers, pid, program_number == 0 ? nit_parse : pmt_parse);
	}
	write_unlock();
}

static void pat_create_directory(struct pat_table *pat, struct demuxfs_data *priv)
{
	/* Create a directory named "0x0" and populate it with files */
	pat->dentry.name = strdup("0x0");
	pat->dentry.mode = S_IFDIR | 0555;
	INIT_LIST_HEAD(&pat->dentry.children);
	INIT_LIST_HEAD(&pat->dentry.xattrs);

	psi_populate((void **) &pat, &pat->dentry);
	pat_populate(pat, &pat->dentry, priv);
	pat_dump(pat);

	write_lock();
	hashtable_add(priv->table, pat->dentry.inode, pat);
	list_add_tail(&pat->dentry.list, &priv->root->children);
	write_unlock();

	/* Create a symlink named "PAT" pointing to "0x0" */
	CREATE_SYMLINK(priv->root, "PAT", pat->dentry.name, NULL);
}

static void pat_update_directory(struct pat_table *current_pat, struct pat_table *pat,
		struct demuxfs_data *priv)
{
	dprintf("TODO: parse new version");
}

int pat_parse(const struct ts_header *header, const void *vpayload, uint8_t payload_len, 
		struct demuxfs_data *priv)
{
	//int table_size = TS_PAYLOAD_LENGTH(payload);
	struct pat_table *pat = (struct pat_table *) calloc(1, sizeof(struct pat_table));
	assert(pat);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) pat, vpayload, payload_len);
	if (ret < 0) {
		free(pat);
		return ret;
	}

	/* Set hash key and check if there's already one version of this table in the hash */
	pat->dentry.inode = TS_PACKET_HASH_KEY(header, pat);
	struct pat_table *current_pat = hashtable_get(priv->table, pat->dentry.inode);

	/* Check whether we should keep processing this packet or not */
	if (! pat->current_next_indicator || (current_pat && current_pat->version_number == pat->version_number)) {
		free(pat);
		return 0;
	}

	/* Parse PAT specific bits */
	pat->num_programs = (pat->section_length - 
		/* transport_stream_id */ 2 -
		/* reserved/version_number/current_next_indicator */ 1 -
		/* section_number */ 1 -
		/* last_section_number */ 1 -
		/* crc32 */ 4) / 4;

	pat->programs = (struct pat_program *) calloc(pat->num_programs, sizeof(struct pat_program));
	assert(pat->programs);

	char *payload = (char *) vpayload;
	for (uint16_t i=0; i<pat->num_programs; ++i) {
		uint16_t offset = 8 + (i * 4);
		pat->programs[i].program_number = (payload[offset] << 8) | payload[offset+1];
		pat->programs[i].reserved = payload[offset+2] >> 4;
		pat->programs[i].pid = ((payload[offset+2] << 8) | payload[offset+3]) & 0x1fff;
	}

	if (! current_pat)
		pat_create_directory(pat, priv);
	else
		pat_update_directory(current_pat, pat, priv);
	
	return 0;
}
