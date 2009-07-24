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
#include "byteops.h"
#include "buffer.h"
#include "fifo.h"
#include "hash.h"
#include "ts.h"
#include "tables/psi.h"
#include "tables/pes.h"
#include "dsm-cc/dsmcc.h"
#include "dsm-cc/dii.h"
#include "dsm-cc/ddb.h"

struct pes_header {
	int      stream_id;
	uint32_t packet_length;
};

int pes_identify_stream_id(uint8_t stream_id)
{
	switch (stream_id) {
		case 0xbc: return PES_PROGRAM_STREAM_MAP;
		case 0xbd: return PES_PRIVATE_STREAM_1;
		case 0xbe: return PES_PADDING_STREAM;
		case 0xbf: return PES_PRIVATE_STREAM_2;
		case 0xc0 ... 0xdf:
				   return PES_AUDIO_STREAM;
		case 0xe0 ... 0xef:
				   return PES_VIDEO_STREAM;
		case 0xf0: return PES_ECM_STREAM;
		case 0xf1: return PES_EMM_STREAM;
		case 0xf2: return PES_DSMCC_STREAM;
		case 0xf3: return PES_ISO_IEC_13522_STREAM; 
		case 0xf4: return PES_H222_1_TYPE_A; 
		case 0xf5: return PES_H222_1_TYPE_B;
		case 0xf6: return PES_H222_1_TYPE_C;
		case 0xf7: return PES_H222_1_TYPE_D;
		case 0xf8: return PES_H222_1_TYPE_E;
		case 0xf9: return PES_ANCILLARY_STREAM;
		case 0xfa: return PES_SL_PACKETIZED_STREAM;
		case 0xfb: return PES_FLEXMUX_STREAM;
		case 0xfc ... 0xfe:
				   return PES_RESERVED_DATA_STREAM;
		case 0xff: return PES_PROGRAM_STREAM_DIRECTORY;
		default:   return PES_UNKNOWN_STREAM;
	}
}

static struct dentry *pes_get_dentry(const struct ts_header *header, 
		const char *fifo_name, struct demuxfs_data *priv)
{
	struct dentry *slink, *dentry = NULL;
	char pathname[PATH_MAX];
	ino_t key = header->pid << 1 | (strcmp(fifo_name, FS_ES_FIFO_NAME) == 0 ? 0 : 1);

	dentry = hashtable_get(priv->pes_tables, key);
	if (! dentry) {
		sprintf(pathname, "/%s/%#x", FS_STREAMS_NAME, header->pid);
		slink = fsutils_get_dentry(priv->root, pathname);
		if (! slink) {
			dprintf("couldn't get a dentry for '%s'", pathname);
			return NULL;
		}

		sprintf(pathname, "%s/%s", slink->contents, fifo_name);
		dentry = fsutils_get_dentry(priv->root, pathname);
		if (! dentry) {
			dprintf("couldn't get a dentry for '%s'", pathname);
			return NULL;
		}
		hashtable_add(priv->pes_tables, key, dentry);
	}
	return dentry;
}

static int pes_append_to_fifo(struct dentry *dentry, bool pusi,
		const char *payload, uint32_t payload_len)
{
	int ret = 0;

	if (! dentry || payload_len == 0)
		return 0;

	pthread_mutex_lock(&dentry->mutex);
	/* Do not feed the FIFO if no process wants to read from it */
	if (dentry->refcount > 0) {
		bool append = true;
		struct fifo_priv *priv_data = (struct fifo_priv *) dentry->priv;

		if (fifo_is_flushed(priv_data->fifo) && !IS_NAL_IDC_REFERENCE(payload)) {
			/* Skip delta frames before start feeding the FIFO */
			append = false;
		}
		
		if (append) {
			ret = fifo_append(priv_data->fifo, payload, payload_len);
			/* Awake reader, if any */
			sem_post(&dentry->semaphore);
		}
	}
	pthread_mutex_unlock(&dentry->mutex);

	if (ret < 0)
		dprintf("Error writing to the FIFO: %d", ret);
	return ret;
}

int pes_parse_audio(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct dentry *pes_dentry;

	pes_dentry = pes_get_dentry(header, FS_PES_FIFO_NAME, priv);
	if (! pes_dentry)
		return -ENOENT;

	return pes_append_to_fifo(pes_dentry, false, payload, payload_len);
}

int pes_parse_video(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct dentry *es_dentry, *pes_dentry;

	if (header->payload_unit_start_indicator && payload_len < 6) {
		TS_WARNING("cannot parse PES header: contents is smaller than 6 bytes (%d)", payload_len);
		return -1;
	}

	if (priv->options.parse_pes) {
		struct video_fifo_priv *priv_data;
		const char *data = payload;
		uint32_t data_len = payload_len - 4;

		es_dentry = pes_get_dentry(header, FS_ES_FIFO_NAME, priv);
		if (! es_dentry)
			return -ENOENT;

		priv_data = (struct video_fifo_priv *) es_dentry->priv;

		if (header->payload_unit_start_indicator) {
			uint32_t n = 6, offset = 0;

			/* Flush ES buffer */
			priv_data->pes_packet_length = CONVERT_TO_16(payload[4], payload[5]);
			priv_data->pes_packet_parsed_length = 0;
			priv_data->pes_packet_initialized = true;

			if (IS_H222_PES(payload)) {
				/* This is a H.222.0 (13818-1) PES packet */
				/* payload[8] contains the PES header data length */
				offset = payload[8] + 9;
			} else {
				/* This is a MPEG-1 (11172-1) PES packet */
				while (n < payload_len && payload[n] == 0xff)
					/* Skip padding byte */
					n++;
				if ((payload[n] & 0xC0) == 0x40)
					/* Skip buffer scale/size */
					n += 2;
				if ((payload[n] & 0xF0) == 0x20)
					/* Skip PTS */
					n += 5;
				else if ((payload[n] & 0xF0) == 0x30)
					/* Skip PTS/DTS */
					n += 10;
				else
					/* There's nothing we can do */
					n++;
				offset = n;
			}

			if (offset > payload_len) {
				dprintf("Error: offset(%d) > payload_len(%d)", offset, payload_len);
				return -1;
			}

			data = &payload[offset];
			data_len = payload_len - offset - 4;
		} else if (priv_data->pes_packet_initialized) {
			int cur_size = priv_data->pes_packet_parsed_length;
			int max_size = priv_data->pes_packet_length;
			if (max_size != 0) {
				/* Not an unbounded PES packet */
				if ((cur_size + payload_len) >= max_size) {
					data_len = max_size - payload_len;
					/* XXX: potentially discarding next packet */
				}
			}
		} else if (! priv_data->pes_packet_initialized) {
			data_len = 0;
		}
		
		if (data_len)
			pes_append_to_fifo(es_dentry, header->payload_unit_start_indicator, data, data_len);
	}

	pes_dentry = pes_get_dentry(header, FS_PES_FIFO_NAME, priv);
	if (! pes_dentry) {
		dprintf("dentry = NULL");
		return -ENOENT;
	}
	return pes_append_to_fifo(pes_dentry, header->payload_unit_start_indicator, payload, payload_len);
}

int pes_parse_other(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	return 0;
}
