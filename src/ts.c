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
#include "crc32.h"

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
	fprintf(stdout, "--- ts header ---\n");
	fprintf(stdout, "sync_byte=%#x\ntransport_error_indicator=%#x\n"
			"payload_unit_start_indicator=%#x\ntransport_priority=%#x\n"
			"pid=%#x\ntransport_scrambling_control=%#x\n"
			"adaptation_field=%#x\ncontinuity_counter=%#x\n",
			header->sync_byte, header->transport_error_indicator,
			header->payload_unit_start_indicator, header->transport_priority,
			header->pid, header->transport_scrambling_control,
			header->adaptation_field, header->continuity_counter);
}

void ts_dump_payload(const char *payload, int len)
{
	int size = CONVERT_TO_16(payload[1], payload[2]) & 0x0fff;
	fprintf(stdout, "--- ts payload ---\n");
	fprintf(stdout, "table_id=%#x\nsize=%#x (%d)\n", payload[0], size, size);
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
	uint16_t pid = header->pid;
	parse_function_t parse_function;
	parse_function = (parse_function_t) hashtable_get(priv->psi_parsers, pid);

	if (parse_function)
		return parse_function;
	else if (table_id == TS_PAT_TABLE_ID && header->pid == TS_PAT_PID)
		return pat_parse;
	else if (table_id == TS_PMT_TABLE_ID)
		return pmt_parse;
	else if (table_id == TS_NIT_TABLE_ID && header->pid == TS_NIT_PID)
		return nit_parse;
	else if (table_id == TS_SDT_TABLE_ID && header->pid == TS_SDT_PID)
		return sdt_parse;
	else if (table_id == TS_TOT_TABLE_ID)
		return tot_parse;
	else if (table_id == TS_SDTT_TABLE_ID && 
		(header->pid == TS_SDTT1_PID || header->pid == TS_SDTT2_PID))
		return sdtt_parse;
	else if (table_id == TS_CDT_TABLE_ID && header->pid == TS_CDT_PID)
		dprintf("TS carries a CDT table");
	//else if (table_id == TS_TDT_TABLE_ID)
	//	dprintf("TS carries a TDT table");
	//else if ((table_id >= TS_EIT_FIRST_TABLE_ID && table_id <= TS_EIT_LAST_TABLE_ID) || pid == TS_EIT1_PID)
	//	return eit_parse;
	else if ((pid == TS_NULL_PID) && (header->payload_unit_start_indicator != 0))
		TS_WARNING("NULL packet has payload_unit_start_indicator != 0");

    return NULL;
}

