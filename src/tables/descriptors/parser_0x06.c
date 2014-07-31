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
#include "fsutils.h"
#include "xattr.h"
#include "ts.h"
#include "descriptors.h"

// http://www.etherguidesystems.com/help/sdos/mpeg/semantics/mpeg-2/alignment_type.aspx

struct data_stream_alignment_descriptor {
    uint8_t alignment_type;
};

/* DATA_STREAM_ALIGNMENT_DESCRIPTOR parser */
int descriptor_0x06_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	if (! descriptor_is_parseable(parent, payload[0], 3, len))
		return -ENODATA;

    struct data_stream_alignment_descriptor dsa;
    dsa.alignment_type = payload[2];
  
    TS_WARNING("0x06_parser: DATA_STREAM_ALIGNMENT_DESCRIPTOR parser is not fully implemented");
    struct dentry *subdir = CREATE_DIRECTORY(parent, "Data_Stream_Alignment_Descriptor");
    CREATE_FILE_NUMBER(subdir, &dsa, alignment_type);
    return 0;
}

