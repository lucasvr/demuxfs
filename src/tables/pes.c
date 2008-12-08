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

#if 0
struct program_stream_pack_header { /* mpeg2.pdf, table 2-33, pg 73 */
	uint32_t pack_start_code;
	uint8_t  _0_1:2;
	uint8_t  system_clock_reference_base_32_30:3;
	uint8_t  marker_bit_1:3;
	uint16_t system_clock_reference_base_29_15:15;
	uint16_t marker_bit_2:1;
	uint16_t system_clock_reference_base_14_0:15;
	uint16_t marker_bit_3:1;
	uint16_t system_clock_reference_extension:9;
	uint16_t marker_bit_4:7;
	uint32_t program_mux_rate:22;
	uint32_t marker_bit_5:1;
	uint32_t marker_bit_6:1;
	uint32_t reserved:5;
	uint32_t pack_stuffing_length:3;
};
#endif

#define TRICK_MODE_FAST_FORWARD 0x00
#define TRICK_MODE_SLOW_MOTION  0x01
#define TRICK_MODE_FREEZE_FRAME 0x02
#define TRICK_MODE_FAST_REVERSE 0x03
#define TRICK_MODE_SLOW_REVERSE 0x04

enum {
	PROGRAM_STREAM_MAP,
	PRIVATE_STREAM_1,
	PADDING_STREAM,
	PRIVATE_STREAM_2,
	AUDIO_STREAM,
	VIDEO_STREAM,
	ECM_STREAM,
	EMM_STREAM,
	DSMCC_STREAM,
	ISO_IEC_13522_STREAM,
	H222_1_TYPE_A,
	H222_1_TYPE_B,
	H222_1_TYPE_C,
	H222_1_TYPE_D,
	H222_1_TYPE_E,
	ANCILLARY_STREAM,
	SL_PACKETIZED_STREAM,
	FLEXMUX_STREAM,
	RESERVED_DATA_STREAM,
	PROGRAM_STREAM_DIRECTORY,
	UNKNOWN_STREAM
};

static int pes_identify_stream_id(uint8_t stream_id)
{
	switch (stream_id) {
		case 0xbc: return PROGRAM_STREAM_MAP;
		case 0xbd: return PRIVATE_STREAM_1;
		case 0xbe: return PADDING_STREAM;
		case 0xbf: return PRIVATE_STREAM_2;
		case 0xc0 ... 0xdf:
				   return AUDIO_STREAM;
		case 0xe0 ... 0xef:
				   return VIDEO_STREAM;
		case 0xf0: return ECM_STREAM;
		case 0xf1: return EMM_STREAM;
		case 0xf2: return DSMCC_STREAM;
		case 0xf3: return ISO_IEC_13522_STREAM; 
		case 0xf4: return H222_1_TYPE_A; 
		case 0xf5: return H222_1_TYPE_B;
		case 0xf6: return H222_1_TYPE_C;
		case 0xf7: return H222_1_TYPE_D;
		case 0xf8: return H222_1_TYPE_E;
		case 0xf9: return ANCILLARY_STREAM;
		case 0xfa: return SL_PACKETIZED_STREAM;
		case 0xfb: return FLEXMUX_STREAM;
		case 0xfc ... 0xfe:
				   return RESERVED_DATA_STREAM;
		case 0xff: return PROGRAM_STREAM_DIRECTORY;
		default:   return UNKNOWN_STREAM;
	}
}

struct pes_other {
	uint8_t _1_0:2;
	uint8_t pes_scrambling_control:2;
	uint8_t pes_priority:1;
	uint8_t data_alignment_indicator:1;
	uint8_t copyright:1;
	uint8_t original_or_copy:1;

	uint8_t pts_dts_flags:2;
	uint8_t escr_flag:1;
	uint8_t es_rate_flag:1;
	uint8_t dsm_trick_mode_flag:1;
	uint8_t additional_copy_info_flag:1;
	uint8_t pes_crc_flag:1;
	uint8_t pes_extension_flag:1;

	uint8_t pes_header_data_length:8;
};

/**
 * We do not present PES data in the filesystem tree. We do, however, parse them to some extent.
 * This function returns the new offset within @payload from where the parsing should continue.
 *
 * TODO: verify offset against @payload_len so that we never go out of bounds in @payload.
 */
