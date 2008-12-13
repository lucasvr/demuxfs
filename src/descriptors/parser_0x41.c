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
#include "xattr.h"
#include "ts.h"

struct formatted_descriptor {
	uint16_t service_id;
	char service_type[64];
};

static const char *translate_service(uint8_t service_type)
{
	switch (service_type) {
		case 0x00: return "Reserved for future use";
		case 0x01: return "Digital television service";
		case 0x02: return "Digital audio service";
		case 0x03: return "Teletext service";
		case 0x04: return "NVOD reference service";
		case 0x05: return "Time-shifted NVOD service";
		case 0x06: return "Mosaic service";
		case 0x07 ... 0x09: return "Reserved for future use";
		case 0x0a: return "Advanced codification for digital radio service";
		case 0x0b: return "Advanced codification for mosaic service";
		case 0x0c: return "Data transmission service";
		case 0x0d: return "Reserved for common-use interface (see EN 50221)";
		case 0x0e: return "RCS map (see EN 301 790)";
		case 0x0f: return "RCS FLS (see EN 301 790)";
		case 0x10: return "DVB MHP service";
		case 0x11: return "MPEG-2 HD digital TV service";
		case 0x12 ... 0x15: return "Reserved for future use";
		case 0x16: return "Advanced service codification for SD digital TV";
		case 0x17: return "Advanced service codification for SD time-shifted NVOD";
		case 0x18: return "Advanced service codification for SD NVOD reference";
		case 0x19: return "Advanced service codification for HD digital TV";
		case 0x1a: return "Advanced service codification for HD time-shifted NVOD";
		case 0x1b: return "Advanced service codification for HD NVOD reference";
		case 0x1c ... 0x7f: return "Reserved for future use";
		case 0x80 ... 0xa0: return "Defined by the service provider";
		case 0xa1: return "Special video service";
		case 0xa2: return "Special audio service";
		case 0xa3: return "Special data service";
		case 0xa4: return "Engineering service";
		case 0xa5: return "Promotional video service";
		case 0xa6: return "Promotional audio service";
		case 0xa7: return "Promotional data service";
		case 0xa8: return "Anticipated data storage service";
		case 0xa9: return "Data storage only service";
		case 0xaa: return "Bookmark service list";
		case 0xab: return "Simultaneous server-type service";
		case 0xac: return "File-independent service";
		case 0xad ... 0xbf: return "Undefined";
		case 0xc0: return "Data service";
		default: return "Undefined";
	}
}

/* SERVICE_LIST_DESCRIPTOR parser */
int descriptor_0x41_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	uint8_t i;
	char buf[64];
	struct dentry *dentry, *subdir, *pat_programs;
	struct formatted_descriptor f;
	
	snprintf(buf, sizeof(buf), "/%s/%s/%s", FS_PAT_NAME, FS_CURRENT_NAME, FS_PROGRAMS_NAME);
	pat_programs = fsutils_get_dentry(priv->root, buf);
	if (! pat_programs) {
		TS_WARNING("%s doesn't exit", buf);
		return -1;
	}

	dentry = CREATE_DIRECTORY(parent, "SERVICE_LIST");

	for (i=0; i<len; i+=3) {
		uint8_t service_type;
		f.service_id = CONVERT_TO_16(payload[i], payload[i+1]);
		service_type = payload[i+2];
		sprintf(f.service_type, "%s [%#x]", translate_service(service_type), service_type);
		
		sprintf(buf, "SERVICE_%02d", (i/3)+1);
		subdir = CREATE_DIRECTORY(dentry, buf);
		CREATE_FILE_NUMBER(subdir, &f, service_id);
		CREATE_FILE_STRING(subdir, &f, service_type, XATTR_FORMAT_STRING_AND_NUMBER);
		
		sprintf(buf, "%#04x", f.service_id);
		if (! fsutils_get_child(pat_programs, buf))
			TS_WARNING("service_id %#x not declared by the PAT", f.service_id);
	}
    return 0;
}

