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
#include "tables/sdt.h"
#include "tables/pes.h"
#include "tables/pat.h"

static void sdt_check_header(struct sdt_table *sdt)
{
	if (sdt->section_number != 0)
		TS_WARNING("section_number != 0");
	if (sdt->last_section_number != 0)
		TS_WARNING("last_section_number != 0");
}

void sdt_free(struct sdt_table *sdt)
{
	if (sdt->dentry && sdt->dentry->name)
		fsutils_dispose_tree(sdt->dentry);
	else if (sdt->dentry)
		/* Dentry has simply been calloc'ed */
		free(sdt->dentry);

	/* Free the sdt table structure */
	if (sdt->_services)
		free(sdt->_services);
	free(sdt);
}

static void sdt_create_directory(const struct ts_header *header, struct sdt_table *sdt, 
		struct dentry **version_dentry, struct demuxfs_data *priv)
{
	/* Create a directory named "SDT" at the root filesystem */
	sdt->dentry->name = strdup(FS_SDT_NAME);
	sdt->dentry->mode = S_IFDIR | 0555;
	CREATE_COMMON(priv->root, sdt->dentry);

	/* Create the versioned dir and update the Current symlink */
	*version_dentry = fsutils_create_version_dir(sdt->dentry, sdt->version_number);

	psi_populate((void **) &sdt, *version_dentry);
	//sdt_populate(sdt, *version_dentry, priv);
}

int sdt_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct sdt_table *current_sdt = NULL;
	struct sdt_table *sdt = (struct sdt_table *) calloc(1, sizeof(struct sdt_table));
	assert(sdt);
	
	sdt->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(sdt->dentry);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) sdt, payload, payload_len);
	if (ret < 0) {
		sdt_free(sdt);
		return ret;
	}
	sdt_check_header(sdt);
	
	/* Set hash key and check if there's already one version of this table in the hash */
	sdt->dentry->inode = TS_PACKET_HASH_KEY(header, sdt);
	current_sdt = hashtable_get(priv->psi_tables, sdt->dentry->inode);
	
	/* Check whether we should keep processing this packet or not */
	if (! sdt->current_next_indicator || (current_sdt && current_sdt->version_number == sdt->version_number)) {
		sdt_free(sdt);
		return 0;
	}
	
	dprintf("*** SDT parser: pid=%#x, table_id=%#x, current_sdt=%p, sdt->version_number=%#x, len=%d ***", 
			header->pid, sdt->table_id, current_sdt, sdt->version_number, payload_len);

	/* Parse SDT specific bits */
	struct dentry *version_dentry = NULL;
	sdt_create_directory(header, sdt, &version_dentry, priv);

	sdt->original_network_id = CONVERT_TO_16(payload[8], payload[9]);
	sdt->reserved_future_use = payload[10];
	CREATE_FILE_NUMBER(version_dentry, sdt, original_network_id);
	
	uint32_t crc;
	uint32_t j, i = 11;
	while (i < payload_len-sizeof(crc)) {
		uint16_t descriptor_loop_length = CONVERT_TO_16(payload[i+3], payload[i+4]) & 0x0FFF;
		i += 5 + descriptor_loop_length;
		if (i > payload_len - 4) {
			TS_WARNING("descriptor_loop_length exceeds table size");
			sdt_free(sdt);
			return -EINVAL;
		}
		sdt->_number_of_services++;
	}

	sdt->_services = calloc(sdt->_number_of_services, sizeof(struct sdt_service_info));
	for (j=0, i=11; j < sdt->_number_of_services; ++j) {
		struct sdt_service_info *si = &sdt->_services[j];
		struct dentry *service_dentry = CREATE_DIRECTORY(version_dentry, "Service_%02d", j+1);

		si->service_id = CONVERT_TO_16(payload[i], payload[i+1]);
		si->reserved_future_use = (payload[i+2] >> 2) & 0x3f;
		si->eit_schedule_flag = (payload[i+2] >> 1) & 0x01;
		si->eit_present_following_flag = payload[i+2] & 0x01;
		si->running_status = (payload[i+3] >> 5) & 0x07;
		si->free_ca_mode = (payload[i+3] >> 4) & 0x01;
		si->descriptors_loop_length = CONVERT_TO_16(payload[i+3], payload[i+4]) & 0x0fff;
		CREATE_FILE_NUMBER(service_dentry, si, service_id);
		CREATE_FILE_NUMBER(service_dentry, si, eit_schedule_flag);
		CREATE_FILE_NUMBER(service_dentry, si, eit_present_following_flag);
		CREATE_FILE_NUMBER(service_dentry, si, running_status);
		CREATE_FILE_NUMBER(service_dentry, si, free_ca_mode);
		CREATE_FILE_NUMBER(service_dentry, si, descriptors_loop_length);

		if (! pat_announces_service(si->service_id, priv))
			TS_WARNING("service_id %#x not declared by the PAT", si->service_id);

		uint32_t n = 0;
		while (n < si->descriptors_loop_length) {
			uint8_t descriptor_length = payload[i+5+n+1];
			descriptors_parse(&payload[i+5+n], 1, service_dentry, priv);
			n += 2 + descriptor_length;
		}
		i += 5 + si->descriptors_loop_length;
	}

	if (current_sdt) {
		hashtable_del(priv->psi_tables, current_sdt->dentry->inode);
		fsutils_migrate_children(current_sdt->dentry, sdt->dentry);
		sdt_free(current_sdt);
	}
	hashtable_add(priv->psi_tables, sdt->dentry->inode, sdt, (hashtable_free_function_t) sdt_free);

	return 0;
}
