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
#include "debug.h"

struct formatted_descriptor {
	uint8_t descriptor_number:4;
	uint8_t last_descriptor_number:4;
	char language_code[4];
	uint8_t length_items;
	uint8_t item_description_length;
	char *item_description;
	uint8_t item_length;
	char *item;
	uint8_t text_length;
	char *text;
};

/* EXTENDED_EVENT_DESCRIPTOR parser */
int descriptor_0x4e_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct formatted_descriptor f;
	struct dentry *dentry, *subdir;
	int i;
	
	if (! descriptor_is_parseable(parent, payload[0], 4, len))
		return -ENODATA;

	memset(&f, 0, sizeof(f));
	dentry = CREATE_DIRECTORY(parent, "Extended_Event_Descriptor");

	/* Descriptor and Last Descriptor numbers */
	f.descriptor_number = (payload[2] >> 4) & 0x0f;
	f.last_descriptor_number = payload[2] & 0x0f;

	subdir = CREATE_DIRECTORY(dentry, "Descriptor_%02d", f.descriptor_number);
	CREATE_FILE_NUMBER(subdir, &f, descriptor_number);
	CREATE_FILE_NUMBER(subdir, &f, last_descriptor_number);
	i = 3;

	/* Language code */
	memcpy(f.language_code, &payload[i], 3);
	CREATE_FILE_STRING(subdir, &f, language_code, XATTR_FORMAT_STRING);
	len -= 3, i += 3;

	/* Length items */
	f.length_items = payload[i];
	CREATE_FILE_NUMBER(subdir, &f, length_items);
	len--, i++;

	/* Item description */
	f.item_description_length = payload[i];
	CREATE_FILE_NUMBER(subdir, &f, item_description_length);
	len--, i++;

	if (f.item_description_length) {
		f.item_description = strndup(&payload[i], f.item_description_length);
		CREATE_FILE_STRING(subdir, &f, item_description, XATTR_FORMAT_STRING);
		len -= f.item_description_length, i += f.item_description_length;
		free(f.item_description);
	}
	
	/* Item */
	f.item_length = payload[i];
	CREATE_FILE_NUMBER(subdir, &f, item_length);
	len--, i++;

	if (len > 0 && f.item_length) {
		f.item = strndup(&payload[i], f.item_length);
		CREATE_FILE_STRING(subdir, &f, item, XATTR_FORMAT_STRING);
		len -= f.item_length, i += f.item_length;
		free(f.item);
	}

	/* Text */
	f.text_length = payload[i];
	CREATE_FILE_NUMBER(subdir, &f, text_length);
	len--, i++;

	if (len > 0 && f.text_length) {
		f.text = strndup(&payload[i], f.text_length);
		CREATE_FILE_STRING(subdir, &f, text, XATTR_FORMAT_STRING);
		len -= f.text_length, i += f.text_length;
		free(f.text);
	}

    return 0;
}

