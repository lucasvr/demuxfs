#ifndef __dsmcc_h
#define __dsmcc_h

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

/**
 * MessageHeader - used by the DII parser
 */
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
 * DownloadDataHeader - used by the DDB parser
 */
struct dsmcc_download_data_header {
	uint8_t protocol_discriminator;
	uint8_t dsmcc_type;
	uint16_t message_id;
	uint32_t download_id;
	uint8_t reserved;
	uint8_t adaptation_length;
	uint16_t message_length;
	struct dsmcc_adaptation_header dsmcc_adaptation_header;
};

void dsmcc_create_download_data_header_dentries(struct dsmcc_download_data_header *data_header, struct dentry *parent);
void dsmcc_create_message_header_dentries(struct dsmcc_message_header *msg_header, struct dentry *parent);
void dsmcc_create_compatibility_descriptor_dentries(struct dsmcc_compatibility_descriptor *cd, struct dentry *parent);
int dsmcc_parse_message_header(struct dsmcc_message_header *msg_header, const char *payload, int index);
int dsmcc_parse_download_data_header(struct dsmcc_download_data_header *data_header, const char *payload, int index);
int dsmcc_parse_compatibility_descriptors(struct dsmcc_compatibility_descriptor *cd, const char *payload, int index);
int dsmcc_parse(const struct ts_header *header, const char *payload, uint32_t payload_len, struct demuxfs_data *priv);
void dsmcc_free_compatibility_descriptors(struct dsmcc_compatibility_descriptor *cd);
void dsmcc_free_message_header(struct dsmcc_message_header *msg_header);

#endif /* __dsmcc_h */
