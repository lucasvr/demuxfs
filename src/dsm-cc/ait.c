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
#include "hash.h"
#include "fifo.h"
#include "ts.h"
#include "tables/psi.h"
#include "dsm-cc/ait.h"
#include "dsm-cc/descriptors/descriptors.h"

/* AIT descriptor 0x00 */
struct application_descriptor {
	uint8_t application_profiles_length;
	struct app_profile {
		uint16_t application_profile;
		uint8_t version_major;
		uint8_t version_minor;
		uint8_t version_micro;
	} app_profile;
	uint8_t service_bound_flag:1;
	uint8_t _visibility:2;
	uint8_t reserved_future_use:5;
	uint8_t application_priority;
	char *visibility;
	char *transport_protocol_label;
};

/* AIT descriptor 0x01 */
struct application_name_descriptor {
	uint32_t iso_639_language_code;
	uint8_t application_name_length;
	char *application_name;
};

/* AIT descriptor 0x02 */
struct transport_protocol_descriptor {
	uint16_t _protocol_id;
	char protocol_id[128];
	uint8_t transport_protocol_label;
	char *selector_byte;

	struct carousel_transport_info {
		uint8_t remote_connection:1;
		uint8_t reserved_future_use:7;
		/* if remote_connection == 1 */
		uint16_t original_network_id;
		uint16_t transport_stream_id;
		uint16_t service_id;
		/* endif */
		uint8_t component_tag;
	} carousel;

	struct ip_transport_info {
		uint8_t remote_connection:1;
		uint8_t reserved_future_use:7;
		/* if remote_connection == 1 */
		uint16_t original_network_id;
		uint16_t transport_stream_id;
		uint16_t service_id;
		/* endif */
		uint8_t alignment_indicator:1;
		uint8_t reserved:7;
		/* loop: */
		uint8_t URL_length;
		char *URL_byte; // [URL_length];
		/* end loop */
	} ip;
};

/* AIT descriptor 0x03 */
struct ginga_j_application_descriptor {
	uint8_t parameter_length;
	char *parameter;
};

/* AIT descriptor 0x04 */
struct ginga_j_application_location_descriptor {
	uint8_t base_directory_length;
	char *base_directory;
	uint8_t classpath_extension_length;
	char *classpath_extension;
	char *initial_class;
};

/* AIT descriptor 0x05 */
struct external_application_authorization_descriptor {
	struct application_identifier {
		uint32_t organization_id;
		uint16_t application_id;
	} id;
	uint8_t application_priority;
};

/* AIT descriptor 0x0b */
struct application_icons_descriptor {
	uint8_t icon_locator_length;
	char *icon_locator;
	uint16_t _icon_flags;
	char icon_flags[64];
	char *reserved_future_use;
};

/* AIT descriptor 0x0c */
struct prefetch_descriptor {
	uint8_t transport_protocol_label;
	/* loop: */
	uint8_t label_length;
	char *label;
	uint8_t prefetch_priority;
	/* loop end */
};

/* AIT descriptor 0x11 */
struct ip_signalling_descriptor {
	uint32_t platform_id:24;
	uint32_t _padding:8;
};

void ait_free(struct ait_table *ait)
{
	if (ait->dentry && ait->dentry->name)
		fsutils_dispose_tree(ait->dentry);
	else if (ait->dentry)
		/* Dentry has simply been calloc'ed */
		free(ait->dentry);

	/* Free the ait table structure */
	if (ait->ait_data)
		free(ait->ait_data);
	free(ait);
}

static void ait_parse_descriptor(uint8_t tag, uint8_t len, const char *payload,
		struct dentry *parent)
{
	struct dentry *dentry;

