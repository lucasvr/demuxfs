/* 
 * Copyright (c) 2008-2018, Lucas C. Villa Real <lucasvr@gobolinux.org>
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
#include "services.h"
#include "descriptors.h"

struct formatted_descriptor {
	uint8_t _service_type;
	char service_type[64];
	uint8_t service_provider_name_length;
	char service_provider_name[256];
	uint8_t service_name_length;
	char service_name[256];
};

/* SERVICE_DESCRIPTOR parser */
int descriptor_0x48_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct formatted_descriptor f;
	struct dentry *dentry;
	int i, j;

	if (! descriptor_is_parseable(parent, payload[0], 4, len))
		return -ENODATA;

	memset(&f, 0, sizeof(f));
	dentry = CREATE_DIRECTORY(parent, "Service_Descriptor");

	f._service_type = payload[2];
	sprintf(f.service_type, "%s [%#x]", service_type_to_string(f._service_type), f._service_type);
	CREATE_FILE_STRING(dentry, &f, service_type, XATTR_FORMAT_STRING_AND_NUMBER);

	f.service_provider_name_length = payload[3];
	for (i=0; i<f.service_provider_name_length; ++i)
		f.service_provider_name[i] = payload[4+i];
	CREATE_FILE_NUMBER(dentry, &f, service_provider_name_length);
	CREATE_FILE_STRING(dentry, &f, service_provider_name, XATTR_FORMAT_STRING);

	f.service_name_length = payload[4+i];
	for (j=0; j<f.service_name_length; ++j)
		f.service_name[j] = payload[5+i+j];
	CREATE_FILE_NUMBER(dentry, &f, service_name_length);
	CREATE_FILE_STRING(dentry, &f, service_name, XATTR_FORMAT_STRING);

    return 0;
}

