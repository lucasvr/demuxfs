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

static const char *pes_parse_audio_video_payload(const char *, uint32_t, uint32_t *);

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
		hashtable_add(priv->pes_tables, key, dentry, NULL);
	}
	return dentry;
}

static int pes_append_to_fifo(struct dentry *dentry, bool pusi,
		const char *payload, uint32_t payload_len, int es_stream_type)
{
	struct fifo_priv *priv_data = dentry ? (struct fifo_priv *) dentry->priv : NULL;
	struct fifo *fifo = priv_data ? priv_data->fifo : NULL;
	int ret = 0;

	if (! fifo || payload_len == 0)
		return 0;

	/* Do not feed the FIFO if no process wants to read from it */
	if (fifo_is_open(fifo)) {
		const char *ptr = payload;
		bool append = true;

		/* Skip delta frames before feeding the FIFO for the first time */
		if (es_stream_type == ES_VIDEO_STREAM) {
			while (payload_len && !IS_NAL_IDC_REFERENCE(ptr)) {
				ptr++;
				payload_len--;
			}
			append = payload_len > 0;
			payload = ptr;
		} else if (es_stream_type == ES_AUDIO_STREAM) {
			while (payload_len && !IS_AAC_LATM_SYNCWORD(ptr)) {
				ptr++;
				payload_len--;
			}
			append = payload_len > 0;
			payload = ptr;
		}
		
		if (append)
			ret = fifo_append(fifo, payload, payload_len);
	}

	if (ret < 0)
		dprintf("Error writing to the FIFO: %d", ret);
	return ret;
}

int pes_parse_audio(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct dentry *es_dentry, *pes_dentry;
	struct av_fifo_priv *priv_data;
	bool is_audio = true;

	if (header->payload_unit_start_indicator && payload_len < 6) {
		TS_WARNING("cannot parse PES header: payload holds less than 6 bytes (%d)", payload_len);
		return -1;
	}

	if (priv->options.parse_pes) {
		const char *data = payload;
		uint32_t data_len = payload_len - 4;

		es_dentry = pes_get_dentry(header, FS_ES_FIFO_NAME, priv);
		if (! es_dentry) {
			TS_WARNING("failed to get ES dentry");
			return -ENOENT;
		}
		priv_data = (struct av_fifo_priv *) es_dentry->priv;

		if (header->payload_unit_start_indicator) {
			int stream_type = pes_identify_stream_id(payload[3]);
			is_audio = stream_type == PES_AUDIO_STREAM;

			/* Flush ES buffer */
			priv_data->pes_packet_length = CONVERT_TO_16(payload[4], payload[5]);
			priv_data->pes_packet_parsed_length = 0;
			priv_data->pes_packet_initialized = true;
			data = pes_parse_audio_video_payload(payload, payload_len, &data_len);
			if (data == NULL) {
				TS_WARNING("failed to parse PES audio/video payload");
				return -1;
			}
			priv_data->pes_packet_parsed_length += data_len;
		} else if (priv_data->pes_packet_initialized) {
			if (priv_data->pes_packet_length == 0) {
				/* Unbounded PES packet */
				data_len = payload_len;
			}
		} else if (! priv_data->pes_packet_initialized) {
			data_len = payload_len;
		}

		if (data && data_len)
			pes_append_to_fifo(es_dentry, header->payload_unit_start_indicator,
				data, data_len, is_audio ? ES_AUDIO_STREAM : ES_OTHER_STREAM);
	}

	pes_dentry = pes_get_dentry(header, FS_PES_FIFO_NAME, priv);
	if (! pes_dentry) {
		dprintf("dentry = NULL");
		return -ENOENT;
	}
	return pes_append_to_fifo(pes_dentry, header->payload_unit_start_indicator,
		payload, payload_len, ES_OTHER_STREAM);
}

