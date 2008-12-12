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
#include "ts.h"
#include "tables/psi.h"

void psi_populate(void **table, struct dentry *parent)
{
	struct psi_common_header *header = *(struct psi_common_header **) table;
	CREATE_FILE_NUMBER(parent, header, table_id);
	CREATE_FILE_NUMBER(parent, header, section_syntax_indicator);
	//CREATE_FILE_NUMBER(parent, header, reserved_1);
	//CREATE_FILE_NUMBER(parent, header, reserved_2);
	CREATE_FILE_NUMBER(parent, header, section_length);
	CREATE_FILE_NUMBER(parent, header, identifier);
	//CREATE_FILE_NUMBER(parent, header, reserved_3);
	CREATE_FILE_NUMBER(parent, header, version_number);
	CREATE_FILE_NUMBER(parent, header, current_next_indicator);
	CREATE_FILE_NUMBER(parent, header, section_number);
	CREATE_FILE_NUMBER(parent, header, last_section_number);
}

void psi_dump_header(struct psi_common_header *header)
{
	dprintf("table_id=%#x\nsection_syntax_indicator=%d\nsection_length=%d\n"
			"identifier=%d\nversion_number=%d\ncurrent_next_indicator=%d\nsection_number=%d\n"
			"last_section_number=%d\n__inode=%#llx\n__name=%s",
			header->table_id, header->section_syntax_indicator, header->section_length,
			header->identifier, header->version_number, header->current_next_indicator, 
			header->section_number,	header->last_section_number, 
			header->dentry->inode, header->dentry->name);
}

static bool psi_check_header(struct psi_common_header *header)
{
	bool ret = true;

	if (header->section_syntax_indicator != 1) {
		TS_WARNING("section_syntax_indicator != 1");
		ret = false;
	}
	if (header->section_length > TS_MAX_SECTION_LENGTH) {
		TS_WARNING("section_length is greater than %#x bytes", TS_MAX_SECTION_LENGTH);
		ret = false;
	}
	if (header->table_id > TS_LAST_TABLE_ID) {
		TS_WARNING("table_id is greater than %#x", TS_LAST_TABLE_ID);
		ret = false;
	}
	if (header->_remaining_packets > 0)
		dprintf("_remaining_packets = %d", header->_remaining_packets);
	return ret;
}

int psi_parse(struct psi_common_header *header, const char * payload, uint32_t payload_len)
{
	if (payload_len < 8) {
		TS_WARNING("cannot parse PSI header: contents is smaller than 8 bytes (%d)", payload_len);
		return -1;
	}
	header->table_id                 = payload[0];
	header->section_syntax_indicator = (payload[1] >> 7) & 0x01;
	header->reserved_1               = (payload[1] >> 6) & 0x01;
	header->reserved_2               = (payload[1] >> 4) & 0x03;
	header->section_length           = ((payload[1] << 8) | payload[2]) & 0x0fff;
	header->identifier               = (payload[3] << 8) | payload[4];
	header->reserved_3               = (payload[5] >> 6) & 0x03;
	header->version_number           = (payload[5] >> 1) & 0x1f;
	header->current_next_indicator   = payload[5] & 0x01;
	header->section_number           = payload[6];
	header->last_section_number      = payload[7];
	header->_remaining_packets       = (header->section_length + 3) / 188 + 
									   ((header->section_length + 3) % 188 ? 1 : 0) - 1;
	psi_check_header(header);

	return 0;
}

