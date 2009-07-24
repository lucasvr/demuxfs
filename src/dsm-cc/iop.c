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
#include "byteops.h"
#include "fsutils.h"
#include "ts.h"
#include "iop.h"
#include "biop.h"
#include "xattr.h"
#include "tables/psi.h"

int iop_create_ior_dentries(struct dentry *parent, struct iop_ior *ior)
{
	CREATE_FILE_NUMBER(parent, ior, type_id_length);
	CREATE_FILE_STRING(parent, ior, type_id, XATTR_FORMAT_STRING);
	if (ior->type_id_length % 4)
		CREATE_FILE_BIN(parent, ior, alignment_gap, ior->type_id_length % 4);

	CREATE_FILE_NUMBER(parent, ior, tagged_profiles_count);
	if (ior->tagged_profiles_count)
		biop_create_tagged_profiles_dentries(parent, ior->tagged_profiles);

	return 0;
}

void iop_free_ior(struct iop_ior *ior)
{
	if (ior->type_id) {
		free(ior->type_id);
		ior->type_id = NULL;
	}
	if (ior->tagged_profiles) {
		free(ior->tagged_profiles);
		ior->tagged_profiles = NULL;
	}
	/* Do not deallocate ior */
}

int iop_parse_ior(struct iop_ior *ior, const char *payload, uint32_t len)
{
	int j = 0;

	ior->type_id_length = CONVERT_TO_32(payload[j], payload[j+1], payload[j+2], payload[j+3]);
	if (ior->type_id_length != 4) {
		dprintf("Error: ior->type_id_length != 4 (%#x)", ior->type_id_length);
		return -1;
	}

	ior->type_id = calloc(ior->type_id_length+1, sizeof(char));
	memcpy(ior->type_id, &payload[j+4], ior->type_id_length);
	j += 4 + ior->type_id_length;
	
	uint8_t gap_bytes = ior->type_id_length % 4;
	if (gap_bytes) {
		memcpy(ior->alignment_gap, &payload[j], 4 - gap_bytes);
		j += 4 - gap_bytes;
	}

	ior->tagged_profiles_count = CONVERT_TO_32(payload[j], payload[j+1], payload[j+2], payload[j+3]);
	j += 4;
	if (ior->tagged_profiles_count) {
		/* Parse tagged profiles */
		ior->tagged_profiles = calloc(ior->tagged_profiles_count, sizeof(struct biop_tagged_profile));
		j += biop_parse_tagged_profiles(ior->tagged_profiles, ior->tagged_profiles_count, 
				&payload[j], len-j);
	}

	return j;
}