	switch (tag) {
		case 0x00: /* Application descriptor */
			{
				uint8_t i;
				struct application_descriptor desc;

				dentry = CREATE_DIRECTORY(parent, "APPLICATION");

				desc.application_profiles_length = payload[2];
				for (i=3; i<desc.application_profiles_length+3; i+=5) {
					struct dentry *prof_dentry;
					struct app_profile *profile = &desc.app_profile;

					profile->application_profile = CONVERT_TO_16(payload[i], payload[i+1]);
					profile->version_major = payload[i+2];
					profile->version_minor = payload[i+3];
					profile->version_micro = payload[i+4];

					prof_dentry = CREATE_DIRECTORY(dentry, "APPLICATION_PROFILE_%02d", 
						profile->application_profile);

					CREATE_FILE_NUMBER(prof_dentry, profile, version_major);
					CREATE_FILE_NUMBER(prof_dentry, profile, version_minor);
					CREATE_FILE_NUMBER(prof_dentry, profile, version_micro);
				}
				desc.service_bound_flag = payload[i] >> 7;
				desc._visibility = (payload[i] >> 5) & 0x03;
				desc.application_priority = payload[i+1];

				if (desc._visibility == 0x00)
					desc.visibility = strdup("Not visible to users or other applications through API, except for logout information errors and such [0x00]");
				else if (desc._visibility == 0x01)
					desc.visibility = strdup("Not visible to users but visible to other applications through API [0x01]");
				else if (desc._visibility == 0x02)
					desc.visibility = strdup("Reserved for future use [0x02]");
				else /* if (desc._visibility == 0x03) */
					desc.visibility = strdup("Visible to users and to other applications through API [0x03]");

				CREATE_FILE_NUMBER(dentry, &desc, service_bound_flag);
				CREATE_FILE_STRING(dentry, &desc, visibility, XATTR_FORMAT_STRING_AND_NUMBER);
				CREATE_FILE_NUMBER(dentry, &desc, application_priority);

				free(desc.visibility);
			}
			break;
		case 0x01: /* Application name descriptor */
			{
				struct application_name_descriptor desc;
				struct dentry *app_dentry;
				uint8_t i = 2, app_nr = 1;

				dentry = CREATE_DIRECTORY(parent, "APPLICATION_NAME");
				while (i < len) {
					app_dentry = CREATE_DIRECTORY(dentry, "APPLICATION_NAME_%02d", app_nr++); 

					desc.iso_639_language_code = CONVERT_TO_24(payload[i],
							payload[i+1], payload[i+2]) & 0x00ffffff;
					desc.application_name_length = payload[i+3];
					desc.application_name = strndup(&payload[i+4], desc.application_name_length+1);
					desc.application_name[desc.application_name_length] = '\0';

					CREATE_FILE_NUMBER(app_dentry, &desc, iso_639_language_code);
					CREATE_FILE_NUMBER(app_dentry, &desc, application_name_length);
					CREATE_FILE_STRING(app_dentry, &desc, application_name, XATTR_FORMAT_STRING);
					
					free(desc.application_name);
					i += 4 + desc.application_name_length;
				}
			}
			break;
		case 0x02: /* Transport protocol descriptor */
			{
				struct transport_protocol_descriptor desc;
				memset(&desc, 0, sizeof(desc));

				dentry = CREATE_DIRECTORY(parent, "TRANSPORT_PROTOCOL");
				desc._protocol_id = CONVERT_TO_16(payload[2], payload[3]);
				desc.transport_protocol_label = payload[4];
				switch (desc._protocol_id) {
					case 0x0001: 
						sprintf(desc.protocol_id, "Object carousel transport protocol [%#x]", desc._protocol_id);
						break;
					case 0x0002: 
						sprintf(desc.protocol_id, "IP over multiprotocol DVB [%#x]", desc._protocol_id);
						break;
					case 0x0003: 
						sprintf(desc.protocol_id, "Reserved / Data piping [%#x]", desc._protocol_id);
						break;
					case 0x0004: 
						sprintf(desc.protocol_id, "Data carousel transport protocol [%#x]", desc._protocol_id);
						break;
					default:
						sprintf(desc.protocol_id, "Reserved for future use [%#x]", desc._protocol_id);
						break;
				}
				CREATE_FILE_STRING(dentry, &desc, protocol_id, XATTR_FORMAT_STRING_AND_NUMBER);
				CREATE_FILE_NUMBER(dentry, &desc, transport_protocol_label);
				if ((len - 3) > 0) {
					uint8_t i = 0;
					uint8_t url_number = 1;
					struct dentry *subdir;

					switch (desc._protocol_id) {
						case 0x0001:
						case 0x0004:
							subdir = CREATE_DIRECTORY(dentry, "REMOTE_CONNECTION");
							desc.carousel.remote_connection = payload[5] & 0x01;
							desc.carousel.reserved_future_use = (payload[5] >> 1) & 0x7f;
							CREATE_FILE_NUMBER(subdir, &desc.carousel, remote_connection);

							if (desc.carousel.remote_connection) {
								desc.carousel.original_network_id = CONVERT_TO_16(payload[6], payload[7]);
								desc.carousel.transport_stream_id = CONVERT_TO_16(payload[8], payload[9]);
								desc.carousel.service_id = CONVERT_TO_16(payload[10], payload[11]);
								CREATE_FILE_NUMBER(subdir, &desc.carousel, original_network_id);
								CREATE_FILE_NUMBER(subdir, &desc.carousel, transport_stream_id);
								CREATE_FILE_NUMBER(subdir, &desc.carousel, service_id);
								i = 6;
							}
							desc.carousel.component_tag = payload[6+i];
							CREATE_FILE_NUMBER(subdir, &desc.carousel, component_tag);
							break;
						case 0x0002:
							subdir = CREATE_DIRECTORY(dentry, "IP_TRANSPORT");
							desc.ip.remote_connection = payload[5] & 0x01;
							desc.ip.reserved_future_use = (payload[5] >> 1) & 0x7f;
							CREATE_FILE_NUMBER(subdir, &desc.ip, remote_connection);

							if (desc.ip.remote_connection) {
								desc.ip.original_network_id = CONVERT_TO_16(payload[6], payload[7]);
								desc.ip.transport_stream_id = CONVERT_TO_16(payload[8], payload[9]);
								desc.ip.service_id = CONVERT_TO_16(payload[10], payload[11]);
								CREATE_FILE_NUMBER(subdir, &desc.ip, original_network_id);
								CREATE_FILE_NUMBER(subdir, &desc.ip, transport_stream_id);
								CREATE_FILE_NUMBER(subdir, &desc.ip, service_id);
								i = 6;
							}
							desc.ip.alignment_indicator = payload[6+i] & 0x01;
							desc.ip.reserved = (payload[6+i] >> 1) & 0x7f;
							CREATE_FILE_NUMBER(subdir, &desc.ip, alignment_indicator);
							i += 7;
							while (i < len) {
								char url_dirname[64];
								struct dentry *url_dentry;

								sprintf(url_dirname, "URL_%02d", url_number++);
								url_dentry = CREATE_DIRECTORY(subdir, url_dirname);

								desc.ip.URL_length = payload[i++];
								CREATE_FILE_NUMBER(url_dentry, &desc.ip, URL_length);

								if (desc.ip.URL_length) {
									desc.ip.URL_byte = calloc(desc.ip.URL_length + 1, sizeof(char));
									memcpy(desc.ip.URL_byte, &payload[i], desc.ip.URL_length);
									CREATE_FILE_STRING(url_dentry, &desc.ip, URL_byte, XATTR_FORMAT_STRING);
									free(desc.ip.URL_byte);
								}
								i += desc.ip.URL_length;
							}
							break;
						default:
							desc.selector_byte = malloc((len-3) * sizeof(char));
							for (i=0; i<len-3; ++i)
								desc.selector_byte[i] = payload[5+i];
							CREATE_FILE_BIN(dentry, &desc, selector_byte, (len-3));
							free(desc.selector_byte);
					}
				}
			}
			break;
		case 0x03: /* Ginga-J application descriptor */
			{
				uint8_t param = 1;
				uint8_t i = 2;
				dentry = CREATE_DIRECTORY(parent, "GINGA-J_APPLICATION");
				while (i < len) {
					struct dentry *param_dentry;
					struct ginga_j_application_descriptor desc;

					param_dentry = CREATE_DIRECTORY(dentry, "PARAMETER_%02d", param++);
					desc.parameter_length = payload[i];
					CREATE_FILE_NUMBER(param_dentry, &desc, parameter_length);

					if (desc.parameter_length) {
						desc.parameter = strndup(&payload[i+1], desc.parameter_length+1);
						desc.parameter[desc.parameter_length] = '\0';

						CREATE_FILE_STRING(param_dentry, &desc, parameter, XATTR_FORMAT_STRING);
						free(desc.parameter);
					}
					i += 2 + desc.parameter_length;
				}
			}
			break;
		case 0x04: /* Ginga-J application location descriptor */
			{
				struct ginga_j_application_location_descriptor desc;
				uint8_t i;

				dentry = CREATE_DIRECTORY(parent, "GINGA-J_APPLICATION_LOCATION");

				desc.base_directory_length = payload[2];
				desc.base_directory = strndup(&payload[3], desc.base_directory_length+1);
				desc.base_directory[desc.base_directory_length] = '\0';

				i = 3 + desc.base_directory_length;
				desc.classpath_extension_length = payload[i];
				desc.classpath_extension = strndup(&payload[i+1], desc.classpath_extension_length+1);
				desc.classpath_extension[desc.classpath_extension_length] = '\0';

				i += 1 + desc.classpath_extension_length;
				desc.initial_class = strndup(&payload[i], len-i+2);
				desc.initial_class[len-i+2] = '\0';

				CREATE_FILE_NUMBER(dentry, &desc, base_directory_length);
				CREATE_FILE_STRING(dentry, &desc, base_directory, XATTR_FORMAT_STRING);
				CREATE_FILE_NUMBER(dentry, &desc, classpath_extension_length);
				CREATE_FILE_STRING(dentry, &desc, classpath_extension, XATTR_FORMAT_STRING);
				CREATE_FILE_STRING(dentry, &desc, initial_class, XATTR_FORMAT_STRING);

				free(desc.base_directory);
				free(desc.classpath_extension);
				free(desc.initial_class);
			}
			break;
		case 0x05: /* External application authorization descriptor */
			{
				uint8_t i = 2;
				uint8_t app = 1;
				dentry = CREATE_DIRECTORY(parent, "EXTERNAL_APPLICATION_AUTHORIZATION");
				while (i < len) {
					struct dentry *app_dentry;
					struct external_application_authorization_descriptor desc;

					desc.id.organization_id = CONVERT_TO_32(payload[i], payload[i+1], payload[i+2], payload[i+3]);
					desc.id.application_id = CONVERT_TO_16(payload[i+4], payload[i+5]);
					desc.application_priority = payload[i+6];
					i += 7;

					app_dentry = CREATE_DIRECTORY(dentry, "APPLICATION_%02d", app++);
					CREATE_FILE_NUMBER(app_dentry, &desc.id, organization_id);
					CREATE_FILE_NUMBER(app_dentry, &desc.id, application_id);
					CREATE_FILE_NUMBER(app_dentry, &desc, application_priority);
				}
			}
			break;
		case 0x06: /* Ginga-NCL application descriptor */
			{
				dentry = CREATE_DIRECTORY(parent, "GINGA-NCL_APPLICATION");
				dprintf("Parser for AIT descriptor %#x not implemented", tag);
			}
			break;
		case 0x07: /* Ginga-NCL application location descriptor */
			{
				dentry = CREATE_DIRECTORY(parent, "GINGA-NCL_APPLICATION_LOCATION");
				dprintf("Parser for AIT descriptor %#x not implemented", tag);
			}
			break;
		case 0x08 ... 0x0a: /* NCL-HTML (reserved by MHP) */
			{
				dentry = CREATE_DIRECTORY(parent, "NCL-HTML");
				dprintf("Parser for AIT descriptor %#x not implemented", tag);
			}
			break;
		case 0x0b: /* Application icons descriptor */
			{
				struct application_icons_descriptor desc;
				uint8_t i;

				dentry = CREATE_DIRECTORY(parent, "APPLICATION_ICONS");
				desc.icon_locator_length = payload[2];
				CREATE_FILE_NUMBER(dentry, &desc, icon_locator_length);

				if (desc.icon_locator_length) {
					desc.icon_locator = malloc(desc.icon_locator_length);
					memcpy(desc.icon_locator, &payload[3], desc.icon_locator_length);
					CREATE_FILE_BIN(dentry, &desc, icon_locator, desc.icon_locator_length);
				}
				i = 3 + desc.icon_locator_length;
				desc._icon_flags = CONVERT_TO_16(payload[i], payload[i+1]);
				switch (desc._icon_flags) {
					case 0x01: sprintf(desc.icon_flags, 
							  "32x32 for presentation on squared pixel screens [%#x]", desc._icon_flags);
							   break;
					case 0x02: sprintf(desc.icon_flags, 
							   "32x32 for presentation on 4:3 screens [%#x]", desc._icon_flags);
							   break;
					case 0x04: sprintf(desc.icon_flags, 
							   "24x32 for presentation on 16:9 screens [%#x]", desc._icon_flags);
							   break;
					case 0x08: sprintf(desc.icon_flags, 
							   "64x64 for presentation on squared pixel screens [%#x]", desc._icon_flags);
							   break;
					case 0x10: sprintf(desc.icon_flags, 
							   "64x64 for presentation on 4:3 screens [%#x]", desc._icon_flags);
							   break;
					case 0x20: sprintf(desc.icon_flags, 
							   "48x64 for presentation on 16:9 screens [%#x]", desc._icon_flags);
							   break;
					case 0x40: sprintf(desc.icon_flags, 
							   "128x128 for presentation on squared pixel screens [%#x]", desc._icon_flags);
							   break;
					case 0x80: sprintf(desc.icon_flags, 
							   "128x128 for presentation on 4:3 screens [%#x]", desc._icon_flags);
							   break;
					case 0x100: sprintf(desc.icon_flags,
							  "96x128 for presentation on 16:9 screens [%#x]", desc._icon_flags);
							   break;
					default:   sprintf(desc.icon_flags, 
							   "reserved for future use [%#x]", desc._icon_flags);
				}
				CREATE_FILE_STRING(dentry, &desc, icon_flags, XATTR_FORMAT_STRING_AND_NUMBER);
			}
			break;
		case 0x0c: /* Prefetch descriptor */
			{
				dentry = CREATE_DIRECTORY(parent, "PREFETCH");
				dprintf("Parser for AIT descriptor %#x not implemented", tag);
			}
			break;
		case 0x0d: /* DII location descriptor */
			{
				uint8_t i = 3;
				uint8_t label_number = 1;
				struct prefetch_descriptor desc;

				dentry = CREATE_DIRECTORY(parent, "DII_LOCATION");
				desc.transport_protocol_label = payload[2];
				CREATE_FILE_NUMBER(dentry, &desc, transport_protocol_label);

				while (i < len) {
					char label_name[32];
					struct dentry *label_dentry;

					sprintf(label_name, "LABEL_%02d", label_number++);
					label_dentry = CREATE_DIRECTORY(dentry, label_name);

					desc.label_length = payload[i++];
					CREATE_FILE_NUMBER(label_dentry, &desc, label_length);
					if (desc.label_length) {
						desc.label = calloc(desc.label_length + 1, sizeof(char));
						memcpy(desc.label, &payload[i], desc.label_length);
						CREATE_FILE_STRING(label_dentry, &desc, label, XATTR_FORMAT_STRING);
					}
					i += desc.label_length;

					desc.prefetch_priority = payload[i++];
					if (desc.prefetch_priority <= 0 || desc.prefetch_priority > 100)
						TS_WARNING("prefetch_priority not in range 1..100 (%d)", desc.prefetch_priority);
					CREATE_FILE_NUMBER(label_dentry, &desc, prefetch_priority);
				}
			}
			break;
		case 0x0e ... 0x10: /* Reserved for future use by MHP */
			{
				dprintf("Parser for AIT descriptor %#x not implemented for it's reserved for future use by MHP", tag);
			}
			break;
		case 0x11: /* IP signalling descriptor */
			{
				struct ip_signalling_descriptor desc;
				desc.platform_id = CONVERT_TO_24(payload[2], payload[3], payload[4]);
				dentry = CREATE_DIRECTORY(parent, "IP_SIGNALLING");
				CREATE_FILE_NUMBER(dentry, &desc, platform_id);
			}
			break;
		case 0x12 ... 0x5e: /* Reserved for future use by MHP */
			{
				dprintf("Parser for AIT descriptor %#x not implemented for it's reserved for future use by MHP", tag);
			}
			break;
		case 0x5f: /* Private data specifier descriptor */
			{
				dentry = CREATE_DIRECTORY(parent, "PRIVATE_DATA_SPECIFIER");
				dprintf("Parser for AIT descriptor %#x not implemented for it's reserved for future use by MHP", tag);
			}
			break;
		case 0x60 ... 0x7f: /* Reserved for future use by MHP */
			{
				dprintf("Parser for AIT descriptor %#x not implemented for it's reserved for future use by MHP", tag);
			}
			break;
		case 0x80 ... 0xfe: /* User defined */
			{
				dprintf("Parser for AIT descriptor %#x not implemented", tag);
			}
			break;
	}
}

