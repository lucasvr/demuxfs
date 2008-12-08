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

static void nit_create_directory(struct nit_table *nit, struct demuxfs_data *priv)
{
	/* Create a directory named "NIT" and populate it with files */
	nit->dentry.name = strdup(FS_NIT_NAME);
	nit->dentry.mode = S_IFDIR | 0555;
	CREATE_COMMON(priv->root, &nit->dentry);

	psi_populate((void **) &nit, &nit->dentry);
}

int nit_parse(const struct ts_header *header, const char *payload, uint8_t payload_len,
		struct demuxfs_data *priv)
{
	struct nit_table *current_nit = NULL;
	struct nit_table *nit = (struct nit_table *) calloc(1, sizeof(struct nit_table));
	assert(nit);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) nit, payload, payload_len);
	if (ret < 0) {
		free(nit);
		return ret;
	}

	/* Set hash key and check if there's already one version of this table in the hash */
	nit->dentry.inode = TS_PACKET_HASH_KEY(header, nit);
	current_nit = hashtable_get(priv->table, nit->dentry.inode);
	hashtable_add(priv->table, nit->dentry.inode, nit);

	return 0;
}
