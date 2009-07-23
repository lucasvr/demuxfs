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
#include "biop.h"
#include "ts.h"

static int biop_parse_message_header(struct biop_message_header *msg,
	const char *buf, uint32_t len)
{
	int j = 0;

	msg->magic = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
	msg->biop_version_major = buf[j+4];
	msg->biop_version_minor = buf[j+5];
	msg->byte_order = buf[j+6];
	msg->message_type = buf[j+7];
	msg->message_size = CONVERT_TO_32(buf[j+8], buf[j+9], buf[j+10], buf[j+11]);
	msg->object_key_length = buf[j+12];
	if (msg->object_key_length) {
		msg->object_key_data = calloc(msg->object_key_length, sizeof(char));
		memcpy(msg->object_key_data, &buf[j+13], msg->object_key_length);
	}
	j += 13 + msg->object_key_length;
	msg->object_kind[0] = buf[j];
	msg->object_kind[1] = buf[j+1];
	msg->object_kind[2] = buf[j+2];
	msg->object_kind[3] = buf[j+3];

	return j+4;
}

static int biop_parse_object_location(struct biop_object_location *ol,
	const char *buf, uint32_t len)
{
	int j = 0;

	ol->object_location_tag = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
	ol->object_location_length = buf[j+4];
	ol->carousel_id = CONVERT_TO_32(buf[j+5], buf[j+6], buf[j+7], buf[j+8]);
	ol->module_id = CONVERT_TO_16(buf[j+9], buf[j+10]);
	ol->version_major = buf[j+11];
	ol->version_minor = buf[j+12];
	ol->object_key_length = buf[j+13];
	j += 14;
	switch (ol->object_key_length) {
		case 1:
			ol->object_key = buf[j] & 0xff;
			break;
		case 2:
			ol->object_key = CONVERT_TO_16(buf[j], buf[j+1]) & 0xff;
			break;
		case 3:
			ol->object_key = CONVERT_TO_24(buf[j], buf[j+1], buf[j+2]) & 0xff;
			break;
		case 4:
			ol->object_key = CONVERT_TO_32(buf[j], buf[j+1], buf[j+3], buf[j+4]);
			break;
		default:
			dprintf("object_key_length indicates more than 4 bytes, cannot parse object_key");
	}
	j += ol->object_key_length;

	return j;
}

static int biop_parse_connbinder(struct biop_connbinder *cb, const char *buf, uint32_t len)
{
	int i, j = 0;
	
	cb->connbinder_tag = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
	cb->connbinder_length = buf[j+4];
	cb->tap_count = buf[j+5];
	j += 6;

	if (cb->tap_count) {
		cb->taps = calloc(cb->tap_count, sizeof(struct dsmcc_tap));
		for (i=0; i<cb->tap_count; ++i) {
			struct dsmcc_tap *tap = &cb->taps[i];
			tap->tap_id = CONVERT_TO_16(buf[j], buf[j+1]);
			tap->tap_use = CONVERT_TO_16(buf[j+2], buf[j+3]);
			tap->association_tag = CONVERT_TO_16(buf[j+4], buf[j+5]);
			j += 6;

			if (tap->tap_use == BIOP_DELIVERY_PARA_USE) {
				struct message_selector *s = calloc(1, sizeof(struct message_selector));
				s->selector_length = buf[j];
				s->selector_type = CONVERT_TO_16(buf[j+1], buf[j+2]);
				s->transaction_id = CONVERT_TO_32(buf[j+3], buf[j+4], buf[j+5], buf[j+6]);
				s->timeout = CONVERT_TO_32(buf[j+7], buf[j+8], buf[j+9], buf[j+10]);
				j += 11;
				tap->message_selector = s;
			} else if (tap->tap_use == BIOP_OBJECT_USE) {
				/* 
				 * TODO
				 * This is not expected to be in a primary tap and at least the ATSC standard
				 * doesn't expect more than 1 tap. We need to verify what SBTVD, ISDB and DVB
				 * presume here.
				 */
				dprintf("BIOP_OBJECT_USE parser not implemented");
			} else {
				dprintf("Unsupported tap_use value %#x, cannot parse selector() field.", tap->tap_use);
			}
		}
	}

	return j;
}

static int biop_parse_profile_body(struct biop_tagged_profile *profile, 
	const char *buf, uint32_t len)
{
	struct biop_profile_body *pb = calloc(1, sizeof(struct biop_profile_body));
	int j = 0;
	
	pb->profile_id_tag = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
	pb->profile_data_length = CONVERT_TO_32(buf[j+4], buf[j+5], buf[j+6], buf[j+7]);
	pb->profile_data_byte_order = buf[j+8];
	pb->component_count = buf[j+9];
	j += 10;

	j += biop_parse_object_location(&pb->object_location, &buf[j], len-j);
	j += biop_parse_connbinder(&pb->connbinder, &buf[j], len-j);
	
	if (pb->component_count-2 > 0) {
		dprintf("component_count=%d, but LiteOptionsComponent() parser is not implemented.",
			pb->component_count);
	}
		
	profile->profile_body = pb;
	return j;
}

