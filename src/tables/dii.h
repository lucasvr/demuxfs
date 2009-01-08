#ifndef __dii_h
#define __dii_h

struct dsmcc_sub_descriptor {
	uint8_t sub_descriptor_type;
	uint8_t sub_descriptor_length;
	uint8_t *additional_information;
};

struct dsmcc_descriptor_entry {
	uint8_t descriptor_type;
	uint8_t descriptor_length;
	uint8_t specifier_type;
	uint8_t specifier_data[3];
	uint16_t model;
	uint16_t version;
	uint8_t sub_descriptor_count;
	struct dsmcc_sub_descriptor *sub_descriptors;
};

struct dsmcc_compatibility_descriptor {
	uint16_t compatibility_descriptor_length;
	uint16_t descriptor_count;
	struct dsmcc_descriptor_entry *descriptors;
};

struct dsmcc_adaptation_header {
	uint8_t adaptation_type;
	uint8_t *adaptation_data_bytes; // adaptation_length bytes
};

struct dsmcc_message_header {
	uint8_t protocol_discriminator;
	uint8_t dsmcc_type;
	uint16_t message_id;
	uint32_t transaction_id;
	uint8_t reserved;
	uint8_t adaptation_length;
	uint16_t message_length;
	struct dsmcc_adaptation_header dsmcc_adaptation_header;
};

/**
 * DII - Download Info Indication
 */
struct dii_module {
	uint16_t module_id;
	uint32_t module_size;
	uint8_t module_version;
	uint8_t module_info_length;
	uint8_t *module_info_bytes;
};

struct dii_table {
	/* struct dentry always comes first */
	struct dentry *dentry;
	/* common PSI header */
	PSI_HEADER();
	/* DII specific bits */
	struct dsmcc_message_header dsmcc_message_header;
	uint32_t download_id;
	uint16_t block_size;
	uint8_t window_size;
	uint8_t ack_period;
	uint32_t t_c_download_window;
	uint32_t t_c_download_scenario;
	struct dsmcc_compatibility_descriptor compatibility_descriptor;
	uint16_t number_of_modules;
	struct dii_module *modules;
	uint16_t private_data_length;
	uint8_t *private_data_bytes;
	uint32_t crc;
} __attribute__((__packed__));

int dii_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);

#endif /* __dii_h */
