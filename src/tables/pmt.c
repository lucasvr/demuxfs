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
#include "fsutils.h"
#include "xattr.h"
#include "hash.h"
#include "fifo.h"
#include "ts.h"
#include "byteops.h"
#include "snapshot.h"
#include "descriptors.h"
#include "stream_type.h"
#include "component_tag.h"
#include "tables/psi.h"
#include "tables/pmt.h"
#include "tables/pes.h"
#include "dsm-cc/dsmcc.h"

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

void pmt_free(struct pmt_table *pmt)
{
	if (pmt->dentry && pmt->dentry->name)
		fsutils_dispose_tree(pmt->dentry);
	else if (pmt->dentry)
		/* Dentry has simply been calloc'ed */
		free(pmt->dentry);

	/* Free the pmt table structure */
	pmt->dentry = NULL;
	free(pmt);
}

static void pmt_populate(struct pmt_table *pmt, struct dentry *parent, 
		struct demuxfs_data *priv)
{
	CREATE_FILE_NUMBER(parent, pmt, pcr_pid);
	CREATE_FILE_NUMBER(parent, pmt, program_information_length);
}

static void pmt_populate_stream_dir(struct pmt_stream *stream, const char *descriptor_info,
		struct dentry *version_dentry, struct dentry **subdir, struct demuxfs_data *priv)
{
	uint8_t tag = descriptor_info ? descriptor_info[0] : 0;
	uint8_t component_tag = descriptor_info ? descriptor_info[2] : 0;
	bool is_primary = false, is_secondary = false;
	struct dentry *parent = NULL;
	const char *streams_name = FS_RESERVED_STREAMS_NAME;
	char dirname[16], stream_type[256], es_path[PATH_MAX], *es;

	// STREAM_IDENTIFIER_DESCRIPTOR
	if (tag == 0x52) {
		bool is_reserved = false;
		if (component_is_video(component_tag, &is_primary))
			streams_name = component_is_one_seg(component_tag) ? FS_ONE_SEG_VIDEO_STREAMS_NAME : FS_VIDEO_STREAMS_NAME;
		else if (component_is_audio(component_tag, &is_primary))
			streams_name = component_is_one_seg(component_tag) ? FS_ONE_SEG_AUDIO_STREAMS_NAME : FS_AUDIO_STREAMS_NAME;
		else if (component_is_caption(component_tag, &is_primary))
			streams_name = FS_CLOSED_CAPTION_STREAMS_NAME;
		else if (component_is_superimposed(component_tag, &is_primary))
			streams_name = FS_SUPERIMPOSED_STREAMS_NAME;
		else if (component_is_object_carousel(component_tag, &is_primary))
			streams_name = FS_OBJECT_CAROUSEL_STREAMS_NAME;
		else if (component_is_data_carousel(component_tag, &is_primary))
			streams_name = FS_DATA_CAROUSEL_STREAMS_NAME;
		else if (component_is_event_message(component_tag))
			streams_name = FS_EVENT_MESSAGE_STREAMS_NAME;
		else
			is_reserved = true;
		if (! is_primary && ! is_reserved)
			is_secondary = true;
	} else if (stream_type_is_data_carousel(stream->stream_type_identifier)) {
		streams_name = FS_DATA_CAROUSEL_STREAMS_NAME;
	} else if (stream_type_is_event_message(stream->stream_type_identifier)) {
		streams_name = FS_EVENT_MESSAGE_STREAMS_NAME;
	} else if (stream_type_is_mpe(stream->stream_type_identifier)) {
		streams_name = FS_MPE_STREAMS_NAME;
	} else if (stream_type_is_object_carousel(stream->stream_type_identifier)) {
		streams_name = FS_OBJECT_CAROUSEL_STREAMS_NAME;
	} else if (stream_type_is_video(stream->stream_type_identifier)) {
		streams_name = FS_VIDEO_STREAMS_NAME;
	} else if (stream_type_is_audio(stream->stream_type_identifier)) {
		streams_name = FS_AUDIO_STREAMS_NAME;
	}

	parent = CREATE_DIRECTORY(version_dentry, streams_name);

	/* Create a directory with this stream's PID number in /PMT/<pid>/Current/<streams_name>/ */
	sprintf(dirname, "%#04x", stream->elementary_stream_pid);
	*subdir = CREATE_DIRECTORY(parent, dirname);

