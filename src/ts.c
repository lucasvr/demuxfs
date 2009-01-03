/* 
 * Copyright (c) 2008, Lucas C. Villa Real <lucasvr@gobolinux.org>
 * Copyright (c) 2008, Iuri Gomes Diniz <iuridiniz@gmail.com>
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
#include "buffer.h"
#include "byteops.h"
#include "hash.h"
#include "ts.h"

/* PSI tables */
#include "tables/psi.h"
#include "tables/pat.h"
#include "tables/pmt.h"
#include "tables/nit.h"
#include "tables/pes.h"
#include "tables/sdt.h"
#include "tables/sdtt.h"
#include "tables/tot.h"

void ts_dump_header(const struct ts_header *header)
{
	fprintf(stdout, "sync_byte=%#x\ntransport_error_indicator=%#x\n"
			"payload_unit_start_indicator=%#x\ntransport_priority=%#x\n"
			"pid=%#x\ntransport_scrambling_control=%#x\n"
			"adaptation_field=%#x\ncontinuity_counter=%#x\n",
			header->sync_byte, header->transport_error_indicator,
			header->payload_unit_start_indicator, header->transport_priority,
			header->pid, header->transport_scrambling_control,
			header->adaptation_field, header->continuity_counter);
}

static bool ts_is_psi_packet(uint16_t pid, struct demuxfs_data *priv)
{
	parse_function_t parse_function;

	switch (pid) {
		case TS_PAT_PID:
		case TS_CAT_PID:
		case TS_NIT_PID:
		case TS_SDT_PID: /* or TS_BAT_PID */
		case TS_EIT1_PID:
		case TS_EIT2_PID:
		case TS_EIT3_PID:
		case TS_RST_PID:
		case TS_TDT_PID:
		case TS_DCT_PID:
		case TS_DIT_PID:
		case TS_SIT_PID:
		case TS_PCAT_PID:
		case TS_SDTT1_PID:
		case TS_SDTT2_PID:
		case TS_BIT_PID:
		case TS_NBIT_PID: /* or TS_LDT_PID */
		case TS_CDT_PID:
		case TS_NULL_PID:
			return true;
		default:
			parse_function = (parse_function_t) hashtable_get(priv->psi_parsers, pid);
			return parse_function ? true : false;
	}
}

static bool ts_is_pes_packet(uint16_t pid, struct demuxfs_data *priv)
{
	parse_function_t parse_function;
	parse_function = (parse_function_t) hashtable_get(priv->pes_parsers, pid);
	return parse_function ? true : false;
}

static parse_function_t ts_get_psi_parser(const struct ts_header *header, uint8_t table_id,
		struct demuxfs_data *priv)
{
	parse_function_t parse_function;
	uint16_t pid = header->pid;


	if (table_id == TS_PAT_TABLE_ID)
		return pat_parse;
	else if (table_id == TS_PMT_TABLE_ID)
		return pmt_parse;
	else if (table_id == TS_NIT_TABLE_ID)
		return nit_parse;
	else if (table_id == TS_SDT_TABLE_ID)
		return sdt_parse;
	else if (table_id == TS_TOT_TABLE_ID)
		return tot_parse;
	else if (table_id == TS_SDTT_TABLE_ID)
		return sdtt_parse;
	//else if (table_id == TS_TDT_TABLE_ID)
	//	dprintf("TS carries a TDT table");
	//else if ((table_id >= TS_EIT_FIRST_TABLE_ID && table_id <= TS_EIT_LAST_TABLE_ID) || pid == TS_EIT1_PID)
	//	return eit_parse;
	else if ((parse_function = (parse_function_t) hashtable_get(priv->psi_parsers, pid)))
		return parse_function;
	else if ((pid == TS_NULL_PID) && (header->payload_unit_start_indicator != 0))
		TS_WARNING("NULL packet has payload_unit_start_indicator != 0");
    return NULL;
}

/**
 * ts_parse_packet - Parse a transport stream packet. Called by the backend's process() function.
 */
