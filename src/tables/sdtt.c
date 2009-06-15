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
#include "tables/sdtt.h"
#include "tables/pes.h"
#include "tables/pat.h"

struct sdtt_text_info {
	char download_level[32];
	char version_indicator[64];
	char schedule_time_shift_information[128];
};

static void sdtt_check_header(struct sdtt_table *sdtt)
{
	if (sdtt->section_number != 0)
		TS_WARNING("section_number != 0");
	if (sdtt->last_section_number != 0)
		TS_WARNING("last_section_number != 0");
}

static void sdtt_create_directory(const struct ts_header *header, struct sdtt_table *sdtt, 
		struct dentry **version_dentry, struct demuxfs_data *priv)
{
	/* Create a directory named "SDTT" at the root filesystem if it doesn't exist yet */
	struct dentry *sdtt_dir = CREATE_DIRECTORY(priv->root, FS_SDTT_NAME);

	/* Create a directory named "<sdtt_pid>" and populate it with files */
	asprintf(&sdtt->dentry->name, "%#04x", header->pid);
	sdtt->dentry->mode = S_IFDIR | 0555;
	CREATE_COMMON(sdtt_dir, sdtt->dentry);
	
	/* Create the versioned dir and update the Current symlink */
	*version_dentry = fsutils_create_version_dir(sdtt->dentry, sdtt->version_number);

	psi_populate((void **) &sdtt, *version_dentry);
	//sdtt_populate(sdtt, *version_dentry, priv);
}

