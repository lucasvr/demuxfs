/* 
 * Copyright (c) 2010, Rogerio P. Nunes <rogerio@organia.com.br>
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
	uint8_t reference_level;
	char free_text[256];
};

/* FS_METADATA_DESCRIPTOR parser */
int descriptor_0xa0_parser(const char *payload, int len, struct dentry *parent, 
		struct demuxfs_data *priv)
{
	struct dentry *dentry;
	struct formatted_descriptor f;
	int i;
	
	if (! descriptor_is_parseable(parent, payload[0], 2, len))
		return -ENODATA;

	dentry = CREATE_DIRECTORY(parent, "FS_Descriptor");

	f.reference_level = payload[2];

	for (i=0; i<len && i<255; i++)
		f.free_text[i] = payload[i+3];
	f.free_text[i] = '\0';

	CREATE_FILE_NUMBER(dentry, &f, reference_level);
	CREATE_FILE_STRING(dentry, &f, free_text, XATTR_FORMAT_STRING);

    return 0;
}

