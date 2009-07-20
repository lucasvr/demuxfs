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
#include "byteops.h"
#include "descriptors.h"

struct formatted_descriptor {
	uint16_t data_component_id;
	uint8_t dmf:4;
	uint8_t reserved:2;
	uint8_t timing:2;
};

/* DATA_COMPONENT_DESCRIPTOR parser */
int descriptor_0xfd_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct dentry *dentry;
	struct formatted_descriptor f;

	if (! descriptor_is_parseable(parent, payload[0], 5, len))
		return -ENODATA;

	dentry = CREATE_DIRECTORY(parent, "DATA_COMPONENT");
	f.data_component_id = CONVERT_TO_16(payload[2], payload[3]);
	f.dmf = payload[4] >> 4;
	f.reserved = (payload[4] >> 2) & 0x03;
	f.timing = payload[4] & 0x03;
	CREATE_FILE_NUMBER(dentry, &f, data_component_id);
	CREATE_FILE_NUMBER(dentry, &f, dmf);
	CREATE_FILE_NUMBER(dentry, &f, timing);
	if (f.data_component_id != 0x0008)
		TS_WARNING("data_component_id == %#x, expected 0x0008", f.data_component_id);
	if (f.dmf != 0x03)
		TS_WARNING("DMF == %#x, expected 0x03", f.dmf);
	if (f.timing != 0x01)
		TS_WARNING("timing == %#x, expected 0x01", f.timing);

    return 0;
}