static int pes_parse_other(const char *payload, uint8_t payload_len, struct demuxfs_data *priv)
{
	int offset;
	struct pes_other other;
	other._1_0 = (payload[6] >> 6) & 0x03;
	other.pes_scrambling_control = (payload[6] >> 4) & 0x03;
	other.pes_priority = (payload[6] >> 3) & 0x01;
	other.data_alignment_indicator = (payload[6] >> 2) & 0x01;
	other.copyright = (payload[6] >> 1) & 0x01;
	other.original_or_copy = payload[6] & 0x01;

	other.pts_dts_flags = (payload[7] >> 6) & 0x03;
	other.escr_flag = (payload[7] >> 5) & 0x01;
	other.es_rate_flag = (payload[7] >> 4) & 0x01;
	other.dsm_trick_mode_flag = (payload[7] >> 3) & 0x01;
	other.additional_copy_info_flag = (payload[7] >> 2) & 0x01;
	other.pes_crc_flag = (payload[7] >> 1) & 0x01;
	other.pes_extension_flag = payload[7] & 0x01;

	other.pes_header_data_length = payload[8];

	/* Next read starts at offset 9 */
	offset = 9;

	if (other.pts_dts_flags == 0x02) {
		uint8_t  _0_0_1_0 = (payload[9] >> 4) & 0x0f;
		if (_0_0_1_0 != 0x02)
			TS_WARNING("[1] PTS: expected fixed value 0x02, found %#x", _0_0_1_0);

		/* PTS is encoded as a 33-bit variable */
		uint64_t pts = (payload[9] << 30);
		uint64_t pts_2 = CONVERT_TO_16(payload[10], payload[11]) & 0xfffe;
		uint64_t pts_3 = CONVERT_TO_16(payload[12], payload[13]) & 0xfffe;
		pts |= ((pts_2 << 14) | (pts_3 >> 1));
		dprintf("[1] PTS=%#llx", pts & 0x1ffffffff);

		uint8_t  marker_1 = payload[9] & 0x01;
		uint8_t  marker_2 = payload[11] & 0x01;
		uint8_t  marker_3 = payload[13] & 0x01;
		if (!marker_1 || !marker_2 || !marker_3)
			TS_WARNING("[1] PTS: marker_1=%#x, marker_2=%#x, marker_3=%#x", marker_1, marker_2, marker_3);

		/* Next read starts at offset 14 */
		offset = 14;
	} else if (other.pts_dts_flags == 0x03) {
		/* Parse Presentation Time Stamp */
		uint8_t  _0_0_1_1 = (payload[9] >> 4) & 0x0f;
		if (_0_0_1_1 != 0x03)
			TS_WARNING("[2] PTS: expected fixed value 0x03, found %#x", _0_0_1_1);

		/* PTS is encoded as a 33-bit variable */
		uint64_t pts = (payload[9] << 30);
		uint64_t pts_2 = CONVERT_TO_16(payload[10], payload[11]) & 0xfffe;
		uint64_t pts_3 = CONVERT_TO_16(payload[12], payload[13]) & 0xfffe;
		pts |= ((pts_2 << 14) | (pts_3 >> 1));
		dprintf("[2] PTS=%#llx", pts & 0x1ffffffff);

		uint8_t  marker_1 = payload[9] & 0x01;
		uint8_t  marker_2 = payload[11] & 0x01;
		uint8_t  marker_3 = payload[13] & 0x01;
		if (!marker_1 || !marker_2 || !marker_3)
			TS_WARNING("[2] PTS: marker_1=%#x, marker_2=%#x, marker_3=%#x", marker_1, marker_2, marker_3);

		/* Parse Decoding Time Stamp */
		uint8_t  _0_0_0_1 = (payload[14] >> 4) & 0x0f;
		if (_0_0_0_1 != 0x01)
			TS_WARNING("[2] DTS: expected fixed value 0x03, found %#x", _0_0_0_1);

		/* DTS is encoded as a 33-bit variable, too */
		uint64_t dts = (payload[14] << 30);
		uint64_t dts_2 = CONVERT_TO_16(payload[15], payload[16]) & 0xfffe;
		uint64_t dts_3 = CONVERT_TO_16(payload[17], payload[18]) & 0xfffe;
		dts |= ((dts_2 << 14) | (dts_3 >> 1));
		dprintf("[2] DTS=%#llx", dts & 0x1ffffffff);

		uint8_t  marker_4 = payload[14] & 0x01;
		uint8_t  marker_5 = payload[16] & 0x01;
		uint8_t  marker_6 = payload[18] & 0x01;
		if (!marker_4 || !marker_5 || !marker_6)
			TS_WARNING("[2] DTS: marker_4=%#x, marker_5=%#x, marker_6=%#x", marker_4, marker_5, marker_6);
		
		/* Next read starts at offset 19 */
		offset = 19;
	}
	if (other.escr_flag == 0x01) {
		// --11 1x11   1111 1111   1111 1x11   1111 1111   1111 1xyy   yyyy yyyx
		// 0x38  0x3      0xff     0xf8 0x03      0xff       0xf8
		uint64_t escr = (payload[offset] & 0x38) << 27;
		escr |= (payload[offset] & 0x03) << 28;
		escr |= (payload[offset+1] << 20);
		escr |= (payload[offset+2] & 0xf8) << 12;
		escr |= (payload[offset+2] & 0x03) << 13;
		escr |= (payload[offset+3] << 5);
		escr |= (payload[offset+4] & 0xf8) >> 3;
		dprintf("[3] ESCR=%#llx", escr & 0x1ffffffff);

		uint16_t escr_extension = (payload[offset+4] & 0x03) << 7;
		escr_extension |= (payload[offset+5] >> 1);
		dprintf("[3] ESCR_extension=%#x", escr_extension);
		
		uint8_t marker_1 = (payload[offset] >> 2) & 0x01;
		uint8_t marker_2 = (payload[offset+2] >> 2) & 0x01;
		uint8_t marker_3 = (payload[offset+4] >> 2) & 0x01;
		uint8_t marker_4 = (payload[offset+5] & 0x01);
		if (!marker_1 || !marker_2 || !marker_3 || !marker_4)
			TS_WARNING("[3] ESCR: marker_1=%#x, marker_2=%#x, marker_3=%#x, marker_4=%#x", 
					marker_1, marker_2, marker_3, marker_4);
		
		/* Update offset */
		offset += 6;
	}
	if (other.es_rate_flag == 0x01) {
		uint32_t es_rate = (CONVERT_TO_24(payload[offset], payload[offset+1], payload[offset+2]) >> 1) & 0x3fffff;
		dprintf("ES_rate=%#x", es_rate);
		uint8_t marker_1 = payload[offset] >> 7;
		uint8_t marker_2 = payload[offset+2] & 0x01;
		if (!marker_1 || !marker_2)
			TS_WARNING("[3] ES_rate: marker_1=%#x, marker_2=%#x", marker_1, marker_2);

		/* Update offset */
		offset += 3;
	}
	if (other.dsm_trick_mode_flag == 0x01) {
		uint8_t trick_mode_control = (payload[offset] >> 5) & 0x07;
		if (trick_mode_control == TRICK_MODE_FAST_FORWARD) {
			uint8_t field_id = (payload[offset] >> 3) & 0x03;
			uint8_t intra_slice_refresh = (payload[offset] >> 2) & 0x01;
			uint8_t frequency_truncation = payload[offset] & 0x03;
			dprintf("trick_mode=fast_forward field_id=%#x, intra_slice_refresh=%#x, frequency_truncation=%#x",
					field_id, intra_slice_refresh, frequency_truncation);
		} else if (trick_mode_control == TRICK_MODE_SLOW_MOTION) {
			uint8_t rep_cntrl = payload[offset] & 0x1f;
			dprintf("trick_mode=slow_motion rep_cntrl=%#x", rep_cntrl);
		} else if (trick_mode_control == TRICK_MODE_FREEZE_FRAME) {
			uint8_t field_id = (payload[offset] >> 3) & 0x03;
			uint8_t reserved = payload[offset] & 0x07;
			dprintf("trick_mode=freeze_frame field_id=%#x, reserved=%#x", field_id, reserved);
		} else if (trick_mode_control == TRICK_MODE_FAST_REVERSE) {
			uint8_t field_id = (payload[offset] >> 3) & 0x03;
			uint8_t intra_slice_refresh = (payload[offset] >> 2) & 0x01;
			uint8_t frequency_truncation = payload[offset] & 0x03;
			dprintf("trick_mode=fast_reverse field_id=%#x, intra_slice_refresh=%#x, frequency_truncation=%#x",
					field_id, intra_slice_refresh, frequency_truncation);
		} else if (trick_mode_control == TRICK_MODE_SLOW_REVERSE) {
			uint8_t rep_cntrl = payload[offset] & 0x1f;
			dprintf("trick_mode=slow_reverse rep_cntrl=%#x", rep_cntrl);
		} else {
			dprintf("Using reserved bits in trick_mode_control");
		}
		/* Update offset */
		offset++;
	}
	if (other.additional_copy_info_flag == 0x01) {
		uint8_t marker = (payload[offset] >> 7) & 0x01;
		uint8_t additional_copy_info = payload[offset] & 0x7f;
		dprintf("additional_copy_info=%#x", additional_copy_info);
		if (!marker)
			dprintf("additional_copy_info: marker=%#x", marker);
		/* Update offset */
		offset++;
	}
	if (other.pes_crc_flag == 0x01) {
		uint16_t previous_pes_packet_crc = CONVERT_TO_16(payload[offset], payload[offset+1]);
		dprintf("previous_pes_packet_crc=%#x", previous_pes_packet_crc);
		/* Update offset */
		offset += 2;
	}
	if (other.pes_extension_flag == 0x01) {
		uint8_t pes_private_data_flag = (payload[offset] >> 7) & 0x01;
		uint8_t pack_header_field_flag = (payload[offset] >> 6) & 0x01;
		uint8_t program_packet_sequence_counter_flag = (payload[offset] >> 5) & 0x01;
		uint8_t p_std_buffer_flag = (payload[offset] >> 4) & 0x01;
		uint8_t reserved = (payload[offset] >> 3) & 0x7;
		uint8_t pes_extension_flag_2 = payload[offset] & 0x01;
		offset++;
		if (pes_private_data_flag == 0x01) {
			/* 128 bits of PES private data */
			offset += 16;
		}
		if (pack_header_field_flag == 0x01) {
			uint8_t pack_field_length = payload[offset++];
			/* Pack header */
			offset += pack_field_length;
		}
		uint8_t original_stuff_length = 0;
		if (program_packet_sequence_counter_flag == 0x01) {
			uint8_t marker_1 = (payload[offset] >> 7) & 0x01;
			uint8_t program_packet_sequence_counter = payload[offset] & 0x7f;
			uint8_t marker_2 = (payload[offset+1] >> 7) & 0x01;
			uint8_t mpeg1_mpeg2_identifier = (payload[offset+1] >> 6) & 0x01;
			original_stuff_length = (payload[offset+1] & 0x3f);
			offset += 2;
		}
		uint16_t p_std_buffer_size = 0;
		if (p_std_buffer_flag == 0x01) {
			uint8_t _0_1 = (payload[offset] >> 6);
			uint8_t p_std_buffer_scale = (payload[offset] >> 5) & 0x01;
			p_std_buffer_size = CONVERT_TO_16(payload[offset], payload[offset+1]) & 0x1fff;
			offset += 2;
		}
		if (pes_extension_flag_2 == 0x01) {
			uint8_t marker_1 = (payload[offset] >> 7) & 0x01;
			uint8_t pes_extension_field_length = payload[offset] & 0x7f;
			uint8_t stream_id_extension_flag = (payload[offset+1] >> 7) & 0x01;
			if (stream_id_extension_flag == 0x00) {
				uint8_t stream_id_extension = payload[offset+1] & 0x7f;
				for (uint8_t i=0; i<pes_extension_field_length; ++i) {
					/* reserved */
				}
				offset += pes_extension_field_length;
			}
			offset += 2;
		}
		for (uint8_t i=0; i<original_stuff_length; ++i) { 
			/* stuffing byte */ 
			offset++;
		}
		for (uint8_t i=0; i<p_std_buffer_size; ++i) { 
			/* PES_packet_data_byte */ 
			offset++;
		}
	}
	return offset;
}