int ts_parse_packet(const struct ts_header *header, const char *payload, struct demuxfs_data *priv)
{
	int ret = 0;
	uint8_t pointer_field = 0;
	uint16_t section_length = 0;
	const char *payload_end;
	const char *payload_start = payload;
	parse_function_t parse_function;

	if (header->sync_byte != TS_SYNC_BYTE) {
		TS_WARNING("sync_byte != %#x (%#x)", TS_SYNC_BYTE, header->sync_byte);
		return -EBADMSG;
	}

	if (header->adaptation_field == 0x00) {
		/* ITU-T Rec. H.222.0 decoders shall discard this packet */
		return 0;
	} else if (header->adaptation_field == 0x01) {
		/* No adaptation field, payload only */
	} else if (header->adaptation_field == 0x02) {
		/* Adaptation field only, no payload */
		return 0;
	} else if (header->adaptation_field == 0x03) {
		/* Adaptation field followed by payload */
		uint8_t adaptation_field_length = payload[0];
		payload_start += 1 + adaptation_field_length;
		if ((payload_start - payload) > priv->options.packet_size) {
			TS_WARNING("adaptation_field length is bigger than a TS packet: %d", 
					adaptation_field_length);
			return -ENOBUFS;
		}
		/* TODO: parse adaptation field */
	}
	struct user_options *opt = &priv->options;
	payload_end = payload + opt->packet_size - 1 - opt->packet_error_correction_bytes;

	if (ts_is_psi_packet(header->pid, priv)) {
		const char *start = payload_start;
		const char *end = payload_end;
		bool remnant_flag = false;
		uint8_t table_id;

		if (header->payload_unit_start_indicator) {
			/* The first byte of the payload carries the pointer_field */
			pointer_field = payload_start[0];
			start = payload_start + 1;
			if ((payload_start + pointer_field) > payload_end) {
				TS_WARNING("pointer_field > TS packet size (%d)", pointer_field);
				return -ENOBUFS;
			}
			section_length = CONVERT_TO_16(start[1], start[2]) & 0x0fff;
			if (pointer_field > 0) {
				end = payload_start + pointer_field;
				remnant_flag = true;
			} else {
				end = ((payload_start + 3 + section_length) <= payload_end) ?
						payload_start + 3 + section_length : 
						payload_end;
			}
		} else {
			section_length = CONVERT_TO_16(start[1], start[2]) & 0x0fff;
			remnant_flag = true;
		}

		while (true) {
			struct buffer *buffer = hashtable_get(priv->packet_buffer, header->pid);
			if (! buffer && ! remnant_flag) {
				buffer = buffer_create(section_length + 3, false);
				hashtable_add(priv->packet_buffer, header->pid, buffer);
			}

			buffer_append(buffer, start, end - start + 1);
			if (buffer_contains_full_psi_section(buffer) || pointer_field > 0) {
				pointer_field = 0;
				table_id = start[0];
				if ((parse_function = ts_get_psi_parser(header, table_id, priv)))
					/* Invoke the PSI parser for this packet */
					ret = parse_function(header, buffer->data, buffer->current_size, priv);
				buffer_reset_size(buffer);
			}
			
			if (! header->payload_unit_start_indicator || ((end + 1) > payload_end))
				break;

			start = end + 1;
			if (((uint8_t) start[0]) == 0xff)
				break;

			section_length = CONVERT_TO_16(start[1], start[2]) & 0x0fff;
			end = ((start + section_length + 2) <= payload_end) ? 
					start + section_length + 2 :
					payload_end;
			remnant_flag = false;
		}
	} else if (ts_is_pes_packet(header->pid, priv)) {
		uint16_t size;
		struct buffer *buffer = hashtable_get(priv->packet_buffer, header->pid);
		if (! buffer) {
			if (! header->payload_unit_start_indicator || (payload_end - payload_start <= 6))
				return 0;
			size = CONVERT_TO_16(payload_start[4], payload_start[5]);
			buffer = buffer_create(size, true);
			if (! buffer)
				return 0;
			hashtable_add(priv->packet_buffer, header->pid, buffer);
		} else if (header->payload_unit_start_indicator && buffer_unbounded(buffer)) {
			/* 
			 * This is a new payload unit, and the previous unit had unbounded size.
			 * We need to reset the unbounded flag and let the next call to 
			 * buffer_contains_full_pes_section() below detect if this new 
			 * payload is also unbounded or not.
			 */
			buffer_reset_full(buffer);
		}

		buffer_append(buffer, payload_start, payload_end - payload_start + 1);
		if (buffer_contains_full_pes_section(buffer) || 
			(buffer_unbounded(buffer) && ! header->payload_unit_start_indicator)) {
			/* Invoke the PES parser for this packet */
			if ((parse_function = (parse_function_t) hashtable_get(priv->pes_parsers, header->pid)))
				ret = parse_function(header, buffer->data, buffer->current_size, priv);
			buffer_reset_size(buffer);
		}
	}
	return ret;
}