int sdtt_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct sdtt_table *current_sdtt = NULL;
	struct sdtt_table *sdtt = (struct sdtt_table *) calloc(1, sizeof(struct sdtt_table));
	assert(sdtt);
	
	sdtt->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(sdtt->dentry);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) sdtt, payload, payload_len);
	if (ret < 0) {
		free(sdtt->dentry);
		free(sdtt);
		return ret;
	}
	sdtt_check_header(sdtt);
	
	/* Set hash key and check if there's already one version of this table in the hash */
	sdtt->dentry->inode = TS_PACKET_HASH_KEY(header, sdtt);
	current_sdtt = hashtable_get(priv->psi_tables, sdtt->dentry->inode);
	
	/* Check whether we should keep processing this packet or not */
	if (! sdtt->current_next_indicator || (current_sdtt && current_sdtt->version_number == sdtt->version_number)) {
		free(sdtt->dentry);
		free(sdtt);
		return 0;
	}
	
	dprintf("*** SDTT parser: pid=%#x, table_id=%#x, current_sdtt=%p, sdtt->version_number=%#x, len=%d ***", 
			header->pid, sdtt->table_id, current_sdtt, sdtt->version_number, payload_len);

	/* Parse SDTT specific bits */
	struct dentry *version_dentry = NULL;
	sdtt_create_directory(header, sdtt, &version_dentry, priv);

	sdtt->maker_id = sdtt->identifier >> 8;
	sdtt->model_id = sdtt->identifier & 0xff;
	sdtt->transport_stream_id = CONVERT_TO_16(payload[8], payload[9]);
	sdtt->original_network_id = CONVERT_TO_16(payload[10], payload[11]);
	sdtt->service_id = CONVERT_TO_16(payload[12], payload[13]);
	sdtt->num_of_contents = payload[14];
	if (sdtt->num_of_contents)
		sdtt->contents = calloc(sdtt->num_of_contents, sizeof(struct sdtt_contents));

	CREATE_FILE_NUMBER(version_dentry, sdtt, maker_id);
	CREATE_FILE_NUMBER(version_dentry, sdtt, model_id);
	CREATE_FILE_NUMBER(version_dentry, sdtt, transport_stream_id);
	CREATE_FILE_NUMBER(version_dentry, sdtt, original_network_id);
	CREATE_FILE_NUMBER(version_dentry, sdtt, service_id);
	CREATE_FILE_NUMBER(version_dentry, sdtt, num_of_contents);
	
	for (uint8_t i=0; i<sdtt->num_of_contents; ++i) {
		struct sdtt_contents *c = &sdtt->contents[i];
		struct sdtt_text_info txt;
		struct dentry *subdir = CREATE_DIRECTORY(version_dentry, "%02d", i+1);

		int index = 15 * (i+1);
		c->group = payload[index] >> 4;
		c->target_version = CONVERT_TO_16(payload[index], payload[index+1]) & 0x0fff;
		c->new_version = CONVERT_TO_16(payload[index+2], payload[index+3]) >> 4;
		c->download_level = payload[index+3] >> 2;
		CREATE_FILE_NUMBER(subdir, c, group);
		CREATE_FILE_NUMBER(subdir, c, target_version);
		CREATE_FILE_NUMBER(subdir, c, new_version);
		CREATE_FILE_NUMBER(subdir, c, download_level);

		if (c->download_level == 0x01)
			sprintf(txt.download_level, "Mandatory [0x01]");
		else if (c->download_level == 0x00)
			sprintf(txt.download_level, "Optional [0x00]");
		else
			sprintf(txt.download_level, "Unknown [%#02x]", c->download_level);
		CREATE_FILE_STRING(subdir, &txt, download_level, XATTR_FORMAT_STRING_AND_NUMBER);
		
		c->version_indicator = payload[index+3] & 0x03;
		if (c->version_indicator == 0x00)
			sprintf(txt.version_indicator, "All versions are considered valid [0x00]");
		else if (c->version_indicator == 0x01)
			sprintf(txt.version_indicator, "The specified version or older versions are considered valid [0x01]");
		else if (c->version_indicator == 0x02)
			sprintf(txt.version_indicator, "The specified version or newer versions are considered valid [0x02]");
		else if (c->version_indicator == 0x03)
			sprintf(txt.version_indicator, "Only the specified version is considered valid [0x03]");
		CREATE_FILE_STRING(subdir, &txt, version_indicator, XATTR_FORMAT_STRING_AND_NUMBER);

		c->content_descriptor_length = CONVERT_TO_16(payload[index+4], payload[index+5]) >> 4;
		c->reserved = payload[index+5] & 0x0f;
		c->schedule_descriptor_length = CONVERT_TO_16(payload[index+6], payload[index+7]) >> 4;
		CREATE_FILE_NUMBER(subdir, c, content_descriptor_length);
		CREATE_FILE_NUMBER(subdir, c, schedule_descriptor_length);

		c->schedule_time_shift_information = payload[index+7] & 0x0f;
		if (c->schedule_time_shift_information == 0)
			sprintf(txt.schedule_time_shift_information,
				"The same download contents is transmitted in the same schedule with multiple service_ids [0x00]");
		else if (c->schedule_time_shift_information >= 1 && c->schedule_time_shift_information <= 12)
			sprintf(txt.schedule_time_shift_information,
				"The same download contents is transmitted by shifting the time from 1 to 12 hours for" \
				"each service_id with multiple service_ids [%#02x]", c->schedule_time_shift_information);
		else if (c->schedule_time_shift_information == 13 || c->schedule_time_shift_information == 14)
			sprintf(txt.schedule_time_shift_information, "Reserved [%#02x]", c->schedule_time_shift_information);
		else
			sprintf(txt.schedule_time_shift_information,
				"The download contents is transmitted with a unique service_id [0x0f]");
		CREATE_FILE_STRING(subdir, &txt, schedule_time_shift_information, XATTR_FORMAT_STRING_AND_NUMBER);

		c->_sched_entries = c->schedule_descriptor_length/sizeof(uint64_t);
		if (c->_sched_entries)
			c->sched = calloc(c->_sched_entries, sizeof(struct sdtt_schedule));
		for (uint16_t j=0; j<c->_sched_entries; ++j) {
			struct dentry *sched_dentry = CREATE_DIRECTORY(subdir, "sched_%02d", j+1);
			int idx = index * (j+1);

			c->sched[j].start_time = CONVERT_TO_40(payload[idx], payload[idx+1], 
					payload[idx+2], payload[idx+3], payload[idx+4]) & 0xffffffffff;
			c->sched[j].duration = CONVERT_TO_24(payload[idx+5], payload[idx+6],
					payload[idx+7]);
			CREATE_FILE_NUMBER(sched_dentry, &c->sched[j], start_time);
			CREATE_FILE_NUMBER(sched_dentry, &c->sched[j], duration);
		}
		c->_descriptors_length = c->content_descriptor_length - c->schedule_descriptor_length;

		index = index+8+(c->_sched_entries*8);
		c->_num_descriptors = descriptors_count(&payload[index], c->_descriptors_length);
		descriptors_parse(&payload[index], c->_num_descriptors, subdir, priv);
	}

	if (current_sdtt) {
		hashtable_del(priv->psi_tables, current_sdtt->dentry->inode);
		fsutils_migrate_children(current_sdtt->dentry, sdtt->dentry);
		fsutils_dispose_tree(current_sdtt->dentry);
		free(current_sdtt);
	}
	hashtable_add(priv->psi_tables, sdtt->dentry->inode, sdtt);

	return 0;
}
