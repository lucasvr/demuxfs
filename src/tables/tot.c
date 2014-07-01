/* 
 * Copyright (c) 2008-2010, Lucas C. Villa Real <lucasvr@gobolinux.org>
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

void tot_free(struct tot_table *tot)
{
	if (tot->dentry && tot->dentry->name)
		fsutils_dispose_tree(tot->dentry);
	else if (tot->dentry)
		/* Dentry has simply been calloc'ed */
		free(tot->dentry);

	/* Free the tot table structure */
	free(tot);
}

static char *convert_string_from_utc(uint64_t utc)
{
    uint8_t hh, mm, ss;
	uint8_t d = 0, y = 0, m = 0;

	const uint8_t h_1 = (utc & 0xF00000) >> 20;
	const uint8_t h_2 = (utc & 0x0F0000) >> 16;
	hh = h_1*10 + h_2;

	const uint8_t m_1 = (utc & 0x00F000) >> 12;
	const uint8_t m_2 = (utc & 0x000F00) >> 8; 
	mm = m_1*10 + m_2;

	uint8_t s_1 = (utc & 0x0000F0) >> 4;
	uint8_t s_2 = (utc & 0x00000F);
	ss = s_1*10 + s_2;

	const uint32_t mjd = utc >> 24;
	if (mjd != 0) {
		const uint32_t _y = (uint32_t)((mjd - 15078.2)/365.25);
		const uint32_t _m = (uint32_t)((mjd - 14956.1 - (uint32_t)(_y * 365.25))/30.6001);
		const uint32_t _d = mjd - 14956 - (uint32_t)(_y * 365.25) - (uint32_t)(_m * 30.6001);
		const uint32_t _k = (_m == 14 || _m == 15)?1:0;

		y = (uint8_t) _y + _k;
		m = (uint8_t) _m - 1 - _k * 12;
		d = (uint8_t) _d;
	}
    
	char *ret = (char *) malloc(sizeof(char) * 80);
	if (ret)
		snprintf(ret, 80, "%04u-%02u-%02u %02u:%02u:%02u", y?y+1900:0, m, d, hh, mm, ss);
    return ret;   
}

static void tot_create_ut3c_time(struct tot_table *tot)
{
	char *ret, *utc, raw_str[64];
	struct tm tm;
	memset(&tm, 0, sizeof(tm));

	/* 1: convert from network time format to a formatted string */
	utc = convert_string_from_utc(tot->_utc3_time);

	/* 2: convert from formatted string to struct tm */
	ret = strptime(utc, "%Y-%m-%d %H:%M:%S", &tm);
	if (! ret) {
		perror("strptime");
		free(utc);
		return;
	}
	free(utc);

	/* 3: convert from struct tm to a human understandable string */
	memset(tot->utc3_time, 0, sizeof(tot->utc3_time));
	ret = asctime_r(&tm, tot->utc3_time);
	if (! ret) {
		perror("asctime_r");
		return;
	}

	tot->utc3_time[strlen(tot->utc3_time)-1] = '\0';
	snprintf(raw_str, sizeof(raw_str), " [%#0llx]", tot->_utc3_time);
	strcat(tot->utc3_time, raw_str);

	CREATE_FILE_STRING(tot->dentry, tot, utc3_time, XATTR_FORMAT_STRING_AND_NUMBER);
}

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
	CREATE_FILE_NUMBER(tot->dentry, tot, descriptors_loop_length);
	tot_create_ut3c_time(tot);
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
	tot->section_length           = CONVERT_TO_16(payload[1], payload[2]) & 0x0fff;
	
	/* Parse TOT specific bits */
	tot->_utc3_time = CONVERT_TO_40(payload[3], payload[4], payload[5], payload[6], payload[7]) & 0xffffffffff;
	tot->reserved_4 = payload[8] >> 4;
	tot->descriptors_loop_length = CONVERT_TO_16(payload[8], payload[9]) & 0x0fff;
	num_descriptors = descriptors_count(&payload[10], tot->descriptors_loop_length);

	/* Set hash key and check if there's already one version of this table in the hash */
	tot->dentry->inode = TS_PACKET_HASH_KEY(header, tot);
	current_tot = hashtable_get(priv->psi_tables, tot->dentry->inode);
	
//	TS_INFO("TOT parser: pid=%#x, table_id=%#x, current_tot=%p, len=%d", 
//		header->pid, tot->table_id, current_tot, payload_len);
	
	if (current_tot) {
		current_tot->section_length = tot->section_length;
		current_tot->_utc3_time = tot->_utc3_time;
		current_tot->descriptors_loop_length = tot->descriptors_loop_length;

	//	fs_dispose_tree(tot->dentry);
		free(tot->dentry);
		free(tot);

		tot = current_tot;
		tot_create_ut3c_time(tot);
		descriptors_parse(&payload[10], num_descriptors, tot->dentry, priv);
	} else {
		tot_create_directory(header, tot, priv);
		descriptors_parse(&payload[10], num_descriptors, tot->dentry, priv);
		hashtable_add(priv->psi_tables, tot->dentry->inode, tot, (hashtable_free_function_t) tot_free);
	}
	
	return 0;
}
