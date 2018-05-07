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

struct formatted_descriptor {
	char broadcasting_flag[64];
	char broadcasting_identifier[64];
	uint8_t additional_broadcasting_identification;
	char additional_identification_information[256];
};

/* SYSTEM_MANAGEMENT_DESCRIPTOR parser */
int descriptor_0xfe_parser(const char *payload, int len, struct dentry *parent, 
		struct demuxfs_data *priv)
{
	struct dentry *dentry;
	struct formatted_descriptor f;
	uint16_t bflag, bid;
	int i;
	
	if (! descriptor_is_parseable(parent, payload[0], 2, len))
		return -ENODATA;

	dentry = CREATE_DIRECTORY(parent, "System_Management_Descriptor");

	bflag = (payload[2] >> 6);
	if (bflag == 0)
		sprintf(f.broadcasting_flag, "Broadcasting [%#x]", bflag);
	else if (bflag == 1 || bflag == 2)
		sprintf(f.broadcasting_flag, "Non-broadcasting [%#x]", bflag);
	else
		sprintf(f.broadcasting_flag, "Reserved [%#x]", bflag);

	bid = payload[2] & 0x3f;
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
	f.additional_broadcasting_identification = payload[3];

	len -= 2;
	for (i=0; i<len; ++i)
		f.additional_identification_information[i] = payload[4+i];
	f.additional_identification_information[i] = '\0';

	CREATE_FILE_STRING(dentry, &f, broadcasting_flag, XATTR_FORMAT_STRING_AND_NUMBER);
	CREATE_FILE_STRING(dentry, &f, broadcasting_identifier, XATTR_FORMAT_STRING_AND_NUMBER);
	CREATE_FILE_NUMBER(dentry, &f, additional_broadcasting_identification);
	CREATE_FILE_STRING(dentry, &f, additional_identification_information, XATTR_FORMAT_STRING);

    return 0;
}

