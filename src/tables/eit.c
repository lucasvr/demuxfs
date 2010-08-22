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
#include "byteops.h"
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

/* Convert from Modified Julian Date format to UTC */
static time_t eit_convert_from_mjd_time(uint64_t mjd_time)
{
	/* MJD epoch is set to Jan 01, 1970 */
	const int mjd_epoch = 40587;
	uint16_t  mjd       = (mjd_time >> 24) & 0xffff;
	time_t    utc_ymd   = (mjd - mjd_epoch) * 86400.0;

	/* Decode the BCD part of the time */
	uint32_t  bcd       = mjd_time & 0xffffff;
	uint8_t   hours     = (((bcd >> 20) & 0x0f) * 10) +
						   ((bcd >> 16) & 0x0f);
	uint8_t   minutes   = (((bcd >> 12) & 0xff) * 10) +
						   ((bcd >>  8) & 0x0f);
	uint8_t   seconds   = (((bcd >>  4) & 0x0f) * 10) +
						   ((bcd) & 0x0f);
	time_t    utc_hms   = (hours * 360) + (minutes * 60) + seconds;

	/* Set final UTC time */
	time_t    utc_time  = utc_ymd + utc_hms;

	return utc_time;
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

	eit->transport_stream_id = CONVERT_TO_16(payload[8], payload[9]);
	eit->original_network_id = CONVERT_TO_16(payload[10], payload[11]);
	eit->segment_last_section_number = payload[12];
	eit->last_table_id = payload[13];
	CREATE_FILE_NUMBER(version_dentry, eit, transport_stream_id);
	CREATE_FILE_NUMBER(version_dentry, eit, original_network_id);
	CREATE_FILE_NUMBER(version_dentry, eit, segment_last_section_number);
	CREATE_FILE_NUMBER(version_dentry, eit, last_table_id);

	struct eit_event *this_event;
	eit->eit_event = calloc(1, sizeof(struct eit_event));
	this_event = eit->eit_event;

	int event_nr = 1, i = 14;
	/* Include extra 4 bytes needed by the CRC32 */
	while ((i + 4) < payload_len) {
		char event_dirname[32];
		struct dentry *event_dentry;
		struct eit_event *next_event = NULL;
		
		this_event->event_id = CONVERT_TO_16(payload[i], payload[i+1]);
		this_event->start_time = CONVERT_TO_40(payload[i+2], payload[i+3], payload[i+4], payload[i+5], payload[i+6]);
		this_event->duration = CONVERT_TO_24(payload[i+7], payload[i+8], payload[i+9]);
		this_event->running_status = (payload[i+10] >> 5) & 0x03;
		this_event->free_ca_mode = (payload[i+10] >> 4) & 0x01;
		this_event->descriptors_loop_length = CONVERT_TO_16(payload[i+10], payload[i+11]) & 0x0fff;
		i += 12;

		eit_convert_from_mjd_time(this_event->start_time);
		sprintf(event_dirname, "EVENT_%02d", event_nr++);
		event_dentry = CREATE_DIRECTORY(version_dentry, event_dirname);
		CREATE_FILE_NUMBER(event_dentry, this_event, event_id);
		CREATE_FILE_NUMBER(event_dentry, this_event, start_time);
		CREATE_FILE_NUMBER(event_dentry, this_event, duration);
		CREATE_FILE_NUMBER(event_dentry, this_event, running_status);
		CREATE_FILE_NUMBER(event_dentry, this_event, free_ca_mode);
		CREATE_FILE_NUMBER(event_dentry, this_event, descriptors_loop_length);
		if (this_event->descriptors_loop_length) {
		}
		i += this_event->descriptors_loop_length;

		if (i < payload_len) {
			next_event = calloc(1, sizeof(struct eit_event));
			this_event->next = next_event;
			this_event = next_event;
		} else
			this_event->next = NULL;
	}

	if (current_eit) {
		hashtable_del(priv->psi_tables, current_eit->dentry->inode);
		fsutils_migrate_children(current_eit->dentry, eit->dentry);
		eit_free(current_eit);
	}

	hashtable_add(priv->psi_tables, eit->dentry->inode, eit, (hashtable_free_function_t) eit_free);
	return 0;
}
