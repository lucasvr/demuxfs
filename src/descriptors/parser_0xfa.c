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

struct formatted_descriptor {
	uint16_t area_code;
	uint8_t _guard_interval;
	uint8_t _transmission_mode;
	char guard_interval[16];
	char transmission_mode[32];
	uint8_t _frequency;
	char frequency[256];
};

/* TERRESTRIAL_DELIVERY_SYSTEM_DESCRIPTOR parser */
int descriptor_0xfa_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	int i;
	struct dentry *dentry;
	struct formatted_descriptor f;
	const char *guard_interval[] = { "1/32", "1/16", "1/8", "1/4" };

	f.area_code = ((payload[0] << 8) | payload[1]) >> 4;
	f._guard_interval = (payload[1] >> 2) & 0x03;
	sprintf(f.guard_interval, "%s [%#x]", guard_interval[f._guard_interval], f._guard_interval);
	
	f._transmission_mode = payload[1] & 0x03;
	if (f._transmission_mode == 0 || f._transmission_mode == 1 || f._transmission_mode == 2)
		sprintf(f.transmission_mode, "Mode %d [%#x]", f._transmission_mode+1, f._transmission_mode);
	else
		sprintf(f.transmission_mode, "Undefined [%#x]", f._transmission_mode);
	
	dentry = CREATE_DIRECTORY(parent, "TERRESTRIAL_DELIVERY_SYSTEM");
	CREATE_FILE_NUMBER(dentry, &f, area_code);
	CREATE_FILE_STRING(dentry, &f, guard_interval, XATTR_FORMAT_STRING_AND_NUMBER);
	CREATE_FILE_STRING(dentry, &f, transmission_mode, XATTR_FORMAT_STRING_AND_NUMBER);
	
	memset(f.frequency, 0, sizeof(f.frequency));
	for (i=0; i<len-2; i+=2) {
		char buf[16];
		f._frequency = (payload[2+i] << 8) | payload[2+i+1];
		sprintf(buf, "%s%#x", i == 0 ? "" : "\n", f._frequency);
		strcat(f.frequency, buf);
	}
	CREATE_FILE_STRING(dentry, &f, frequency, XATTR_FORMAT_NUMBER_ARRAY);

    return 0;
}