	/* Create a 'Primary' symlink pointing to <streams_name> if it happens to be the primary component */
	if (is_primary)
		CREATE_SYMLINK(parent, FS_PRIMARY_NAME, dirname);
	else if (is_secondary && ! fsutils_get_child(parent, FS_SECONDARY_NAME))
		CREATE_SYMLINK(parent, FS_SECONDARY_NAME, dirname);
	else if (is_secondary) {
		/* TODO: The descriptor with the lowest component_tag will become the secondary stream */
	}

	/* Create a symlink in /Streams pointing to this new directory */
	es = fsutils_path_walk((*subdir), es_path, sizeof(es_path));
	if (es) {
		struct dentry *streams_dir = CREATE_DIRECTORY(priv->root, FS_STREAMS_NAME);
		if (es > es_path + 2) {
			*(--es) = '.';
			*(--es) = '.';
		}
		CREATE_SYMLINK(streams_dir, dirname, es);
	}

	/* Create a FIFO which will contain this stream's PES contents */
	if (stream_type_is_audio(stream->stream_type_identifier) ||
		stream_type_is_video(stream->stream_type_identifier)) {
		int obj_type = stream_type_is_video(stream->stream_type_identifier) ? 
			OBJ_TYPE_VIDEO_FIFO : OBJ_TYPE_AUDIO_FIFO;

		CREATE_FIFO((*subdir), obj_type, FS_PES_FIFO_NAME, priv);

		if (priv->options.parse_pes) {
			/* Create a FIFO which will contain this stream's ES contents */
			struct dentry *es_dentry = CREATE_FIFO((*subdir), obj_type, FS_ES_FIFO_NAME, priv);
#ifdef USE_FFMPEG
			if (stream_type_is_video(stream->stream_type_identifier))
				/* Create a file named snapshot.ppm */
				CREATE_SNAPSHOT_FILE((*subdir), FS_VIDEO_SNAPSHOT_NAME, es_dentry, priv);
#endif
		}
	}
	if (stream_type_is_data_carousel(stream->stream_type_identifier) ||
		stream_type_is_object_carousel(stream->stream_type_identifier)) {
		char target[PATH_MAX];
		if (0) {
			sprintf(target, "../../../../../DDB/%#02x/%s/BIOP", stream->elementary_stream_pid, FS_CURRENT_NAME);
			struct dentry *biop_symlink = CREATE_SYMLINK(*subdir, "BIOP", target);
		} else {
			sprintf(target, "../../../../../DDB/%#02x/%s", stream->elementary_stream_pid, FS_CURRENT_NAME);
			struct dentry *biop_symlink = CREATE_SYMLINK(*subdir, "BIOP", target);
		}
	}

	struct formatted_descriptor f;
	snprintf(stream_type, sizeof(stream_type), "%s [%#x]",
			stream_type_to_string(stream->stream_type_identifier),
			stream->stream_type_identifier);
	f.stream_type_identifier = stream_type;
	CREATE_FILE_STRING((*subdir), &f, stream_type_identifier, XATTR_FORMAT_STRING_AND_NUMBER);
	CREATE_FILE_NUMBER((*subdir), stream, elementary_stream_pid);
	CREATE_FILE_NUMBER((*subdir), stream, es_information_length);
	
	if (stream_type_is_data_carousel(stream->stream_type_identifier) ||
		stream_type_is_event_message(stream->stream_type_identifier) ||
		stream_type_is_mpe(stream->stream_type_identifier) ||
		stream_type_is_object_carousel(stream->stream_type_identifier)) {
		/* Assign this PID to the DSM-CC parser */
		if (! hashtable_get(priv->psi_parsers, stream->elementary_stream_pid))
			hashtable_add(priv->psi_parsers, stream->elementary_stream_pid, dsmcc_parse, NULL);
	} else if (stream_type_is_audio(stream->stream_type_identifier)) {
		/* Assign this to the PES audio parser */
		if (! hashtable_get(priv->pes_parsers, stream->elementary_stream_pid))
			hashtable_add(priv->pes_parsers, stream->elementary_stream_pid, pes_parse_audio, NULL);
	} else if (stream_type_is_video(stream->stream_type_identifier)) {
		/* Assign this to the PES video parser */
		if (! hashtable_get(priv->pes_parsers, stream->elementary_stream_pid))
			hashtable_add(priv->pes_parsers, stream->elementary_stream_pid, pes_parse_video, NULL);
	} else if (! hashtable_get(priv->pes_parsers, stream->elementary_stream_pid)) {
		/* Assign this to the PES generic parser */
		TS_INFO("Will parse pid %#x / stream_type %#x using a generic PES parser", 
				stream->elementary_stream_pid, stream->stream_type_identifier);
		hashtable_add(priv->psi_parsers, stream->elementary_stream_pid, pes_parse_other, NULL);
	}
}

