#ifndef __ddb_h
#define __ddb_h

/**
 * DDB - Download Data Block
 */
struct ddb_table {
	/* struct dentry always comes first */
	struct dentry *dentry;
	/* common PSI header */
	PSI_HEADER();
	/* DDB specific bits */
	uint16_t original_network_id;
	uint8_t reserved_future_use;
	struct ddb_service_info *_services;
	uint32_t _number_of_services;
	uint32_t crc;
} __attribute__((__packed__));

int ddb_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);

#endif /* __ddb_h */
