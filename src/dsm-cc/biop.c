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
#include "byteops.h"
#include "fsutils.h"
#include "xattr.h"
#include "biop.h"
#include "iop.h"
#include "ts.h"
#include "debug.h"

static ino_t biop_get_sub_header_inode(struct biop_message_sub_header *sub_header)
{
	ino_t inode = 0;
	if (sub_header)
		inode = CONVERT_TO_32(
			sub_header->object_key.object_key[0],
			sub_header->object_key.object_key[1],
			sub_header->object_key.object_key[2],
			sub_header->object_key.object_key[3]
		);
	return inode;
}

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
	j += 12;

	if (msg->magic != 0x42494f50)
		TS_WARNING("magic != 0x42494f50/'BIOP' (%#x)", msg->magic);
	if (msg->biop_version_major != 0x01)
		TS_WARNING("biop_version_major != 0x01 (%#x)", msg->biop_version_major);
	if (msg->biop_version_minor != 0x00)
		TS_WARNING("biop_version_minor != 0x00 (%#x)", msg->biop_version_minor);
	if (msg->byte_order != 0x00)
		TS_WARNING("byte_order != 0x00 (%#x)", msg->byte_order);
	if (msg->message_type != 0x00)
		TS_WARNING("message_type != 0x00 (%#x)", msg->message_type);

	return j;
}

static int biop_parse_message_sub_header(struct biop_message_sub_header *sub_header,
	const char *buf, uint32_t len)
{
	struct biop_object_key *obj_key = &sub_header->object_key;
	int x, j = 0;

	obj_key->object_key_length = buf[j];
	if (obj_key->object_key_length) {
		obj_key->object_key = calloc(obj_key->object_key_length, sizeof(char));
		memcpy(obj_key->object_key, &buf[j+1], obj_key->object_key_length);
	}
	j += 1 + obj_key->object_key_length;

	sub_header->object_kind_length = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
	sub_header->object_kind_data = CONVERT_TO_32(buf[j+4], buf[j+5], buf[j+6], buf[j+7]);
	sub_header->object_info_length = CONVERT_TO_16(buf[j+8], buf[j+9]);
	j += 10;

	if (sub_header->object_kind_data == 0x66696c00) { /* "fil" */
		struct biop_file_object_info *file_info = calloc(1, sizeof(struct biop_file_object_info));
		file_info->content_size = CONVERT_TO_64(buf[j], buf[j+1], buf[j+2], buf[j+3],
			buf[j+4], buf[j+5], buf[j+6], buf[j+7]);
		sub_header->obj_info.file_object_info = file_info;
		j += 8;
		/* Descriptor has been parsed already */
		for (x=0; x<sub_header->object_info_length-8; ++x) {
			uint8_t descriptor_length = buf[j+1];
			j += descriptor_length;
		}
	} else {
		/* Descriptor has been parsed already */
		for (x=0; x<sub_header->object_info_length; ++x) {
			uint8_t descriptor_length = buf[j+1];
			j += descriptor_length;
		}
	}

	sub_header->service_context_list_count = buf[j];
	j += 1;
	if (sub_header->service_context_list_count) {
		sub_header->service_context = calloc(sub_header->service_context_list_count, sizeof(struct biop_service_context));
		for (uint8_t i=0; i<sub_header->service_context_list_count; ++i) {
			struct biop_service_context *ctx = &sub_header->service_context[i];

			ctx->context_id = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
			ctx->context_data_length = CONVERT_TO_16(buf[j+4], buf[j+5]);
			if (ctx->context_data_length) {
				ctx->context_data = calloc(ctx->context_data_length, sizeof(char));
				memcpy(ctx->context_data, &buf[j+6], ctx->context_data_length);
			}
			j += 6 + ctx->context_data_length;
		}
	}
	return j;
}

