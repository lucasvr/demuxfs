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

struct formatted_descriptor {
	char *stream_type_identifier;
};

static void pmt_check_header(struct pmt_table *pmt)
{
	if (pmt->section_number != 0)
		TS_WARNING("section_number != 0");
	if (pmt->last_section_number != 0)
		TS_WARNING("last_section_number != 0");
}

static void pmt_populate(struct pmt_table *pmt, struct dentry *parent, 
		struct demuxfs_data *priv)
{
	CREATE_FILE_NUMBER(parent, pmt, reserved_4);
	CREATE_FILE_NUMBER(parent, pmt, pcr_pid);
	CREATE_FILE_NUMBER(parent, pmt, reserved_5);
	CREATE_FILE_NUMBER(parent, pmt, program_information_length);
}

static void pmt_populate_stream_dir(struct pmt_stream *stream, 
		struct dentry *parent, struct dentry **subdir, struct demuxfs_data *priv)
{
	char dirname[16], stream_type[256], es_path[PATH_MAX], *es;
	sprintf(dirname, "%#4x", stream->elementary_stream_pid);
	*subdir = CREATE_DIRECTORY(parent, dirname);
	
	/* Create a symlink in the root filesystem pointing to this new directory */
	es = fsutils_path_walk((*subdir), es_path, sizeof(es_path));
	if (es)
		CREATE_SYMLINK(priv->root, dirname, es+1);

	/* Create a FIFO which will contain this PES contents */
	CREATE_FIFO((*subdir), FS_PES_FIFO_NAME);

	struct formatted_descriptor f, *fptr = &f;
	snprintf(stream_type, sizeof(stream_type), "%s [%#x]",
			stream_type_to_string(stream->stream_type_identifier),
			stream->stream_type_identifier);
	fptr->stream_type_identifier = stream_type;
	CREATE_FILE_STRING((*subdir), fptr, stream_type_identifier, XATTR_FORMAT_STRING_AND_NUMBER);

	CREATE_FILE_NUMBER((*subdir), stream, reserved_1);
	CREATE_FILE_NUMBER((*subdir), stream, elementary_stream_pid);
	CREATE_FILE_NUMBER((*subdir), stream, reserved_2);
	CREATE_FILE_NUMBER((*subdir), stream, es_information_length);
	
	/* Start parsing this PES PID from now on */
	hashtable_add(priv->pes_parsers, stream->elementary_stream_pid, pes_parse);
}

static void pmt_create_directory(const struct ts_header *header, struct pmt_table *pmt, 
		struct dentry **descriptors_dentry, struct dentry **streams_dentry,
		struct demuxfs_data *priv)
{
	char pathname[PATH_MAX];

	/* Create a directory named "PMT" at the root filesystem if it doesn't exist yet */
	sprintf(pathname, "/%s", FS_PMT_NAME);
	struct dentry *pmt_dir = fsutils_get_dentry(priv->root, pathname);
	if (! pmt_dir)
		pmt_dir = CREATE_DIRECTORY(priv->root, FS_PMT_NAME);

	/* Create a directory named "<pmt_pid>" and populate it with files */
	asprintf(&pmt->dentry.name, "%#04x", header->pid);
	pmt->dentry.mode = S_IFDIR | 0555;
	CREATE_COMMON(pmt_dir, &pmt->dentry);

	psi_populate((void **) &pmt, &pmt->dentry);
	pmt_populate(pmt, &pmt->dentry, priv);
	psi_dump_header((struct psi_common_header *) pmt);

	/* Create a sub-directory named "Descriptors" */
	*descriptors_dentry = CREATE_DIRECTORY(&pmt->dentry, FS_DESCRIPTORS_NAME);
	
	/* Create a sub-directory named "Streams" */
	*streams_dentry = CREATE_DIRECTORY(&pmt->dentry, FS_STREAMS_NAME);
}

int pmt_parse(const struct ts_header *header, const char *payload, uint8_t payload_len,
		struct demuxfs_data *priv)
{
	struct pmt_table *current_pmt = NULL;
	struct pmt_table *pmt = (struct pmt_table *) calloc(1, sizeof(struct pmt_table));
	assert(pmt);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) pmt, payload, payload_len);
	if (ret < 0) {
		free(pmt);
		return ret;
	}
	pmt_check_header(pmt);
	
	/* Set hash key and check if there's already one version of this table in the hash */
	pmt->dentry.inode = TS_PACKET_HASH_KEY(header, pmt);
	current_pmt = hashtable_get(priv->table, pmt->dentry.inode);
	
	/* Check whether we should keep processing this packet or not */
	if (! pmt->current_next_indicator || (current_pmt && current_pmt->version_number == pmt->version_number)) {
		free(pmt);
		return 0;
	}

	/* TODO: increment the directory number somehow to indicate that this is a new version */
	
	/* Parse PMT specific bits */
	struct dentry *descriptors_dentry, *streams_dentry;
	pmt->reserved_4 = payload[8] >> 5;
	pmt->pcr_pid = ((payload[8] << 8) | payload[9]) & 0x1fff;
	pmt->reserved_5 = payload[10] >> 4;
	pmt->program_information_length = ((payload[10] << 8) | payload[11]) & 0x0fff;
	pmt->num_descriptors = descriptors_count(&payload[12], pmt->program_information_length);
	pmt_create_directory(header, pmt, &descriptors_dentry, &streams_dentry, priv);

	uint8_t descriptors_len = descriptors_parse(&payload[12], pmt->num_descriptors, 
			descriptors_dentry, priv);

	uint8_t offset = 12 + descriptors_len;
	pmt->num_programs = 0;
	while (offset < 3 + pmt->section_length - sizeof(pmt->crc)) {
		struct pmt_stream stream;
		stream.stream_type_identifier = payload[offset];
		stream.reserved_1 = (payload[offset+1] >> 5) & 0x7;
		stream.elementary_stream_pid = ((payload[offset+1] << 8) | payload[offset+2]) & 0x1fff;
		stream.reserved_2 = (payload[offset+3] >> 4) & 0x0f; 
		stream.es_information_length = ((payload[offset+3] << 8) | payload[offset+4]) & 0x0fff;

		struct dentry *subdir = NULL;
		pmt_populate_stream_dir(&stream, streams_dentry, &subdir, priv);

		priv->shared_data = (void *) &stream;
		descriptors_parse(&payload[offset+5], 1, subdir, priv);
		priv->shared_data = NULL;

		offset += 5 + stream.es_information_length;
		pmt->num_programs++;
	}
	
	offset = 12 + pmt->program_information_length;

	hashtable_add(priv->table, pmt->dentry.inode, pmt);

	return 0;
}