static void ait_create_directory(const struct ts_header *header, struct ait_table *ait,
		struct dentry **version_dentry, struct demuxfs_data *priv)
{
	struct dentry *dentry = fsutils_get_child(priv->root, "AIT");

	if (! dentry) {
		/* Create a new directory named "AIT" in the root filesystem */
		ait->dentry->name = strdup(FS_AIT_NAME);
		ait->dentry->mode = S_IFDIR | 0555;
		CREATE_COMMON(priv->root, ait->dentry);
		dentry = ait->dentry;
	}

	/* Create the versioned dir and update the Current symlink */
	*version_dentry = fsutils_create_version_dir(dentry, ait->version_number);

	psi_populate((void **) &ait, *version_dentry);
}

int ait_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv)
{
	struct ait_table *current_ait = NULL;
	struct ait_table *ait = (struct ait_table *) calloc(1, sizeof(struct ait_table));
	assert(ait);
	
	ait->dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	assert(ait->dentry);

	/* Copy data up to the first loop entry */
	int ret = psi_parse((struct psi_common_header *) ait, payload, payload_len);
	if (ret < 0) {
		ait_free(ait);
		return ret;
	}

	/* Set hash key and check if there's already one version of this table in the hash */
	ait->dentry->inode = TS_PACKET_HASH_KEY(header, ait);
	current_ait = hashtable_get(priv->psi_tables, ait->dentry->inode);
	
	/* Check whether we should keep processing this packet or not */
	if (! ait->current_next_indicator || (current_ait && current_ait->version_number == ait->version_number)) {
		ait_free(ait);
		return 0;
	}

	dprintf("*** AIT parser: pid=%#x, table_id=%#x, current_ait=%p, ait->version_number=%#x, len=%d ***", 
			header->pid, ait->table_id, current_ait, ait->version_number, payload_len);

	/* Parse AIT specific bits */
	struct dentry *version_dentry = NULL;
	ait_create_directory(header, ait, &version_dentry, priv);

	ait->reserved_4 = payload[8] >> 4;
	ait->common_descriptors_length = CONVERT_TO_16(payload[8], payload[9]) & 0x0fff;
	CREATE_FILE_NUMBER(version_dentry, ait, common_descriptors_length);

	uint16_t i;
	uint8_t len = 0;
	for (i=10; i<10+ait->common_descriptors_length; i+=len)
		len = dsmcc_descriptors_parse(&payload[i], 1, version_dentry, priv);

	ait->reserved_5 = payload[i] >> 4;
	ait->application_loop_length = CONVERT_TO_16(payload[i], payload[i+1]) & 0x0fff;
	CREATE_FILE_NUMBER(version_dentry, ait, application_loop_length);
	i += 2;

	/* Count how many entries are in the application loop */
	uint16_t n = i;
	while (n < ait->application_loop_length) {
		uint16_t application_descriptors_loop_len;
		n += 7; // skip application_identifier() + application_control_code
		application_descriptors_loop_len = CONVERT_TO_16(payload[n], payload[n+1]) & 0x0fff;
		n += 2 + application_descriptors_loop_len;
		ait->_ait_data_entries++;
	}

	/* Allocate and parse application entries */
	if (ait->_ait_data_entries) {
		uint16_t ait_index = 0;
		ait->ait_data = calloc(ait->_ait_data_entries, sizeof(struct ait_data));
		while (i < ait->application_loop_length) {
			struct dentry *app_dentry;
			struct ait_data *data = &ait->ait_data[ait_index++];

			app_dentry = CREATE_DIRECTORY(version_dentry, "Application_%02d", ait_index);

			data->application_identifier.organization_id = CONVERT_TO_32(payload[i],
				payload[i+1], payload[i+2], payload[i+3]);
			data->application_identifier.application_id = CONVERT_TO_16(payload[i+4],
				payload[i+5]);
			data->application_control_code = payload[i+6];
			data->application_descriptors_loop_length = CONVERT_TO_16(payload[i+7],
				payload[i+8]) & 0x0fff;

			CREATE_FILE_NUMBER(app_dentry, &data->application_identifier, organization_id);
			CREATE_FILE_NUMBER(app_dentry, &data->application_identifier, application_id);
			CREATE_FILE_NUMBER(app_dentry, data, application_control_code);
			CREATE_FILE_NUMBER(app_dentry, data, application_descriptors_loop_length);

			i += 9;
			n = 0;
			while (n < data->application_descriptors_loop_length) {
				uint8_t descriptor_tag = payload[i+n];
				uint8_t descriptor_len = payload[i+n+1];
				ait_parse_descriptor(descriptor_tag, descriptor_len, &payload[i+n], app_dentry);
				n += 2 + descriptor_len;
			}
			i += n;
		}
	}

	if (current_ait) {
		hashtable_del(priv->psi_tables, current_ait->dentry->inode);
		fsutils_migrate_children(current_ait->dentry, ait->dentry);
		ait_free(current_ait);
	}
	hashtable_add(priv->psi_tables, ait->dentry->inode, ait, (hashtable_free_function_t) ait_free);

	return 0;
}