int pes_parse_video(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct dentry *es_dentry, *pes_dentry;
	struct av_fifo_priv *priv_data;
	bool is_video = false;

	if (header->payload_unit_start_indicator && payload_len < 6) {
		TS_WARNING("cannot parse PES header: payload holds less than 6 bytes (%d)", payload_len);
		return -1;
	}

	if (priv->options.parse_pes) {
		const char *data = payload;
		uint32_t data_len = payload_len - 4;

		es_dentry = pes_get_dentry(header, FS_ES_FIFO_NAME, priv);
		if (! es_dentry) {
			TS_WARNING("failed to get ES dentry");
			return -ENOENT;
		}

		priv_data = (struct av_fifo_priv *) es_dentry->priv;

		if (header->payload_unit_start_indicator) {
			int stream_type = pes_identify_stream_id(payload[3]);
			uint32_t n = 6;

			/* Flush ES buffer */
			priv_data->pes_packet_length = CONVERT_TO_16(payload[4], payload[5]);
			priv_data->pes_packet_parsed_length = 0;
			priv_data->pes_packet_initialized = true;

			is_video = stream_type == PES_VIDEO_STREAM;

			if (stream_type != PES_PROGRAM_STREAM_MAP &&
				stream_type != PES_PADDING_STREAM &&
				stream_type != PES_PRIVATE_STREAM_2 &&
				stream_type != PES_ECM_STREAM &&
				stream_type != PES_EMM_STREAM &&
				stream_type != PES_PROGRAM_STREAM_DIRECTORY &&
				stream_type != PES_DSMCC_STREAM &&
				stream_type != PES_H222_1_TYPE_E) {
				/* This is an audio/video packet */
				data = pes_parse_audio_video_payload(payload, payload_len, &data_len);
				if (data == NULL) {
					TS_WARNING("failed to parse PES audio/video payload");
					return -1;
				}
			} else if (stream_type == PES_PROGRAM_STREAM_MAP ||
				stream_type == PES_PRIVATE_STREAM_2 ||
				stream_type == PES_ECM_STREAM ||
				stream_type == PES_EMM_STREAM ||
				stream_type == PES_PROGRAM_STREAM_DIRECTORY ||
				stream_type == PES_DSMCC_STREAM ||
				stream_type == PES_H222_1_TYPE_E) {
				/* Payload holds packet data bytes alone */
				data = &payload[n];
				data_len -= n;
			} else if (stream_type == PES_PADDING_STREAM) {
				/* Payload holds padding bytes only */
				data = NULL;
				data_len = 0;
			} else {
				TS_WARNING("*** unknown PES block: stream_type=%d (pid %#x) ***", stream_type, header->pid);
				data = NULL;
				data_len = 0;
			}
			priv_data->pes_packet_parsed_length += data_len;
		} else if (priv_data->pes_packet_initialized) {
			if (priv_data->pes_packet_length == 0) {
				/* Unbounded PES packet */
				data_len = payload_len;
			}
		} else if (! priv_data->pes_packet_initialized) {
			data = NULL;
			data_len = 0;
		}

		if (data && data_len)
			pes_append_to_fifo(es_dentry, header->payload_unit_start_indicator,
				data, data_len, is_video ? ES_VIDEO_STREAM : ES_OTHER_STREAM);
	}

	pes_dentry = pes_get_dentry(header, FS_PES_FIFO_NAME, priv);
	if (! pes_dentry) {
		dprintf("dentry = NULL");
		return -ENOENT;
	}
	return pes_append_to_fifo(pes_dentry, header->payload_unit_start_indicator,
		payload, payload_len, ES_OTHER_STREAM);
}