struct pes_header {
	uint32_t packet_start_code_prefix:24;
	uint32_t stream_id:8;
	uint16_t pes_packet_length;
};

static int pes_parse_packet(const struct ts_header *header, const char *payload, 
		uint8_t payload_len, struct demuxfs_data *priv)
{
	struct pes_header pes;
	
	pes.packet_start_code_prefix = CONVERT_TO_24(payload[0],payload[1],payload[2]);
	pes.stream_id = payload[3];
	pes.pes_packet_length = CONVERT_TO_16(payload[4],payload[5]);

	if (pes.packet_start_code_prefix != PES_PACKET_START_CODE_PREFIX) {
		TS_WARNING("packet_start_code_prefix != %#x (%#x)", 
				PES_PACKET_START_CODE_PREFIX, pes.packet_start_code_prefix);
	}

	/* XXX: check header->payload_unit_start_indicator */

	int index = 0;
	int stream_id = pes_identify_stream_id(pes.stream_id);
	if (stream_id != PROGRAM_STREAM_MAP &&
		stream_id != PADDING_STREAM &&
		stream_id != PRIVATE_STREAM_2 &&
		stream_id != ECM_STREAM &&
		stream_id != EMM_STREAM &&
		stream_id != PROGRAM_STREAM_DIRECTORY &&
		stream_id != DSMCC_STREAM &&
		stream_id != H222_1_TYPE_E) {
		index = pes_parse_other(payload, payload_len, priv);
	} else if (stream_id == PROGRAM_STREAM_MAP ||
		stream_id != PADDING_STREAM ||
		stream_id != PRIVATE_STREAM_2 ||
		stream_id != ECM_STREAM ||
		stream_id != EMM_STREAM ||
		stream_id != PROGRAM_STREAM_DIRECTORY ||
		stream_id != DSMCC_STREAM ||
		stream_id != H222_1_TYPE_E) {
		for (uint16_t i=0; i<pes.pes_packet_length; ++i) {
			/* PES_packet_data_byte */
			index++;
		}
	} else if (stream_id == PADDING_STREAM) {
		for (uint16_t i=0; i<pes.pes_packet_length; ++i) {
			/* padding bytes */
			index++;
		}
	} else {
		dprintf("Unknown stream_id %#x", pes.stream_id);
	}
	dprintf(">>> payload_len=%d, pes.pes_packet_length=%d, index=%d", payload_len, pes.pes_packet_length, index);

	return 0;
}

