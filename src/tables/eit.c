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
#include "tables/psi.h"
#include "tables/eit.h"

void eit_free(struct eit_table *eit)
{
	if (eit->dentry && eit->dentry->name)
		fsutils_dispose_tree(eit->dentry);
	else if (eit->dentry)
		/* Dentry has simply been calloc'ed */
		free(eit->dentry);

	/* Free the eit table structure */
	free(eit);
}

static void eit_create_directory(const struct ts_header *header, struct eit_table *eit, 
	struct dentry **version_dentry, struct demuxfs_data *priv)
{
	/* Create a directory named "EIT" at the root filesystem if it doesn't exist yet */
	struct dentry *eit_dir = CREATE_DIRECTORY(priv->root, FS_EIT_NAME);

	/* Create a directory named "<eit_pid>" and populate it with files */
	asprintf(&eit->dentry->name, "%#04x", header->pid);
	eit->dentry->mode = S_IFDIR | 0555;
	CREATE_COMMON(eit_dir, eit->dentry);
	
	/* Create the versioned dir and update the Current symlink */
	*version_dentry = fsutils_create_version_dir(eit->dentry, eit->version_number);

	psi_populate((void **) &eit, eit->dentry);
}

int eit_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct eit_table *current_eit = NULL;
	struct eit_table *eit = (struct eit_table *) calloc(1, sizeof(struct eit_table));
	assert(eit);
	
	eit->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(eit->dentry);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) eit, payload, payload_len);
	if (ret < 0) {
		eit_free(eit);
		return ret;
	}

	/* Set hash key and check if there's already one version of this table in the hash */
	eit->dentry->inode = TS_PACKET_HASH_KEY(header, eit);
	current_eit = hashtable_get(priv->psi_tables, eit->dentry->inode);

	/* Check whether we should keep processing this packet or not */
	if (! eit->current_next_indicator || (current_eit && current_eit->version_number == eit->version_number)) {
		eit_free(eit);
		return 0;
	}

	dprintf("*** EIT parser: pid=%#x, table_id=%#x, current_eit=%p, eit->version_number=%#x, len=%d ***", 
			header->pid, eit->table_id, current_eit, eit->version_number, payload_len);

	/* Parse EIT specific bits */
	struct dentry *version_dentry;
	eit_create_directory(header, eit, &version_dentry, priv);

	if (current_eit) {
		hashtable_del(priv->psi_tables, current_eit->dentry->inode);
		fsutils_migrate_children(current_eit->dentry, eit->dentry);
		eit_free(current_eit);
	}

	hashtable_add(priv->psi_tables, eit->dentry->inode, eit, (hashtable_free_function_t) eit_free);
	return 0;
}
