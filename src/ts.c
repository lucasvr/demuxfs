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

int ts_parse_packet(const struct ts_header *header, const void *payload, struct demuxfs_data *priv)
{
	if (header->sync_byte != TS_SYNC_BYTE) {
		TS_WARNING("sync_byte=%#x", header->sync_byte);
		return -1;
	}
	
	void *payload_start = (void *) payload;

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
		uint8_t adaptation_field_length = ((char *) payload)[0];
		payload_start += adaptation_field_length + 1;
	}

	/* XXX: parse adaptation field */

	uint8_t pointer_field = 0;
	if (ts_is_psi_packet(header->pid, priv)) {
		if (header->payload_unit_start_indicator == 1) {
			/* The first byte of the payload carries the pointer_field */
			pointer_field = ((char *) payload_start)[0];
			payload_start += 1 + pointer_field;
		}
	}

	long diff = (long *) payload_start - (long *) payload;
	if (diff < 0 || diff > TS_PACKET_SIZE) {
		TS_WARNING("invalid pointer_field value '%d'", pointer_field);
		return -1;
	}
	uint8_t payload_len = TS_PACKET_SIZE - (uint8_t) diff;
	parse_function_t parse_function;
	
	if (header->pid == TS_PAT_PID)
		return pat_parse(header, payload_start, payload_len, priv);
	else if ((header->pid == TS_NULL_PID) && (header->payload_unit_start_indicator != 0))
		TS_WARNING("NULL packet has payload_unit_start_indicator != 0");
	else if ((parse_function = (parse_function_t) hashtable_get(priv->psi_parsers, header->pid)))
		return parse_function(header, payload_start, payload_len, priv);
    return 0;
}
