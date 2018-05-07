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
#include "byteops.h"
#include "xattr.h"
#include "ts.h"
#include "descriptors.h"

struct formatted_descriptor {
	uint32_t carousel_id;
	char *private_data;
};

/* CAROUSEL_ID_DESCRIPTOR parser */
int descriptor_0x13_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct dentry *dentry;
	struct formatted_descriptor f;

	if (! descriptor_is_parseable(parent, payload[0], 6, len))
		return -ENODATA;

	dentry = CREATE_DIRECTORY(parent, "Carousel_Identifier");
	f.carousel_id = CONVERT_TO_32(payload[2], payload[3], payload[4], payload[5]);
	CREATE_FILE_NUMBER(dentry, &f, carousel_id);

	/* 
	 * TODO: MHP contains a different spec for this descriptor. 
	 * See Table B.36 on page 505 of MHP Specification 1.1.3.
	 */

	if (len > 6) {
		size_t file_size = len-6;
		f.private_data = malloc(file_size * sizeof(char));
		memcpy(f.private_data, &payload[6], file_size);
		CREATE_FILE_BIN(dentry, &f, private_data, file_size);
		free(f.private_data);
	}

    return 0;
}

