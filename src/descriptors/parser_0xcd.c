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

struct transmission_type_data {
	uint8_t transmission_type_info;
	uint8_t num_of_service;
	char service_id[256];
};

struct formatted_descriptor {
	uint8_t remote_control_key_id;
	uint8_t length_of_ts_name;
	uint8_t transmission_type_count;
	char ts_name[256];
};

/* TS_INFORMATION_DESCRIPTOR parser */
int descriptor_0xcd_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct formatted_descriptor f;
	uint8_t offset, i, j;

	if (len < 2) {
		TS_WARNING("cannot parse descriptor %#x: contents smaller than 2 bytes (%d)", 0xfe, len);
		return -1;
	}
	
	f.remote_control_key_id = payload[0];
	f.length_of_ts_name = payload[1] >> 2;
	f.transmission_type_count = payload[1] & 0x03;
	
	for (i=0; i<f.length_of_ts_name; ++i)
		f.ts_name[i] = payload[2+i];
	f.ts_name[i] = '\0';
	
	struct dentry *dentry = CREATE_DIRECTORY(parent, "TRANSMISSION_INFORMATION");
	CREATE_FILE_NUMBER(dentry, &f, remote_control_key_id);
	CREATE_FILE_NUMBER(dentry, &f, length_of_ts_name);
	CREATE_FILE_NUMBER(dentry, &f, transmission_type_count);
	CREATE_FILE_STRING(dentry, &f, ts_name, XATTR_FORMAT_STRING);

	offset = 2 + f.length_of_ts_name;
	for (i=0; i<f.transmission_type_count; ++i) {
		struct transmission_type_data t;
		char transmission_name[32];
		struct dentry *subdir;
		
		sprintf(transmission_name, "TRANSMISSION_%02d", i);
		subdir = CREATE_DIRECTORY(dentry, transmission_name);

		t.transmission_type_info = payload[offset];
		t.num_of_service = payload[offset+1];
		CREATE_FILE_NUMBER(subdir, &t, transmission_type_info);
		CREATE_FILE_NUMBER(subdir, &t, num_of_service);

		memset(t.service_id, 0, sizeof(t.service_id));
		for (j=0; j<t.num_of_service; ++j) {
			char buf[32];
			uint16_t service_id;

			service_id = (payload[offset+j+2] << 8) | payload[offset+j+3];
			sprintf(buf, "%s%#x", j == 0 ? "" : "\n", service_id);
			strcat(t.service_id, buf);
		}
		CREATE_FILE_STRING(subdir, &t, service_id, XATTR_FORMAT_NUMBER_ARRAY);
		offset += 2 + j + (t.num_of_service ? 2 : 0);
	}

    return 0;
}