static int biop_parse_name(struct biop_name *name, const char *buf, uint32_t len)
{
	int j = 0;

	name->name_component_count = buf[j];
	name->id_length = buf[j+1];
	if (name->id_length) {
		name->id_byte = calloc(name->id_length+1, sizeof(char));
		memcpy(name->id_byte, &buf[j+2], name->id_length);
	}
	j += 2 + name->id_length;

	name->kind_length = buf[j++];
	name->kind_data = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
	if (name->kind_length != 4)
		TS_WARNING("kind_length != 4 (%d)", name->kind_length);
	j += name->kind_length;

	if (name->name_component_count != 1)
		TS_WARNING("name_component_count != 1 (%d)", name->name_component_count);

//	dprintf("name_component_count=%d", name->name_component_count);
//	dprintf("name_id_length=%d", name->id_length);
//	dprintf("name_id_byte=%s (%c%c%c)", name->id_byte,
//		(name->kind_data>>24) & 0xff,
//		(name->kind_data>>16) & 0xff,
//		(name->kind_data>>8) & 0xff);
//	dprintf("kind_length=%d", name->kind_length);
//	dprintf("kind_data=%#x", name->kind_data);

	return j;
}

static int biop_parse_descriptor(struct biop_binding *binding, const char *buf, uint32_t len)
{
	int j = 0;
	
	uint8_t descriptor_tag = buf[j];
	uint8_t descriptor_length = buf[j+1];
	
	/* TODO: check for mandatory vs optional descriptors */
	switch (descriptor_tag) {
		case 0x72: /* Content type */
			binding->_content_type = calloc(descriptor_length+1, sizeof(char));
			memcpy(binding->_content_type, &buf[j+2], descriptor_length);
			dprintf("content_type='%s'", binding->_content_type);
			break;
		case 0x81: /* Time stamp */
			binding->_timestamp = CONVERT_TO_64(buf[j+2], buf[j+3], buf[j+4],
				buf[j+5], buf[j+6], buf[j+7], buf[j+8], buf[j+9]);
			dprintf("time_stamp='%#zx'", binding->_timestamp);
			break;
		default:
			dprintf("Unsupported descriptor tag '%#x'", descriptor_tag);
	}
	j += 2 + descriptor_length;

	return j;
}

static void biop_free_message_sub_header(struct biop_message_sub_header *sub_header)
{
	struct biop_object_key *obj_key = &sub_header->object_key;
	int i;

	if (obj_key->object_key) {
		free(obj_key->object_key);
		obj_key->object_key = NULL;
	}
	if (sub_header->obj_info.file_object_info) {
		free(sub_header->obj_info.file_object_info);
		sub_header->obj_info.file_object_info = NULL;
	}
	if (sub_header->service_context) {
		for (i=0; i<sub_header->service_context_list_count; ++i) {
			struct biop_service_context *ctx = &sub_header->service_context[i];
			if (ctx->context_data) {
				free(ctx->context_data);
				ctx->context_data = NULL;
			}
		}
		free(sub_header->service_context);
		sub_header->service_context = NULL;
	}
}

static void biop_free_directory_message(struct biop_directory_message *msg)
{
	struct biop_directory_message_body *msg_body = &msg->message_body;
	int i;
	
	/* Free the MessageSubHeader */
	biop_free_message_sub_header(&msg->sub_header);

	/* Free the DirectoryMessageBody */
	if (msg_body->bindings) {
		for (i=0; i<msg_body->bindings_count; ++i) {
			struct biop_binding *binding = &msg_body->bindings[i];
			struct biop_name *name = &binding->name;
			if (name->id_byte) {
				free(name->id_byte);
				name->id_byte = NULL;
			}
			if (binding->iop_ior) {
				iop_free_ior(binding->iop_ior);
				binding->iop_ior = NULL;
			}
			if (binding->_content_type) {
				free(binding->_content_type);
				binding->_content_type = NULL;
			}
		}
		free(msg_body->bindings);
		msg_body->bindings = NULL;
	}
}

static void biop_free_file_message(struct biop_file_message *msg)
{
	/* Free the MessageSubHeader */
	biop_free_message_sub_header(&msg->sub_header);
}

