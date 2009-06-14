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
#include "xattr.h"
#include "ts.h"

struct application_profile {
	uint16_t application_profile;
	uint8_t version_major;
	uint8_t version_minor;
	uint8_t version_micro;
};

struct formatted_descriptor {
	uint8_t application_profiles_length;
	struct application_profile *profiles;
	uint8_t service_bound_flag:1;
	uint8_t visibility:2;
	uint8_t reserved_future_use:5;
	uint8_t application_priority;
	char *transport_protocol_label;
	uint16_t _transport_protocol_label_count;
};

/* APPLICATION_DESCRIPTOR parser */
int dsmcc_descriptor_0x00_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	uint32_t count, i, n;
	struct formatted_descriptor f;
	struct dentry *subdir = CREATE_DIRECTORY(parent, "APPLICATION");

	memset(&f, 0, sizeof(f));

	f.application_profiles_length = payload[0];
	CREATE_FILE_NUMBER(subdir, &f, application_profiles_length);

	count = f.application_profiles_length / 5;
	if (count) {
		f.profiles = malloc(sizeof(struct application_profile) * count);
		if (! f.profiles)
			return -ENOMEM;
		for (i=0, n=0; n<count; ++n, i+=5) {
			char app_path[PATH_MAX];
			snprintf(app_path, sizeof(app_path), "APPLICATION_PROFILE_%02d", n+1);
			struct dentry *profile_dentry = CREATE_DIRECTORY(subdir, app_path);

			f.profiles[n].application_profile = CONVERT_TO_16(payload[i+1], payload[i+2]);
			f.profiles[n].version_major = payload[i+3];
			f.profiles[n].version_minor = payload[i+4];
			f.profiles[n].version_micro = payload[i+5];
			CREATE_FILE_NUMBER(profile_dentry, &f.profiles[n], application_profile);
			CREATE_FILE_NUMBER(profile_dentry, &f.profiles[n], version_major);
			CREATE_FILE_NUMBER(profile_dentry, &f.profiles[n], version_minor);
			CREATE_FILE_NUMBER(profile_dentry, &f.profiles[n], version_micro);
		}
	}
	i = f.application_profiles_length + 1;
	f.service_bound_flag = payload[i] >> 7;
	f.visibility = (payload[i] >> 5) & 0x03;
	f.reserved_future_use = payload[i] & 0x1f;
	f.application_priority = payload[i+1];
	CREATE_FILE_NUMBER(subdir, &f, service_bound_flag);
	CREATE_FILE_NUMBER(subdir, &f, visibility);
	CREATE_FILE_NUMBER(subdir, &f, application_priority);

	/* Allocate with an extra room for the null terminator */
	count = (len - i+1) + 1;
	f.transport_protocol_label = calloc(count, sizeof(char));
	for (n=0; n<count-1; ++n)
		f.transport_protocol_label[n] = payload[i+2+n];
	f._transport_protocol_label_count = count;
	CREATE_FILE_STRING(subdir, &f, transport_protocol_label, XATTR_FORMAT_STRING);

	return 0;
}

