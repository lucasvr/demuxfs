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
	struct dsmcc_download_data_header dsmcc_download_data_header;
	uint16_t module_id;
	uint8_t module_version;
	uint8_t reserved;
	uint16_t block_number;
	uint16_t _block_data_size;
	char *block_data_bytes;
	uint32_t crc;
} __attribute__((__packed__));

int ddb_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);

#endif /* __ddb_h */