/* The MessageHeader is expected to have been already parsed */
static int biop_parse_directory_message(struct biop_directory_message *msg,
	const char *buf, uint32_t len)
{
	struct biop_directory_message_body *msg_body = &msg->message_body;
	struct biop_message_sub_header *sub_header = &msg->sub_header;
	int ret, retval, x = 0, j = 0, sub_header_len;

	/* MessageSubHeader() */
	sub_header_len = biop_parse_message_sub_header(sub_header, &buf[j], len-j);
	j += sub_header_len;

	/* DirectoryMessageBody() */
	msg->message_body_length = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
	msg_body->bindings_count = CONVERT_TO_16(buf[j+4], buf[j+5]);
	retval = msg->message_body_length + j + 4;
	j += 6;

	if (msg_body->bindings_count) {
		msg_body->bindings = calloc(msg_body->bindings_count, sizeof(struct biop_binding));
		for (uint16_t i=0; i<msg_body->bindings_count; ++i) {
			struct biop_binding *binding = &msg_body->bindings[i];

			ret = biop_parse_name(&binding->name, &buf[j], len-j);
			if (ret < 0)
				break;
			j += ret;

			binding->binding_type = buf[j++];
			binding->iop_ior = calloc(1, sizeof(struct iop_ior));
			ret = iop_parse_ior(binding->iop_ior, &buf[j], len-j);
			if (ret < 0)
				break;
			j += ret;

			binding->child_object_info_length = CONVERT_TO_16(buf[j], buf[j+1]);
			j += 2;
			if (binding->child_object_info_length) {
				if (binding->name.kind_data == 0x66696c00) { /* "fil" */
					binding->content_size = CONVERT_TO_64(buf[j], buf[j+1], 
						buf[j+2], buf[j+3], buf[j+4], buf[j+5], buf[j+6], buf[j+7]);
					j += 8;

					if (binding->content_size & 0xffffffff00000000)
						TS_WARNING("binding %d has invalid content size: %#zx",
							i+1, binding->content_size);
					if (binding->child_object_info_length < 8)
						TS_WARNING("binding->child_object_info_length < 8 (%d)", 
							binding->child_object_info_length);
					if (binding->binding_type != 0x01)
						TS_WARNING("binding->binding_type != 0x01 (%#x)",
							binding->binding_type);

					for (x=0; x<binding->child_object_info_length-8; ++x)
						j += biop_parse_descriptor(binding, &buf[j], len-j);
				} else {
					for (x=0; x<binding->child_object_info_length; ++x)
						j += biop_parse_descriptor(binding, &buf[j], len-j);
				}
			}

			for (x=0; x<binding->iop_ior->tagged_profiles_count; ++x) {
				struct biop_profile_body *pb = binding->iop_ior->tagged_profiles[x].profile_body;
				if (pb) {
					binding->_inode = pb->object_location.object_key;
					break;
				}
			}
		}
	}

	if (retval != j)
		dprintf("Parsed %d elements, but message_body_length is %d", j, retval);

	return retval;
}

/* The MessageHeader is expected to have been already parsed */
static int biop_parse_file_message(struct biop_file_message *msg,
	const char *buf, uint32_t len)
{
	struct biop_file_message_body *msg_body = &msg->message_body;
	struct biop_message_sub_header *sub_header = &msg->sub_header;
	int retval, j = 0;

	j += biop_parse_message_sub_header(sub_header, &buf[j], len-j);

	msg->message_body_length = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
	msg_body->content_length = CONVERT_TO_32(buf[j+4], buf[j+5], buf[j+6], buf[j+7]);
	retval = msg->message_body_length + j + 4;

	if (msg_body->content_length) {
		/* Points to the user provided buffer */
		msg_body->contents = &buf[j+8];
	}
	j += 8 + msg_body->content_length;

	if (retval != j)
		dprintf("Parsed %d elements, but message_body_length is %d", j, retval);

	return retval;
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

	if (ol->version_major != 0x01)
		TS_WARNING("version_major != 0x01 (%#x)", ol->version_major);
	if (ol->version_minor != 0x00)
		TS_WARNING("version_minor != 0x00 (%#x)", ol->version_minor);

	switch (ol->object_key_length) {
		case 1:
			ol->object_key = buf[j] & 0xff;
			break;
		case 2:
			ol->object_key = CONVERT_TO_16(buf[j], buf[j+1]);
			break;
		case 3:
			ol->object_key = CONVERT_TO_24(buf[j], buf[j+1], buf[j+2]);
			break;
		case 4:
			ol->object_key = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
			break;
		default:
			dprintf("object_key_length indicates more than 4 bytes, cannot parse object_key");
			ol->object_key_length = 4;
	}
	j += ol->object_key_length;

	return j;
}

