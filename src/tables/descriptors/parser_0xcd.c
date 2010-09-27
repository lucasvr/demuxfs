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
#include "byteops.h"
#include "fsutils.h"
#include "xattr.h"
#include "ts.h"
#include "tables/psi.h"
#include "tables/pat.h"
#include "descriptors.h"

struct transmission_type_data {
	uint8_t transmission_type_info;
	uint8_t num_of_service;
	uint16_t service_id;
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

	if (! descriptor_is_parseable(parent, payload[0], 4, len))
		return -ENODATA;
	
	f.remote_control_key_id = payload[2];
	f.length_of_ts_name = payload[3] >> 2;
	f.transmission_type_count = payload[3] & 0x03;
	
	for (i=0; i<f.length_of_ts_name; ++i)
		f.ts_name[i] = payload[4+i];
	f.ts_name[i] = '\0';
	
	struct dentry *dentry = CREATE_DIRECTORY(parent, "Transmission_Information_Descriptor");
	CREATE_FILE_NUMBER(dentry, &f, remote_control_key_id);
	CREATE_FILE_NUMBER(dentry, &f, length_of_ts_name);
	CREATE_FILE_NUMBER(dentry, &f, transmission_type_count);
	CREATE_FILE_STRING(dentry, &f, ts_name, XATTR_FORMAT_STRING);

	offset = 4 + f.length_of_ts_name;
	for (i=0; i<f.transmission_type_count; ++i) {
		struct transmission_type_data t;
		struct dentry *subdir, *service;
		
		subdir = CREATE_DIRECTORY(dentry, "Transmission_%02d", i+1);

		t.transmission_type_info = payload[offset];
		t.num_of_service = payload[offset+1];
		offset += 2;
		CREATE_FILE_NUMBER(subdir, &t, transmission_type_info);
		CREATE_FILE_NUMBER(subdir, &t, num_of_service);

		for (j=0; j<t.num_of_service; j++) {
			t.service_id = CONVERT_TO_16(payload[offset], payload[offset+1]);
			offset += 2;

			service = CREATE_DIRECTORY(subdir, "Service_%02d", (j/2)+1);
			CREATE_FILE_NUMBER(service, &t, service_id);

			if (! pat_announces_service(t.service_id, priv))
				TS_WARNING("service_id %#x not declared by the PAT", t.service_id);
		}
	}

    return 0;
}

