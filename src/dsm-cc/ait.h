#ifndef __ait_h
#define __ait_h

/**
 * AIT - Application Information Table
 */
struct ait_application_identifier {
	uint32_t organization_id;
	uint16_t application_id;
};

struct ait_data {
	struct ait_application_identifier application_identifier;
	uint8_t application_control_code;
	uint16_t reserved_1:4;
	uint16_t application_descriptors_loop_length:12;
	// descriptors loop
};

struct ait_table {
	/* struct dentry always comes first */
	struct dentry *dentry;
	/* common PSI header */
	PSI_HEADER();
	/* AIT specific bits */
	uint16_t reserved_4:4;
	uint16_t common_descriptors_length:12;
	// descriptors loop
	uint16_t reserved_5:4;
	uint16_t application_loop_length:12;
	struct ait_data *ait_data;
	int _ait_data_entries;
	uint32_t crc;
} __attribute__((__packed__));

int ait_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);

#endif /* __ait_h */