void biop_free_connbinder(struct biop_connbinder *cb)
{
	int i;
	if (cb->tap_count) {
		for (i=0; i<cb->tap_count; ++i) {
			struct dsmcc_tap *tap = &cb->taps[i];
			if (tap->message_selector) {
				free(tap->message_selector);
				tap->message_selector = NULL;
			}
		}
		free(cb->taps);
	}
}

int biop_parse_connbinder(struct biop_connbinder *cb, const char *buf, uint32_t len)
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

			if (tap->tap_id != 0x00)
				TS_WARNING("tap_id != 0x00 (%#x)", tap->tap_id);

			if (tap->tap_use == BIOP_DELIVERY_PARA_USE) {
				struct message_selector *s = calloc(1, sizeof(struct message_selector));
				s->selector_length = buf[j];
				s->selector_type = CONVERT_TO_16(buf[j+1], buf[j+2]);
				s->transaction_id = CONVERT_TO_32(buf[j+3], buf[j+4], buf[j+5], buf[j+6]);
				s->timeout = CONVERT_TO_32(buf[j+7], buf[j+8], buf[j+9], buf[j+10]);
				j += 11;

				if (s->selector_length != 0x0a)
					TS_WARNING("selector_length != 0x0a (%#x)", s->selector_length);
				if (s->selector_type != 0x0001)
					TS_WARNING("selector_type != 0x0001 (%#x)", s->selector_type);

				tap->message_selector = s;
			} else if (tap->tap_use == BIOP_OBJECT_USE) {
				/* 
				 * TODO
				 * This is not expected to be in a primary tap and at least the ATSC standard
				 * doesn't expect more than 1 tap. We need to verify what SBTVD, ISDB and DVB
				 * presume here.
				 */
				if ((buf[j] & 0xff) != 0)
					dprintf("BIOP_OBJECT_USE: selector_length != 0 (%d)", buf[j] & 0xff);
				j++;
			} else {
				dprintf("Unsupported tap_use value %#x, cannot parse selector() field.", tap->tap_use);
			}
		}
	}

	if (j != 5+cb->connbinder_length)
		dprintf("Parsed %d bytes, expected connbinder_lenght=%d+5", j, cb->connbinder_length);

	return j;
}

void biop_free_profile_body(struct iop_tagged_profile *profile)
{
	struct biop_profile_body *pb = profile->profile_body;

	/* Free connbinder */
	biop_free_connbinder(&pb->connbinder);

	free(pb);
}

int biop_parse_profile_body(struct iop_tagged_profile *profile, const char *buf, uint32_t len)
{
	struct biop_profile_body *pb = calloc(1, sizeof(struct biop_profile_body));
	int j = 0;
	
	pb->profile_id_tag = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
	pb->profile_data_length = CONVERT_TO_32(buf[j+4], buf[j+5], buf[j+6], buf[j+7]);
	pb->profile_data_byte_order = buf[j+8];
	pb->component_count = buf[j+9];
	j += 10;

	if (pb->profile_id_tag != 0x49534f06)
		dprintf("Parsing profile body but profile_id_tag=%#x", pb->profile_id_tag);

	j += biop_parse_object_location(&pb->object_location, &buf[j], len-j);
	j += biop_parse_connbinder(&pb->connbinder, &buf[j], len-j);
	
	if ((int) pb->component_count-2 > 0) {
		dprintf("component_count=%d, but LiteOptionsComponent() parser is not implemented.",
			pb->component_count);
	}
		
	profile->profile_body = pb;
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
	return 0;
}

