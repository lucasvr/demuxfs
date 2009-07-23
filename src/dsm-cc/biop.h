#ifndef __biop_h
#define __biop_h

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
	uint8_t object_key_length;
	char *object_key_data;
	uint8_t object_kind[4];
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

struct lite_profile_body {
	int not_implemented;
};

struct biop_tagged_profile {
	struct biop_profile_body *profile_body;
	struct lite_options_profile_body *lite_body;
};


int biop_parse_tagged_profiles(struct biop_tagged_profile *profile, 
		uint32_t count, const char *buf, uint32_t len);
int biop_create_tagged_profiles_dentries(struct dentry *parent,
		struct biop_tagged_profile *profile);

#endif /* __biop_h */