/* Returns how many bytes were parsed */
int biop_parse_tagged_profiles(struct biop_tagged_profile *profile, uint32_t count, 
	const char *buf, uint32_t len)
{
	int j = 0;
	uint32_t i;

	for (i=0; i<count; ++i) {
		struct biop_tagged_profile *p = &profile[i];

		/* Prefetch identification tag */
		uint32_t id_tag = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);

		switch (id_tag) {
			case 0x42494F50: /* "BIOP" */
				dprintf("BIOP parser not implemented");
				break;
			case 0x49534f05: /* Lite Options profile */
				dprintf("Lite Options profile parser not implemented");
				break;
			case 0x49534f06: /* BIOP profile */
				dprintf("Invoking BIOP profile parser");
				j += biop_parse_profile_body(p, &buf[j], len-j);
				break;
			case 0x49534f40: /* ConnBinder */
				if (! p->profile_body) {
					dprintf("ConnBinder parser invoked, but structure has no profile_body!");
					break;
				}
				dprintf("Invoking ConnBinder parser");
				j += biop_parse_connbinder(&p->profile_body->connbinder, &buf[j], len-j);
				break;
			case 0x49534f46: /* Service location */
				dprintf("Service Location parser not implemented");
				break;
			case 0x49534f50: /* Object location */
				dprintf("Object Location parser not implemented");
				break;
			default:
				dprintf("Unknown profile, cannot parse");
		}
	}

	return j;
}

static int biop_create_biop_msg_header_dentries(struct dentry *parent, 
	struct biop_message_header *msg_header)
{
	struct dentry *biop_dentry = CREATE_DIRECTORY(parent, FS_BIOP_MESSAGE_DIRNAME);
	CREATE_FILE_NUMBER(biop_dentry, msg_header, magic);
	CREATE_FILE_NUMBER(biop_dentry, msg_header, biop_version_major);
	CREATE_FILE_NUMBER(biop_dentry, msg_header, biop_version_minor);
	CREATE_FILE_NUMBER(biop_dentry, msg_header, byte_order);
	CREATE_FILE_NUMBER(biop_dentry, msg_header, message_type);
	CREATE_FILE_NUMBER(biop_dentry, msg_header, message_size);
	CREATE_FILE_NUMBER(biop_dentry, msg_header, object_key_length);
	CREATE_FILE_BIN(biop_dentry, msg_header, object_key_data, msg_header->object_key_length);
	CREATE_FILE_BIN(biop_dentry, msg_header, object_kind, 4);
	return 0;
}

/* Returns 0 on success, -1 on error */
int biop_create_tagged_profiles_dentries(struct dentry *parent, struct biop_tagged_profile *profile)
{
	if (profile->profile_body) {
		struct dentry *body_dentry = CREATE_DIRECTORY(parent, FS_BIOP_PROFILE_BODY_DIRNAME);
		struct biop_profile_body *pb = profile->profile_body;
	
		CREATE_FILE_NUMBER(body_dentry, pb, profile_id_tag);
		CREATE_FILE_NUMBER(body_dentry, pb, profile_data_length);
		CREATE_FILE_NUMBER(body_dentry, pb, profile_data_byte_order);
		CREATE_FILE_NUMBER(body_dentry, pb, component_count);
		if (pb->profile_data_byte_order != 0)
			TS_WARNING("profile_data_byte_order != 0");

		struct dentry *obj_dentry = CREATE_DIRECTORY(body_dentry, FS_BIOP_OBJECT_LOCATION_DIRNAME);
		struct biop_object_location *ol = &pb->object_location;
		
		CREATE_FILE_NUMBER(obj_dentry, ol, object_location_tag);
		CREATE_FILE_NUMBER(obj_dentry, ol, object_location_length);
		CREATE_FILE_NUMBER(obj_dentry, ol, carousel_id);
		CREATE_FILE_NUMBER(obj_dentry, ol, module_id);
		CREATE_FILE_NUMBER(obj_dentry, ol, version_major);
		CREATE_FILE_NUMBER(obj_dentry, ol, version_minor);
		CREATE_FILE_NUMBER(obj_dentry, ol, object_key_length);
		CREATE_FILE_NUMBER(obj_dentry, ol, object_key);
		if (ol->object_location_tag != 0x49534f50)
			TS_WARNING("object_location_tag != 0x49534f50");
		if (ol->module_id >= 0xfff0 && ol->module_id <= 0xffff)
			TS_WARNING("module_id contains a reserved value");
		if (ol->version_major != 0x01)
			TS_WARNING("version_major != 0x01");
		if (ol->version_minor != 0x00)
			TS_WARNING("version_minor != 0x00");

		/* TODO: carousel_id must match with DII->download_id and DDB->download_id */

		struct dentry *cb_dentry = CREATE_DIRECTORY(body_dentry, FS_BIOP_CONNBINDER_DIRNAME);
		struct biop_connbinder *cb = &pb->connbinder;

		CREATE_FILE_NUMBER(cb_dentry, cb, connbinder_tag);
		CREATE_FILE_NUMBER(cb_dentry, cb, connbinder_length);
		CREATE_FILE_NUMBER(cb_dentry, cb, tap_count);
		if (cb->connbinder_tag != 0x49534f40)
			TS_WARNING("connbinder_tag != 0x49534f40");

		for (int i=0; i<cb->tap_count; ++i) {
			struct dentry *tap_dentry = CREATE_DIRECTORY(cb_dentry, "tap_%02d", i+1);
			struct dsmcc_tap *tap = &cb->taps[i];
			
			CREATE_FILE_NUMBER(tap_dentry, tap, tap_id);
			CREATE_FILE_NUMBER(tap_dentry, tap, tap_use);
			CREATE_FILE_NUMBER(tap_dentry, tap, association_tag);
			if (tap->tap_id != 0xffff)
				TS_WARNING("tap_id != 0xffff");

			if (tap->message_selector) {
				struct dentry *selector_dentry = CREATE_DIRECTORY(tap_dentry, 
						FS_DSMCC_MESSAGE_SELECTOR_DIRNAME);
				struct message_selector *ms = tap->message_selector;

				CREATE_FILE_NUMBER(selector_dentry, ms, selector_length);
				CREATE_FILE_NUMBER(selector_dentry, ms, selector_type);
				CREATE_FILE_NUMBER(selector_dentry, ms, transaction_id);
				CREATE_FILE_NUMBER(selector_dentry, ms, timeout);
				if (ms->selector_length != 0x0a)
					TS_WARNING("selector_length != 0x0a");
				if (ms->selector_type != 0x01)
					TS_WARNING("selector_type != 0x01");
			}
		}
	} else if (profile->lite_body) {
		/* TODO */
	}
	return 0;
}