static void pmt_create_directory(const struct ts_header *header, struct pmt_table *pmt, 
		struct dentry **version_dentry, struct demuxfs_data *priv)
{
	/* Create a directory named "PMT" at the root filesystem if it doesn't exist yet */
	struct dentry *pmt_dir = CREATE_DIRECTORY(priv->root, FS_PMT_NAME);

	/* Create a directory named "<pmt_pid>" and populate it with files */
	asprintf(&pmt->dentry->name, "%#04x", header->pid);
	pmt->dentry->mode = S_IFDIR | 0555;
	CREATE_COMMON(pmt_dir, pmt->dentry);
	
	/* Create the versioned dir and update the Current symlink */
	*version_dentry = fsutils_create_version_dir(pmt->dentry, pmt->version_number);

	psi_populate((void **) &pmt, *version_dentry);
	pmt_populate(pmt, *version_dentry, priv);
}

int pmt_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct pmt_table *current_pmt = NULL;
	struct pmt_table *pmt = (struct pmt_table *) calloc(1, sizeof(struct pmt_table));
	assert(pmt);
	
	pmt->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(pmt->dentry);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) pmt, payload, payload_len);
	if (ret < 0) {
		pmt_free(pmt);
		return 0;
	}
	pmt_check_header(pmt);
	
	/* Set hash key and check if there's already one version of this table in the hash */
	pmt->dentry->inode = TS_PACKET_HASH_KEY(header, pmt);
	current_pmt = hashtable_get(priv->psi_tables, pmt->dentry->inode);
	
	/* Check whether we should keep processing this packet or not */
	if (! pmt->current_next_indicator || (current_pmt && current_pmt->version_number == pmt->version_number)) {
		pmt_free(pmt);
		return 0;
	}
	
	TS_INFO("PMT parser: pid=%#x, table_id=%#x, current_pmt=%p, pmt->version_number=%#x, len=%d", 
			header->pid, pmt->table_id, current_pmt, pmt->version_number, payload_len);

	/* Parse PMT specific bits */
	struct dentry *version_dentry;
	pmt->reserved_4 = payload[8] >> 5;
	pmt->pcr_pid = CONVERT_TO_16(payload[8], payload[9]) & 0x1fff;
	pmt->reserved_5 = payload[10] >> 4;
	pmt->program_information_length = CONVERT_TO_16(payload[10], payload[11]) & 0x0fff;
	pmt->num_descriptors = descriptors_count(&payload[12], pmt->program_information_length);
	pmt_create_directory(header, pmt, &version_dentry, priv);

	uint32_t descriptors_len = descriptors_parse(&payload[12], pmt->num_descriptors, 
			version_dentry, priv);

	uint32_t offset = 12 + descriptors_len;
	pmt->num_programs = 0;
	while (offset < 3 + pmt->section_length - sizeof(pmt->crc)) {
		struct pmt_stream stream;
		stream.stream_type_identifier = payload[offset];
		stream.reserved_1 = (payload[offset+1] >> 5) & 0x7;
		stream.elementary_stream_pid = CONVERT_TO_16(payload[offset+1], payload[offset+2]) & 0x1fff;
		stream.reserved_2 = (payload[offset+3] >> 4) & 0x0f; 
		stream.es_information_length = CONVERT_TO_16(payload[offset+3], payload[offset+4]) & 0x0fff;

		uint32_t es_i = 0;
		if (! stream.es_information_length) {
				struct dentry *subdir = NULL;
				pmt_populate_stream_dir(&stream, NULL, version_dentry, &subdir, priv);
		} else {
			while (es_i < stream.es_information_length) {
				struct dentry *subdir = NULL;
				const char *descriptor_info = &payload[offset+5+es_i];
				pmt_populate_stream_dir(&stream, descriptor_info, version_dentry, &subdir, priv);

				priv->shared_data = (void *) &stream;
				es_i += descriptors_parse(descriptor_info, 1, subdir, priv);
				priv->shared_data = NULL;
			}
		}

		offset += 5 + stream.es_information_length;
		pmt->num_programs++;
	}
	offset = 12 + pmt->program_information_length;

	if (current_pmt) {
		fsutils_migrate_children(current_pmt->dentry, pmt->dentry);
		hashtable_del(priv->psi_tables, current_pmt->dentry->inode);
		/* Invalidate all items from the PES hash table */
		hashtable_invalidate_contents(priv->pes_tables);
	}
	hashtable_add(priv->psi_tables, pmt->dentry->inode, pmt, (hashtable_free_function_t) pmt_free);

	return 0;
}
