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
#include "descriptors.h"

struct formatted_descriptor {
	char language_code[4];
	uint8_t event_name_length;
	char *event_name;
	uint8_t text_length;
	char *text;
};

/* SHORT_EVENT_DESCRIPTOR parser */
int descriptor_0x4d_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct formatted_descriptor f;
	struct dentry *dentry;
	int i;
	
	if (! descriptor_is_parseable(parent, payload[0], 4, len))
		return -ENODATA;

	memset(&f, 0, sizeof(f));
	dentry = CREATE_DIRECTORY(parent, "Short_Event_Descriptor");

	memcpy(f.language_code, &payload[2], 3);
	CREATE_FILE_STRING(dentry, &f, language_code, XATTR_FORMAT_STRING);
	len -= 3;

	f.event_name_length = payload[5];
	CREATE_FILE_NUMBER(dentry, &f, event_name_length);
	len--;
	i = 6;

	if (len > 0 && f.event_name_length) {
		f.event_name = strndup(&payload[6], f.event_name_length);
		CREATE_FILE_STRING(dentry, &f, event_name, XATTR_FORMAT_STRING);
		len -= f.event_name_length;
		i += f.event_name_length;
		free(f.event_name);
	}

	f.text_length = payload[i];
	CREATE_FILE_NUMBER(dentry, &f, text_length);
	len--;
	i++;

	if (len > 0 && f.text_length) {
		f.text = strndup(&payload[i], f.text_length);
		CREATE_FILE_STRING(dentry, &f, text, XATTR_FORMAT_STRING);
		len -= f.text_length;
		i += f.text_length;
		free(f.text);
	}

    return 0;
}