/* Returns how many bytes were parsed */
int biop_parse_module_info(struct biop_module_info *modinfo, const char *buf, uint32_t len)
{
	int i, j = 0;

	modinfo->module_timeout = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
	modinfo->block_timeout = CONVERT_TO_32(buf[j+4], buf[j+5], buf[j+6], buf[j+7]);
	modinfo->min_block_time = CONVERT_TO_32(buf[j+8], buf[j+9], buf[j+10], buf[j+11]);
	modinfo->taps_count = buf[j+12];
	j += 13;

	if (modinfo->taps_count) {
		modinfo->taps = calloc(modinfo->taps_count, sizeof(struct biop_module_tap));
		for (i=0; i<modinfo->taps_count; ++i) {
			struct biop_module_tap *tap = &modinfo->taps[i];
			tap->tap_id = CONVERT_TO_16(buf[j], buf[j+1]);
			tap->tap_use = CONVERT_TO_16(buf[j+2], buf[j+3]);
			tap->association_tag = CONVERT_TO_16(buf[j+4], buf[j+5]);
			tap->selector_length = buf[j+6];
			j += 7;
		}
	}

	modinfo->user_info_length = buf[j];
	if (modinfo->user_info_length) {
		modinfo->user_info = calloc(modinfo->user_info_length, sizeof(char));
		memcpy(modinfo->user_info, &buf[j+1], modinfo->user_info_length);
	}
	j += 1 + modinfo->user_info_length;

	return j;
}

/* Returns 0 on success, -1 on error */
int biop_create_module_info_dentries(struct dentry *parent, struct biop_module_info *modinfo)
{
	struct dentry *mod_dentry = CREATE_DIRECTORY(parent, FS_BIOP_MODULE_INFO_DIRNAME);
	CREATE_FILE_NUMBER(mod_dentry, modinfo, module_timeout);
	CREATE_FILE_NUMBER(mod_dentry, modinfo, block_timeout);
	CREATE_FILE_NUMBER(mod_dentry, modinfo, min_block_time);
	CREATE_FILE_NUMBER(mod_dentry, modinfo, taps_count);

	for (int i=0; i<modinfo->taps_count; ++i) {
		struct dentry *tap_dentry = CREATE_DIRECTORY(mod_dentry, "tap_%02d", i+1);
		struct biop_module_tap *tap = &modinfo->taps[i];
		CREATE_FILE_NUMBER(tap_dentry, tap, tap_id);
		CREATE_FILE_NUMBER(tap_dentry, tap, tap_use);
		CREATE_FILE_NUMBER(tap_dentry, tap, association_tag);
		CREATE_FILE_NUMBER(tap_dentry, tap, selector_length);
	}

	CREATE_FILE_NUMBER(mod_dentry, modinfo, user_info_length);
	if (modinfo->user_info_length)
		CREATE_FILE_BIN(mod_dentry, modinfo, user_info, modinfo->user_info_length);

	return 0;
}
