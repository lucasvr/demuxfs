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

int iop_create_tagged_profiles_dentries(struct dentry *parent, struct iop_tagged_profile *profile)
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
		dprintf("LiteBody profile parser not implemented");
	}
	return 0;
}

int iop_create_ior_dentries(struct dentry *parent, struct iop_ior *ior)
{
	CREATE_FILE_NUMBER(parent, ior, type_id_length);
	CREATE_FILE_STRING(parent, ior, type_id, XATTR_FORMAT_STRING);
	if (ior->type_id_length % 4)
		CREATE_FILE_BIN(parent, ior, alignment_gap, ior->type_id_length % 4);

	CREATE_FILE_NUMBER(parent, ior, tagged_profiles_count);
	if (ior->tagged_profiles_count)
		iop_create_tagged_profiles_dentries(parent, ior->tagged_profiles);

	return 0;
}

/* Returns how many bytes were parsed */
int iop_parse_tagged_profiles(struct iop_tagged_profile *profile, uint32_t count, 
	const char *buf, uint32_t len)
{
	int j = 0, x = 0;
	uint32_t i;

	for (i=0; i<count; ++i) {
		struct iop_tagged_profile *p = &profile[i];

		/* Prefetch identification tag and data length */
		uint32_t id_tag = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
		uint32_t data_length = CONVERT_TO_32(buf[j+4], buf[j+5], buf[j+6], buf[j+7]);

		switch (id_tag) {
			case 0x42494F50: /* "BIOP" */
				dprintf("BIOP parser not implemented");
				break;
			case 0x49534f05: /* Lite Options profile */
				dprintf("Lite Options profile parser not implemented");
				break;
			case 0x49534f06: /* BIOP profile */
				x = biop_parse_profile_body(p, &buf[j], len-j);
				break;
			case 0x49534f40: /* ConnBinder */
				if (! p->profile_body) {
					dprintf("ConnBinder parser invoked, but structure has no profile_body!");
					break;
				}
				dprintf("Invoking ConnBinder parser");
				x = biop_parse_connbinder(&p->profile_body->connbinder, &buf[j], len-j);
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
		j += 8 + data_length;
		if (data_length+8 != x)
			dprintf("Error: Parsed %d bytes, data_length=%d+8", x, data_length);
	}

	return j;
}

void iop_free_ior(struct iop_ior *ior)
{
	int i;

	if (ior->type_id) {
		free(ior->type_id);
		ior->type_id = NULL;
	}
	if (ior->tagged_profiles) {
		for (i=0; i<ior->tagged_profiles_count; ++i) {
			struct iop_tagged_profile *p = &ior->tagged_profiles[i];
			if (p->profile_body) {
				biop_free_profile_body(p);
				p->profile_body = NULL;
			}

		}
		free(ior->tagged_profiles);
		ior->tagged_profiles = NULL;
	}
	free(ior);
}

int iop_parse_ior(struct iop_ior *ior, const char *payload, uint32_t len)
{
	int j = 0;

	ior->type_id_length = CONVERT_TO_32(payload[j], payload[j+1], payload[j+2], payload[j+3]);
	if (ior->type_id_length != 4)
		TS_WARNING("ior->type_id_length != 4 (%#x)", ior->type_id_length);
	j += 4;

	ior->type_id = calloc(ior->type_id_length+1, sizeof(char));
	memcpy(ior->type_id, &payload[j], ior->type_id_length);
	j += ior->type_id_length;
	
	uint8_t gap_bytes = ior->type_id_length % 4;
	if (gap_bytes) {
		memcpy(ior->alignment_gap, &payload[j], 4 - gap_bytes);
		j += 4 - gap_bytes;
	}

	ior->tagged_profiles_count = CONVERT_TO_32(payload[j], payload[j+1], payload[j+2], payload[j+3]);
	j += 4;
	if (ior->tagged_profiles_count) {
		/* Parse tagged profiles */
		ior->tagged_profiles = calloc(ior->tagged_profiles_count, sizeof(struct iop_tagged_profile));
		j += iop_parse_tagged_profiles(ior->tagged_profiles, ior->tagged_profiles_count, 
				&payload[j], len-j);
	}

	return j;
}