static const char *pes_parse_audio_video_payload(const char *payload, uint32_t payload_len,
		uint32_t *data_len)
{
	/* Initialize 'n' right before the flags byte:
	 *
	 * packet_start_code_prefix  24
	 * stream_id                  8
	 * PES_packet_length         16
	 *
	 * '10'                       2
	 * PES_scrambling_control     2
	 * PES_priority               1
	 * data_alignment_indicator   1
	 * copyright                  1
	 * original_or_copy           1 */
	int n = 7;

	/* PTS_DTS_flags              2
	 * ESCR_flag                  1
	 * ES_rate_flag               1
	 * DSM_trick_mode_flag        1
	 * additional_copy_info_flag  1
	 * PES_CRC_flag               1
	 * PES_extension_flag         1 */
	uint8_t pts_dts_flags = (payload[n] & 0xc0) >> 6;
	uint8_t escr_flag = (payload[n] & 0x20) >> 5;
	uint8_t es_rate_flag = (payload[n] & 0x10) >> 4;
	uint8_t dsm_trick_mode_flag = (payload[n] & 0x08) >> 3;
	uint8_t additional_copy_info_flag = (payload[n] & 0x04) >> 2;
	uint8_t pes_crc_flag = (payload[n] & 0x02) >> 1;
	uint8_t pes_extension_flag = (payload[n] & 0x01);
	n++;

	/* n == 8 */
	//uint8_t pes_header_data_length = payload[n];
	n++;

	if (pts_dts_flags == 0x2) {
		/* '0010'                 4
		 * PTS[32..30]            3
		 * marker_bit             1
		 * PTS[29..15]           15
		 * marker_bit             1
		 * PTS[14..0]            15
		 * marker_bit             1 */
		n += 5;
	} else if (pts_dts_flags == 0x3) {
		/* '0010'                 4
		 * PTS[32..30]            3
		 * marker_bit             1
		 * PTS[29..15]           15
		 * marker_bit             1
		 * PTS[14..0]            15
		 * marker_bit             1
		 *
		 * '0001'                 4
		 * DTS[32..30]            3
		 * marker_bit             1
		 * DTS[29..15]           15
		 * marker_bit             1
		 * DTS[14..0]            15
		 * marker_bit             1 */
		n += 10;
	}
	if (escr_flag) {
		/* reserved               2
		 * ESCR_base[32..30]      3
		 * marker_bit             1
		 * ESCR_base[29..15]     15
		 * marker_bit             1
		 * ESCR_base[14..0]      15
		 * marker_bit             1
		 * ESCR_extension         9
		 * marker_bit             1 */
		n += 6;
	}
	if (es_rate_flag) {
		/* marker_bit             1
		 * ES_rate               22
		 * marker_bit             1 */
		n += 3;
	}
	if (dsm_trick_mode_flag) {
		uint8_t trick_mode_control = (payload[n] & 0xe) >> 1; /* 3 bits */
		if (trick_mode_control == TRICK_MODE_FAST_FORWARD) {
			/* field_id             2
			 * intra_slice_refresh  1
			 * frequency_truncation 2 */
			n++;
		} else if (trick_mode_control == TRICK_MODE_SLOW_MOTION) {
			/* rep_cntrl            5 */
			n++;
		} else if (trick_mode_control == TRICK_MODE_FREEZE_FRAME) {
			/* field_id             2
			 * reserved             3 */
			n++;
		} else if (trick_mode_control == TRICK_MODE_FAST_REVERSE) {
			/* field_id             2
			 * intra_slice_refresh  1
			 * frequency_truncation 2 */
			n++;
		} else if (trick_mode_control == TRICK_MODE_SLOW_REVERSE) {
			/* rep_cntrl            5 */
			n++;
		} else {
			/* reserved             5 */
			n++;
		}
	}
	if (additional_copy_info_flag) {
		/* marker_bit             1
		 * additional_copy_info   7 */
		n += 1;
	}
	if (pes_crc_flag) {
		/* previous_pes_packet_crc */
		n += 2;
	}
	if (pes_extension_flag) {
		uint8_t pes_private_data_flag = (payload[n] & 0x80) >> 7;
		uint8_t pack_header_field_flag = (payload[n] & 0x40) >> 6;
		uint8_t program_packet_sequence_counter_flag = (payload[n] & 0x20) >> 5;
		uint8_t p_std_buffer_flag = (payload[n] & 0x10) >> 4;
		//uint8_t reserved = (payload[n] & 0x0c) >> 2;
		uint8_t pes_extension_flag_2 = (payload[n] & 0x3);
		n++;

		if (pes_private_data_flag) {
			/* PES_private_data */
			n += 16;
		}
		if (pack_header_field_flag) {
			uint8_t pack_field_length = payload[n];
			/* pack_header(); */
			n += pack_field_length;
		}
		if (program_packet_sequence_counter_flag) {
			/* marker_bit                      1
			 * program_packet_sequence_counter 7
			 * marker_bit                      1
			 * MPEG1_MPEG2_identifier          1
			 * original_stuff_length           6 */
			n += 2;
		}
		if (p_std_buffer_flag) {
			/* '01'                2
			 * P-SDT_buffer_scale  1
			 * P-SDT_buffer_size  13 */
			n += 2;
		}
		if (pes_extension_flag_2) {
			/* marker_bit                 1
			 * PES_extension_field_length 7
			 * for (i=0; i<PES_extension_field_length; i++)
			 *   reserved                 8 */
			uint8_t pes_extension_field_length = payload[n] & 0x7f;
			n += 1 + pes_extension_field_length;
		}
	}

	/* Stuffing bytes */
	while (payload[n] == 0xff)
		n++;

	/* PES packet data bytes */
	if (n > payload_len) {
		TS_ERROR("PES packet offset(%d) > payload_len(%d). Unbound stream?", n, payload_len);
		return NULL;
	}

	*data_len = payload_len - n;
	return &payload[n];
}

int pes_parse_other(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	return 0;
}
