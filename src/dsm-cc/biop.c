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

	return j;
}

static int biop_parse_message_sub_header(struct biop_message_sub_header *sub_header,
	const char *buf, uint32_t len)
{
	struct biop_object_key *obj_key = &sub_header->object_key;
	int j = 0;

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

//	dprintf("object_key_len = %#x (%#x %#x %#x %#x)", obj_key->object_key_length,
//			obj_key->object_key[0], obj_key->object_key[1],
//			obj_key->object_key[2], obj_key->object_key[3]);

	if (sub_header->object_kind_data == 0x66696c00) { /* "fil" */
		struct biop_file_object_info *file_info = calloc(1, sizeof(struct biop_file_object_info));
		file_info->content_size = CONVERT_TO_64(buf[j], buf[j+1], buf[j+2], buf[j+3],
			buf[j+4], buf[j+5], buf[j+6], buf[j+7]);
		j += 8;
		sub_header->object_info_length -= 8;
		sub_header->obj_info.file_object_info = file_info;
	//	dprintf("file object with content_size=%lld", file_info->content_size);
	}

	/* We don't need to parse the descriptors once again; parent did that for us already */
	j += sub_header->object_info_length;

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

	name->kind_length = buf[j];
	name->kind_data = CONVERT_TO_32(buf[j+1], buf[j+2], buf[j+3], buf[j+4]);

//	dprintf("name_component_count=%d", name->name_component_count);
//	dprintf("name_id_length=%d", name->id_length);
//	dprintf("name_id_byte=%s (%c%c%c)", name->id_byte,
//		(name->kind_data>>24) & 0xff,
//		(name->kind_data>>16) & 0xff,
//		(name->kind_data>>8) & 0xff);
//	dprintf("kind_length=%d", name->kind_length);
//	dprintf("kind_data=%#x", name->kind_data);

	return j + 5;
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
			dprintf("time_stamp='%#llx'", binding->_timestamp);
			break;
		default:
			dprintf("Unsupported descriptor tag '%#x'", descriptor_tag);
	}
	j += 2 + descriptor_length;

	return j;
}

static void biop_free_directory_message(struct biop_directory_message *msg)
{
	// TODO
}

static void biop_free_file_message(struct biop_file_message *msg)
{
	// TODO
}

/* The MessageHeader is expected to have been already parsed */
static int biop_parse_directory_message(struct biop_directory_message *msg,
	const char *buf, uint32_t len)
{
	struct biop_directory_message_body *msg_body = &msg->message_body;
	struct biop_message_sub_header *sub_header = &msg->sub_header;
	int ret, retval, x = 0, j = 0;

	j += biop_parse_message_sub_header(sub_header, &buf[j], len-j);

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
			if (binding->name.kind_data == 0x66696c00) { /* "fil" */
				binding->content_size = CONVERT_TO_64(buf[j], buf[j+1], 
					buf[j+2], buf[j+3],	buf[j+4], buf[j+5], buf[j+6], buf[j+7]);
				j += 8;
				while (x < binding->child_object_info_length-8)
					x += biop_parse_descriptor(binding, &buf[j+x], len-j-x);
			} else {
				while (x < binding->child_object_info_length)
					x += biop_parse_descriptor(binding, &buf[j+x], len-j-x);
			}
			j += x;

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
			ol->object_key = CONVERT_TO_32(buf[j], buf[j+1], buf[j+2], buf[j+3]);
			break;
		default:
			dprintf("object_key_length indicates more than 4 bytes, cannot parse object_key");
			ol->object_key_length = 4;
	}
	j += ol->object_key_length;

	return j;
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

int biop_parse_profile_body(struct iop_tagged_profile *profile, const char *buf, uint32_t len)
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
			dprintf("Could not find inode '%#llx'", inode);
			return -1;
		}
	}
	if (dentry->size != msg->message_body.content_length)
		dprintf("'%s': directory object said size=%d, file object says %d",
		dentry->name, dentry->size, msg->message_body.content_length);

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
		struct dentry *entry = NULL;

		if (binding->name.kind_data == 0x66696c00) {
			dprintf("--> creating file '%s' of size '%lld' and inode '%#llx' and parent '%#llx' (%s)", 
					name->id_byte, binding->content_size, binding->_inode, parent_inode,
					found_parent ? "found" : "not found");
			entry = CREATE_SIMPLE_FILE(parent, name->id_byte, binding->content_size);
		} else {
			dprintf("--> creating directory '%s' with inode '%#llx' and parent '%#llx' (%s)", 
					name->id_byte, binding->_inode, parent_inode,
					found_parent ? "found" : "not found");
			entry = CREATE_DIRECTORY(parent, name->id_byte);
		}
		entry->atime = binding->_timestamp;
		entry->ctime = binding->_timestamp;
		entry->mtime = binding->_timestamp;
		entry->inode = binding->_inode;
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

