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

struct component {
	uint8_t component_tag;
	uint8_t _digital_recording_control_data:2;
	uint8_t maximum_bitrate_flag:1;
	uint8_t reserved_future_use:1;
	uint8_t _copy_control_type:2;
	union {
		uint8_t APS_control_data:2;
		uint8_t reserved_future_use:2;
	} u;
	uint8_t maximum_bitrate;
	/* formatted strings */
	char digital_recording_control_data[64];
	char copy_control_type[64];
	char APS_control_data[64];
};

struct formatted_descriptor {
	uint8_t _digital_recording_control_data:2;
	uint8_t maximum_bitrate_flag:1;
	uint8_t component_control_flag:1;
	uint8_t _copy_control_type:2;
	union {
		uint8_t APS_control_data:2;
		uint8_t reserved_future_use:2;
	} u;
	uint8_t maximum_bitrate;
	uint8_t component_control_length;
	struct component c;
	/* formatted strings */
	char digital_recording_control_data[64];
	char copy_control_type[64];
	char APS_control_data[64];
};

void interpret_digital_recording_control_data(char *buf, size_t len, int drcd)
{
	switch (drcd) {
		case 0x0:
			snprintf(buf, len, "Free to copy with no control restrictions [%#x]", drcd);
			break;
		case 0x1:
			snprintf(buf, len, "Digital copy rights defined by the service provider [%#x]", drcd);
			break;
		case 0x2:
			snprintf(buf, len, "Digital copy allowed only once [%#x]", drcd);
			break;
		case 0x3:
			snprintf(buf, len, "Digital copy prohibited [%#x]", drcd);
			break;
	}
}

void interpret_copy_control_type(char *buf, size_t len, int cct)
{
	switch (cct) {
		case 0x0:
			snprintf(buf, len, "Undefined [%#x]", cct);
			break;
		case 0x1:
			snprintf(buf, len, "Encrypted video output [%#x]", cct);
			break;
		case 0x2:
			snprintf(buf, len, "Undefined [%#x]", cct);
			break;
		case 0x3:
			snprintf(buf, len, "Non-encrypted video output [%#x]", cct);
			break;
	}
}

void interpret_aps_control_data(char *buf, size_t len, int aps)
{
	if (aps == 0x0)
		snprintf(buf, len, "Free to copy with no control restrictions [%#x]", aps);
	else
		snprintf(buf, len, "Video resolution limited to 350 000 pixels on copy [%#x]", aps);
}

/* DIGITAL_COPY_CONTROL_DESCRIPTOR parser */
int descriptor_0xc1_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv)
{
	struct dentry *dentry, *subdir;
	struct formatted_descriptor f;
	int i;

	if (! descriptor_is_parseable(parent, payload[0], 3, len))
		return -ENODATA;

	dentry = CREATE_DIRECTORY(parent, "Digital_Copy_Control_Descriptor");

	f._digital_recording_control_data = (payload[2] >> 6) & 0x03;
	interpret_digital_recording_control_data(f.digital_recording_control_data, 
		sizeof(f.digital_recording_control_data), f._digital_recording_control_data);
	CREATE_FILE_STRING(dentry, &f, digital_recording_control_data, XATTR_FORMAT_STRING_AND_NUMBER);

	f.maximum_bitrate_flag = (payload[2] >> 5) & 0x01;
	CREATE_FILE_NUMBER(dentry, &f, maximum_bitrate_flag);

	f.component_control_flag = (payload[2] >> 4) & 0x01;
	CREATE_FILE_NUMBER(dentry, &f, component_control_flag);

	f._copy_control_type = (payload[2] >> 2) & 0x03;
	interpret_copy_control_type(f.copy_control_type, sizeof(f.copy_control_type), f._copy_control_type);
	CREATE_FILE_STRING(dentry, &f, copy_control_type, XATTR_FORMAT_STRING_AND_NUMBER);

	if (f.copy_control_type != 0x00) {
		f.u.APS_control_data = payload[2] & 0x03;
		interpret_aps_control_data(f.APS_control_data, sizeof(f.APS_control_data), f.u.APS_control_data);
		CREATE_FILE_STRING(dentry, &f, APS_control_data, XATTR_FORMAT_STRING_AND_NUMBER);
	} else
		f.u.reserved_future_use = payload[2] & 0x03;
	if (f.maximum_bitrate_flag) {
		if (len < 4) {
			TS_WARNING("Not enough data");
			return -ENODATA;
		}
		f.maximum_bitrate = payload[3];
		CREATE_FILE_NUMBER(dentry, &f, maximum_bitrate);
	}
	i = 3 + f.maximum_bitrate_flag;

	if (f.component_control_flag) {
		int component_id = 0, loop_len;
		struct component *comp = &f.c;

		f.component_control_length = payload[i++];
		loop_len = f.component_control_length;
		CREATE_FILE_NUMBER(dentry, &f, component_control_length);

		while (loop_len > 0) {
			subdir = CREATE_DIRECTORY(dentry, "Component_%02d", component_id++);

			comp->component_tag = payload[i];
			CREATE_FILE_NUMBER(subdir, comp, component_tag);

			comp->_digital_recording_control_data = (payload[i+1] >> 6) & 0x03;
			interpret_digital_recording_control_data(comp->digital_recording_control_data, 
					sizeof(comp->digital_recording_control_data), comp->_digital_recording_control_data);
			CREATE_FILE_STRING(subdir, comp, digital_recording_control_data, XATTR_FORMAT_STRING_AND_NUMBER);

			comp->maximum_bitrate_flag = (payload[i+1] >> 5) & 0x01;
			CREATE_FILE_NUMBER(subdir, comp, maximum_bitrate_flag);

			comp->reserved_future_use = (payload[i+1] >> 4) & 0x01;

			comp->_copy_control_type = (payload[i+1] >> 2) & 0x03;
			interpret_copy_control_type(comp->copy_control_type, sizeof(comp->copy_control_type), 
					comp->_copy_control_type);
			CREATE_FILE_STRING(subdir, comp, copy_control_type, XATTR_FORMAT_STRING_AND_NUMBER);

			if (comp->copy_control_type != 0x00) {
				comp->u.APS_control_data = payload[i+1] & 0x03;
				interpret_aps_control_data(comp->APS_control_data, sizeof(comp->APS_control_data), 
						comp->u.APS_control_data);
				CREATE_FILE_STRING(subdir, comp, APS_control_data, XATTR_FORMAT_STRING_AND_NUMBER);
			} else
				comp->u.reserved_future_use = payload[i+1] & 0x03;
			if (comp->maximum_bitrate_flag) {
				comp->maximum_bitrate = payload[i+2];
				CREATE_FILE_NUMBER(subdir, &f, maximum_bitrate);
			}
			i += 2 + comp->maximum_bitrate_flag;
			loop_len -= 2 + comp->maximum_bitrate_flag;
		}
	}

    return 0;
}

