#ifndef __dii_h
#define __dii_h

#include "biop.h"

/**
 * DII - Download Info Indication
 */
struct dii_module {
	uint16_t module_id;
	uint32_t module_size;
	uint8_t module_version;
	uint8_t module_info_length;
	struct biop_module_info *module_info;
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
	char *private_data_bytes;
	uint32_t crc;
} __attribute__((__packed__));

int dii_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);

#endif /* __dii_h */