static bool continuity_counter_is_ok(const struct ts_header *header, struct buffer *buffer, bool psi)
{
	uint8_t last_cc = buffer->continuity_counter;
	uint8_t this_cc = header->continuity_counter;
	bool buf_empty = buffer_get_current_size(buffer) == 0;

	if (last_cc == this_cc) {
		/* 
		 * Repeating last counter. The standard allows for the transmission of up to 2 sequential 
		 * packets with the same continuity counter.
		 */
		return false;
	} else if (! buf_empty) {
		if ((last_cc == 15 && this_cc != 0) || (this_cc && (this_cc - last_cc) != 1)) {
			TS_WARNING("%s continuity error on pid=%d: last counter=%d, current counter=%d",
					psi ? "PSI" : "PES", header->pid, last_cc, this_cc);
			buffer_reset_size(buffer);
			return false;
		}
	}
	return true;
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
	payload_end = payload + opt->packet_size - 4 - 1 - opt->packet_error_correction_bytes;
	
	//ts_dump_header(header);
	//ts_dump_payload(payload, payload_end-payload_start);

	struct buffer *buffer = NULL;

	if (ts_is_psi_packet(header->pid, priv)) {
		const char *start = payload_start;
		const char *end = payload_end;
		bool is_new_packet = false;
		bool pusi = header->payload_unit_start_indicator;
		uint8_t table_id;

		if (pusi) {
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
			} else {
				is_new_packet = true;
				end = ((payload_start + 3 + section_length) <= payload_end) ?
						payload_start + 3 + section_length : 
						payload_end;
			}
		}

		while (start <= payload_end) {
			buffer = hashtable_get(priv->packet_buffer, header->pid);
			if (! buffer && is_new_packet) {
				buffer = buffer_create(header->pid, section_length + 3, false);
				if (! buffer)
					return 0;
				buffer->continuity_counter = header->continuity_counter;
				hashtable_add(priv->packet_buffer, header->pid, buffer);
			} else if (buffer && ! continuity_counter_is_ok(header, buffer, true)) {
				return 0;
			} else if (buffer && buffer->current_size == 0 && ! is_new_packet) {
				/*
				 * Cannot start appending data if we don't have PUSI set and there are
				 * no contents in the buffer yet.
				 */
				if (! pusi)
					return 0;

				/*
				 * The second half of the packet can very well have valid data.
				 * Just invalidate the buffer to return to the beginning of the loop.
				 */
				buffer = NULL;
			}

			if (is_new_packet && IS_STUFFING_PACKET(start))
				buffer = NULL;

			if (buffer) {
				int ret = buffer_append(buffer, start, end - start + 1);
				if (buffer_contains_full_psi_section(buffer)) {
					table_id = buffer->data[0];
					if (! crc32_check(buffer->data, buffer->current_size))
						TS_WARNING("CRC error on PID %d(%#x), table_id %d(%#x)", 
							header->pid, header->pid, table_id, table_id);
					else if ((parse_function = ts_get_psi_parser(header, table_id, priv)))
						/* Invoke the PSI parser for this packet */
						ret = parse_function(header, buffer->data, buffer->current_size, priv);
					buffer_reset_size(buffer);
				}
			}
			
			if (! pusi || pointer_field == 0)
				break;

			if (buffer)
				buffer_reset_size(buffer);

			start = end + 1;
			section_length = CONVERT_TO_16(start[1], start[2]) & 0x0fff;
			if (IS_STUFFING_PACKET(start) || section_length == 0)
				/* Nothing to parse */
				break;

			end = ((start + section_length + 2) <= payload_end) ? 
					start + section_length + 2 :
					payload_end;

			pusi = false;
			is_new_packet = true;
		}
	} else if (ts_is_pes_packet(header->pid, priv)) {
		uint16_t size;
		bool pusi = header->payload_unit_start_indicator;

		buffer = hashtable_get(priv->packet_buffer, header->pid);
		if (! buffer) {
			if (! pusi || (payload_end - payload_start <= 6))
				return 0;
			size = CONVERT_TO_16(payload_start[4], payload_start[5]);
			buffer = buffer_create(header->pid, size, true);
			if (! buffer)
				return 0;
			buffer->continuity_counter = header->continuity_counter;
			hashtable_add(priv->packet_buffer, header->pid, buffer);
		} else if (!continuity_counter_is_ok(header, buffer, false) ||
			(buffer_get_current_size(buffer) == 0 && !buffer_is_unbounded(buffer) && 
			 (!pusi || (payload_end - payload_start <= 6)))) {
			return 0;
		}

		buffer_append(buffer, payload_start, payload_end - payload_start + 1);
		if (buffer_contains_full_pes_section(buffer)) {
			/* Invoke the PES parser for this packet */
			if ((parse_function = (parse_function_t) hashtable_get(priv->pes_parsers, header->pid)))
				ret = parse_function(header, buffer->data, buffer->current_size, priv);
			buffer_reset_size(buffer);
		}
	} else {
		int i, found=0;
		static int idx = 0; 
		static int pids[100];
		for (i=0; i<idx; ++i) {
			if (pids[i] == header->pid) {
				found=1;
				break;
			}
		}
		if (! found) {
			TS_WARNING("Unknown packet with PID %#x", header->pid);
			if (idx < 100)
				pids[idx++] = header->pid;
		}
	}
	if (buffer)
		buffer->continuity_counter = header->continuity_counter;
	return ret;
}