static struct dentry *pes_get_dentry(const struct ts_header *header, struct demuxfs_data *priv)
{
	struct dentry *slink, *dentry;
	char pathname[PATH_MAX];

	/* TODO: create a dedicated hash table for quick access to the dentries */
	sprintf(pathname, "/%#x", header->pid);
	slink = fsutils_get_dentry(priv->root, pathname);
	if (! slink) {
		dprintf("couldn't get a dentry for '%s'", pathname);
		return NULL;
	}

	sprintf(pathname, "/%s/%s", slink->contents, FS_PES_FIFO_NAME);
	dentry = fsutils_get_dentry(priv->root, pathname);
	if (! dentry) {
		dprintf("couldn't get a dentry for '%s'", pathname);
		return NULL;
	}
	return dentry;
}

/* Packetized Elementary Stream parser */
int pes_parse(const struct ts_header *header, const char *payload, uint8_t payload_len,
		struct demuxfs_data *priv)
{
	struct dentry *dentry;
	int ret;

	if (payload_len < 6) {
		TS_WARNING("cannot parse PES header: contents is smaller than 6 bytes (%d)", payload_len);
		return -1;
	}

	if (priv->options.parse_pes) {
		if (header->payload_unit_start_indicator == 1) {
			if (payload[0] == 0x00 && payload[1] == 0x00 && payload[2] == 0x01)
				pes_parse_packet(header, payload, payload_len, priv);
			else
				dprintf("payload = { %#x, %#x, %#x }", payload[0], payload[1], payload[2]);
		}
	}

	dentry = pes_get_dentry(header, priv);
	if (! dentry) {
		dprintf("dentry = NULL");
		return -ENOENT;
	}

	pthread_mutex_lock(&dentry->mutex);
	ret = fifo_append(dentry->fifo, payload, payload_len);
	pthread_cond_broadcast(&dentry->condition);
	pthread_mutex_unlock(&dentry->mutex);

	return ret;
}