static void biop_reparent_orphaned_dentries(struct dentry *root,
	struct dentry *stepfather)
{
	struct dentry *entry, *aux;

	list_for_each_entry_safe(entry, aux, &stepfather->children, list) {
		struct dentry *real_parent;
		ino_t real_parent_inode;
		
		real_parent_inode = *(ino_t *) entry->priv;
		real_parent = fsutils_find_by_inode(root, real_parent_inode);
		if (! real_parent) {
			/* It's possible that the real parent is also in the stepfather list */
			real_parent = fsutils_find_by_inode(stepfather, real_parent_inode);
		}

		if (! real_parent) {
			dprintf("'%s' is definitely orphaned for its parent '%#llx' is missing", 
					entry->name, real_parent_inode);
			fsutils_dispose_node(entry);
		} else {
			list_move_tail(&entry->list, &real_parent->children);
			free(entry->priv);
			entry->priv = NULL;
		}
	}
}

int biop_create_filesystem_dentries(struct dentry *parent, const char *buf, uint32_t len)
{
	enum p_state { PARSING_GATEWAY, PARSING_DIRS, PARSING_FILES, DONE_PARSING } state;
	struct biop_directory_message gateway_msg;
	struct biop_message_header msg_header;
	struct dentry stepfather;
	char object_kind[4];
	int lookahead_offset, j = 0;

	memset(&gateway_msg, 0, sizeof(gateway_msg));
	INIT_LIST_HEAD(&stepfather.children);
	state = PARSING_GATEWAY;

	while (true) {
		if (j < len)
			j += biop_parse_message_header(&msg_header, &buf[j], len-j);
		if (j >= len) {
			/* Shift state or break if done */
			if (++state == DONE_PARSING)
				break;
			j = 0;
			continue;
		}

		/* Lookahead object kind */
		lookahead_offset = j + 1 + (buf[j+1] & 0xff) + 4 + 4;
		memcpy(object_kind, &buf[lookahead_offset], sizeof(object_kind));

		if (! strncmp(object_kind, "srg", 3)) {
			if (state == PARSING_GATEWAY) {
				dprintf("----------------- gateway ----------------");
				memcpy(&gateway_msg.header, &msg_header, sizeof(msg_header));
				j += biop_parse_directory_message(&gateway_msg, &buf[j], len-j);
				parent->inode = biop_get_sub_header_inode(&gateway_msg.sub_header);
				biop_create_children_dentries(parent, &stepfather, &gateway_msg);
				biop_free_directory_message(&gateway_msg);
			} else
				j += msg_header.message_size;

		} else if (! strncmp(object_kind, "dir", 3)) {
			if (state == PARSING_DIRS) {
				dprintf("----------------- directory ----------------");
				struct biop_directory_message dir_msg;
				memset(&dir_msg, 0, sizeof(dir_msg));
				memcpy(&dir_msg.header, &msg_header, sizeof(msg_header));
				j += biop_parse_directory_message(&dir_msg, &buf[j], len-j);
				biop_create_children_dentries(parent, &stepfather, &dir_msg);
				biop_free_directory_message(&dir_msg);
			} else
				j += msg_header.message_size;

		} else if (! strncmp(object_kind, "fil", 3)) {
			if (state == PARSING_FILES) {
				struct biop_file_message file_msg;
				memset(&file_msg, 0, sizeof(file_msg));
				memcpy(&file_msg.header, &msg_header, sizeof(msg_header));
				j += biop_parse_file_message(&file_msg, &buf[j], len-j);
				biop_update_file_dentry(parent, &stepfather, &file_msg);
				biop_free_file_message(&file_msg);
			} else
				j += msg_header.message_size;

		} else {
			dprintf("Parser for object kind '0x%02x%02x%02x%02x' not implemented", 
				object_kind[0], object_kind[1], object_kind[2], object_kind[3]);
			break;
		}
	}

	biop_reparent_orphaned_dentries(parent, &stepfather);

	return 0;
}
