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
	char broadcasting_flag[64];
	char broadcasting_identifier[64];
	uint8_t additional_broadcasting_identification;
};

/* SYSTEM_MANAGEMENT_DESCRIPTOR parser */
int descriptor_0xfe_parser(const char *payload, int len, struct dentry *parent, 
		struct demuxfs_data *priv)
{
	struct dentry *dentry;
	struct formatted_descriptor f;
	uint16_t bflag, bid;
	int i;
	
	if (len < 2) {
		TS_WARNING("cannot parse descriptor %#x: contents smaller than 2 bytes (%d)", 0xfe, len);
		return -1;
	}

	dentry = CREATE_DIRECTORY(parent, "SYSTEM_MANAGEMENT_DESCRIPTOR");

	bflag = (payload[0] >> 6);
	if (bflag == 0)
		sprintf(f.broadcasting_flag, "Broadcasting [%#x]", bflag);
	else if (bflag == 1 || bflag == 2)
		sprintf(f.broadcasting_flag, "Non-broadcasting [%#x]", bflag);
	else
		sprintf(f.broadcasting_flag, "Reserved [%#x]", bflag);

	bid = payload[0] & 0x3f;
	if (priv->options.standard == SBTVD_STANDARD) {
		if (bid == 0 || bid >= 7)
			sprintf(f.broadcasting_identifier, "Undefined [%#x]", bid);
		else if (bid == 3)
			sprintf(f.broadcasting_identifier, "ISDB [%#x]", bid);
		else
			sprintf(f.broadcasting_identifier, "Not used [%#x]", bid);
	} else if (priv->options.standard == ISDB_STANDARD) {
		if (bid == 0 || bid >= 6)
			sprintf(f.broadcasting_identifier, "Reserved [%#x]", bid);
		else if (bid == 1)
			sprintf(f.broadcasting_identifier, "CS digital broadcasting [%#x]", bid);
		else if (bid == 2)
			sprintf(f.broadcasting_identifier, "BS digital broadcasting [%#x]", bid);
		else if (bid == 3)
			sprintf(f.broadcasting_identifier, "Terrestrial digital television broadcasting [%#x]", bid);
		else if (bid == 4)
			sprintf(f.broadcasting_identifier, "Broadband CS digital broadcasting [%#x]", bid);
		else if (bid == 5)
			sprintf(f.broadcasting_identifier, "Terrestrial digital audio broadcasting [%#x]", bid);
		else
			sprintf(f.broadcasting_identifier, "Unknown [%#x]", bid);
	}

	f.additional_broadcasting_identification = payload[1];

	CREATE_FILE_STRING(dentry, &f, broadcasting_flag, XATTR_FORMAT_STRING_AND_NUMBER);
	CREATE_FILE_STRING(dentry, &f, broadcasting_identifier, XATTR_FORMAT_STRING_AND_NUMBER);
	CREATE_FILE_NUMBER(dentry, &f, additional_broadcasting_identification);

	len -= 2;
	dprintf("Additional identification info: %d bytes", len);
	for (i=0; i<len; ++i) {
		/* TODO: Additional identification info */
	}

    return 0;
}

