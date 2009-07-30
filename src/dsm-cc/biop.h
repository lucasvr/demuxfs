#ifndef __biop_h
#define __biop_h

struct iop_ior;
struct iop_tagged_profile;

/*
 * A complete description of DSM-CC is found in the following URL:
 * http://www.interactivetvweb.org/tutorials/dtv_intro/dsmcc/object_carousel
 */

#define BIOP_FILE_MESSAGE         0x66696C00 /* "fil" */
#define BIOP_DIR_MESSAGE          0x64697200 /* "dir" */
#define BIOP_STREAM_MESSAGE       0x73747200 /* "str" */
#define BIOP_STREAM_EVENT_MESSAGE 0x73746500 /* "ste" */

#define BIOP_DELIVERY_PARA_USE    0x0016
#define BIOP_OBJECT_USE           0x0017
#define BIOP_ES_USE               0x0018
#define BIOP_PROGRAM_USE          0x0019

struct biop_message_header {
	uint32_t magic; /* "BIOP" */
	uint8_t biop_version_major;
	uint8_t biop_version_minor;
	uint8_t byte_order;
	uint8_t message_type;
	uint32_t message_size;
};

struct biop_file_object_info {
	uint64_t content_size;
	char *file_object_info_descriptor;
};

struct biop_message_sub_header {
	struct biop_object_key {
		uint8_t object_key_length;
		char *object_key;
	} object_key;

	uint32_t object_kind_length;
	uint32_t object_kind_data;
	uint16_t object_info_length;
	union {
		char *dir_object_info_descriptor;
		struct biop_file_object_info *file_object_info;
	} obj_info;

	uint8_t service_context_list_count;
	struct biop_service_context {
		uint32_t context_id;
		uint16_t context_data_length;
		char *context_data;
	} *service_context;
};

struct biop_name {
	uint8_t name_component_count;
	uint8_t id_length;
	char *id_byte;
	uint8_t kind_length;
	uint32_t kind_data;
};

struct biop_binding {
	struct biop_name name;
	uint8_t binding_type;
	struct iop_ior *iop_ior;
	uint16_t child_object_info_length;
	uint64_t content_size;	/* Only used if name.kind_data == 0x66696c00 */
	char *_content_type;    /* Content (MIME) type */
	uint64_t _timestamp;    /* Last modified time (UTC time) */
	ino_t _inode;			/* Inode number (DemuxFS extension) */
};

struct biop_directory_message {
	struct biop_message_header header;
	struct biop_message_sub_header sub_header;
	uint32_t message_body_length;
	struct biop_directory_message_body {
		uint16_t bindings_count;
		struct biop_binding *bindings;
	} message_body;
};

struct biop_file_message {
	struct biop_message_header header;
	struct biop_message_sub_header sub_header;
	uint32_t message_body_length;
	struct biop_file_message_body {
		uint32_t content_length;
		const char *contents;
	} message_body;
};

struct biop_module_info {
	uint32_t module_timeout;
	uint32_t block_timeout;
	uint32_t min_block_time;
	uint8_t taps_count;
	struct biop_module_tap {
		uint16_t tap_id;
		uint16_t tap_use;
		uint16_t association_tag;
		uint8_t selector_length;
	} *taps;
	uint8_t user_info_length;
	char *user_info;
};

struct biop_object_location {
	uint32_t object_location_tag;
	uint8_t object_location_length;
	uint32_t carousel_id;
	uint16_t module_id;
	uint8_t version_major;
	uint8_t version_minor;
	uint8_t object_key_length;
	uint32_t object_key;
};

struct biop_connbinder {
	uint32_t connbinder_tag;
	uint8_t connbinder_length;
	uint8_t tap_count;
	struct dsmcc_tap {
		uint16_t tap_id;
		uint16_t tap_use;
		uint16_t association_tag;
		struct message_selector {
			uint8_t selector_length;
			uint16_t selector_type;
			uint32_t transaction_id;
			uint32_t timeout;
		} *message_selector;
	} *taps;
};

struct biop_profile_body {
	uint32_t profile_id_tag;
	uint32_t profile_data_length;
	uint8_t profile_data_byte_order;
	uint8_t component_count;
	struct biop_object_location object_location;
	struct biop_connbinder connbinder;
};

int biop_create_filesystem_dentries(struct dentry *parent,
		struct dentry *stepfather,
		const char *buf, uint32_t len);
void biop_reparent_orphaned_dentries(struct dentry *root, 
		struct dentry *stepfather);

int biop_parse_module_info(struct biop_module_info *modinfo,
		const char *buf, uint32_t len);
int biop_create_module_info_dentries(struct dentry *parent,
		struct biop_module_info *modinfo);

int biop_parse_connbinder(struct biop_connbinder *cb, const char *buf, 
		uint32_t len);
int biop_parse_profile_body(struct iop_tagged_profile *profile, 
		const char *buf, uint32_t len);

#endif /* __biop_h */
