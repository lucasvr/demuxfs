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

struct parental_rating_descriptor {
	char country_code[16];
	uint8_t rating:8;
};

/* PARENTAL_RATING_DESCRIPTOR parser */
int descriptor_0x55_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	int i, n, num_entries = (len-2) / 4;

	for (i=2, n=0; n<num_entries; ++n) {
		struct dentry *subdir;
		char dirname[32];
		sprintf(dirname, "PARENTAL_RATING_%d", n+1);
		subdir = CREATE_DIRECTORY(parent, dirname);

		struct parental_rating_descriptor p;
		memset(&p, 0, sizeof(p));
		sprintf(p.country_code, "%c%c%c [0x%x%x%x]", 
				payload[i], payload[i+1], payload[i+2],
				payload[i], payload[i+1], payload[i+2]);
		p.rating = payload[i+3];
		CREATE_FILE_STRING(subdir, &p, country_code, XATTR_FORMAT_STRING_AND_NUMBER);
		CREATE_FILE_NUMBER(subdir, &p, rating);
		i += 4;
	}
    return 0;
}