void biop_free_module_info(struct biop_module_info *modinfo)
{
	if (modinfo->taps)
		free(modinfo->taps);
	if (modinfo->user_info)
		free(modinfo->user_info);
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

static int biop_update_file_dentry(struct dentry *root, 
	struct dentry *stepfather, struct biop_file_message *msg)
{
	struct biop_message_sub_header *sub_header = &msg->sub_header;
	struct dentry *dentry;
	ino_t inode;
	
	inode = biop_get_sub_header_inode(sub_header);
	dentry = fsutils_find_by_inode(root, inode);
	if (! dentry) {
		dentry = fsutils_find_by_inode(stepfather, inode);
		if (! dentry) {
			/* 
			 * Create dentry with no name in the hope that it will be
			 * updated by a directory message later on.
			 */
			dentry = CREATE_SIMPLE_FILE(stepfather, "", msg->message_body.content_length, inode);
		}
	}
	if (dentry->size != msg->message_body.content_length) {
		dprintf("'%s': directory object said size=%zd, file object says %d (contents=%p, inode=%#zx)",
		dentry->name, dentry->size, msg->message_body.content_length, dentry->contents, dentry->inode);
		if (! dentry->size) {
			/* 
			 * The directory message didn't specify file size or had binding->child_object_info_length=0, 
			 * so we need to set that now.
			 */
			if (dentry->contents) {
				dprintf("warning: object key repeats for more than one object!");
			} else {
				dentry->contents = malloc(msg->message_body.content_length);
				dentry->size = msg->message_body.content_length;
			}
		}
	}

	memcpy(dentry->contents, msg->message_body.contents, dentry->size);
	return 0;
}

static int biop_create_children_dentries(struct dentry *root, 
	struct dentry *stepfather, struct biop_directory_message *msg)
{
	struct biop_directory_message_body *msg_body = &msg->message_body;
	struct biop_message_sub_header *sub_header = &msg->sub_header;
	ino_t parent_inode, *priv_data;
	bool found_parent = true;
	struct dentry *parent;
	uint16_t i;

	parent_inode = biop_get_sub_header_inode(sub_header);
	parent = fsutils_find_by_inode(root, parent_inode);
	if (! parent) {
		parent = stepfather;
		found_parent = false;
	}

	for (i=0; i<msg_body->bindings_count; ++i) {
		struct biop_binding *binding = &msg_body->bindings[i];
		struct biop_name *name = &binding->name;
		struct dentry *entry = NULL, *tmp_entry;

		if (binding->name.kind_data == 0x66696c00) {
			dprintf("--> creating file '%s' of size '%zd' and inode '%#jx' and parent '%#jx' (%s)",
					name->id_byte, binding->content_size, binding->_inode, parent_inode,
					found_parent ? "found" : "not found");
			/* 
			 * First add to stepfather then move to real parent if they differ to
			 * make sure we don't end up with half file info in &stepfather->children
			 * and the other in &parent->children list.
			 */
			entry = CREATE_SIMPLE_FILE(stepfather, name->id_byte, binding->content_size, binding->_inode);
			if (found_parent) {
				tmp_entry = entry;
				entry = CREATE_SIMPLE_FILE(parent, name->id_byte, binding->content_size, binding->_inode);
				if (tmp_entry) {
					memcpy(entry->contents, tmp_entry->contents, tmp_entry->size);
					fsutils_dispose_node(tmp_entry);
				}
			}
		} else {
			dprintf("--> creating directory '%s' with inode '%#jx' and parent '%#jx' (%s)",
					name->id_byte, binding->_inode, parent_inode,
					found_parent ? "found" : "not found");

			entry = fsutils_find_by_inode(parent, binding->_inode);
			if (entry) {
				UPDATE_NAME(entry, name->id_byte);
				UPDATE_PARENT(entry, parent);
			} else {
				entry = CREATE_SIMPLE_DIRECTORY(parent, name->id_byte, binding->_inode);
			}
		}
		entry->atime = binding->_timestamp;
		entry->ctime = binding->_timestamp;
		entry->mtime = binding->_timestamp;
		if (! found_parent) {
			/* 
			 * Possibly the parent wasn't scanned yet. Save the parent inode
			 * number in the child's dentry private field to reparent it
			 * later on.
			 */
			priv_data = malloc(sizeof(ino_t));
			*priv_data = parent_inode;
			entry->priv = priv_data;
		}
	}
	return 0;
}

void biop_reparent_orphaned_dentries(struct dentry *root, struct dentry *stepfather)
{
	struct dentry *entry, *aux;
	bool has_orphaned_entries = false;

	list_for_each_entry_safe(entry, aux, &stepfather->children, list) {
		struct dentry *real_parent;
		ino_t real_parent_inode;
		
		if (! entry->priv) {
			dprintf("oops, orphaned entry '%s' (%#jx) doesn't contain private data",
				entry->name, entry->inode);
			fsutils_dispose_node(entry);
			continue;
		}

		real_parent_inode = *(ino_t *) entry->priv;
		real_parent = fsutils_find_by_inode(root, real_parent_inode);
		if (! real_parent) {
			/* It's possible that the real parent is also in the stepfather list */
			real_parent = fsutils_find_by_inode(stepfather, real_parent_inode);
		}

		if (! real_parent) {
			dprintf("'%s' is definitely orphaned for its parent '%#jx' is missing",
					entry->name, real_parent_inode);
			fsutils_dispose_node(entry);
			has_orphaned_entries = true;
		} else {
			list_move_tail(&entry->list, &real_parent->children);
			free(entry->priv);
			entry->priv = NULL;
		}
	}

	if (has_orphaned_entries) {
		dprintf("--- stepfather list ---");
		fsutils_dump_tree(stepfather);
		dprintf("--- rootfs list ---");
		fsutils_dump_tree(root);
	}
}

int biop_create_filesystem_dentries(struct dentry *parent, struct dentry *stepfather,
	const char *buf, uint32_t len)
{
	struct biop_directory_message gateway_msg;
	struct biop_message_header msg_header;
	char object_kind[4];
	int lookahead_offset, j = 0;

	memset(&gateway_msg, 0, sizeof(gateway_msg));

	while (j < len-1) {
		j += biop_parse_message_header(&msg_header, &buf[j], len-j);
		if (j >= len)
			break;

		/* Lookahead object kind */
		lookahead_offset = j + 1 + (buf[j+1] & 0xff) + 4 + 4;
		memcpy(object_kind, &buf[lookahead_offset], sizeof(object_kind));

		if (! strncmp(object_kind, "srg", 3)) {
			dprintf("----------------- gateway start ----------------");
			memcpy(&gateway_msg.header, &msg_header, sizeof(msg_header));
			j += biop_parse_directory_message(&gateway_msg, &buf[j], len-j);
			parent->inode = biop_get_sub_header_inode(&gateway_msg.sub_header);
			biop_create_children_dentries(parent, stepfather, &gateway_msg);
			biop_free_directory_message(&gateway_msg);

		} else if (! strncmp(object_kind, "dir", 3)) {
			dprintf("----------------- directory start ----------------");
			struct biop_directory_message dir_msg;
			memset(&dir_msg, 0, sizeof(dir_msg));
			memcpy(&dir_msg.header, &msg_header, sizeof(msg_header));
			j += biop_parse_directory_message(&dir_msg, &buf[j], len-j);
			biop_create_children_dentries(parent, stepfather, &dir_msg);
			biop_free_directory_message(&dir_msg);

		} else if (! strncmp(object_kind, "fil", 3)) {
			struct biop_file_message file_msg;
			memset(&file_msg, 0, sizeof(file_msg));
			memcpy(&file_msg.header, &msg_header, sizeof(msg_header));
			j += biop_parse_file_message(&file_msg, &buf[j], len-j);
			biop_update_file_dentry(parent, stepfather, &file_msg);
			biop_free_file_message(&file_msg);

		} else {
			dprintf("Parser for object kind '0x%02x%02x%02x%02x' not implemented",
				object_kind[0], object_kind[1], object_kind[2], object_kind[3]);
			break;
		}
	}

	return 0;
}
