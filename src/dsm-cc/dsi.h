#ifndef __dsi_h
#define __dsi_h

#include "biop.h"
#include "iop.h"

/**
 * DSI - Download Server Initiate
 */

struct dsi_group_info_indication {
	uint16_t number_of_groups;
	struct dsi_group_info {
		uint32_t group_id;
		uint32_t group_size;
		// GroupCompatibility()
		struct dsmcc_compatibility_descriptor group_compatibility;
		uint16_t group_info_length;
		char *group_info_bytes;
	} *dsi_group_info;
	uint16_t private_data_length;
	/* the private bytes actually hold DSM-CC Carousel Descriptors */
};

struct dsi_service_gateway_info {
	struct iop_ior *iop_ior;
	uint8_t download_taps_count;
	char *download_taps;
	uint8_t service_context_list_count;
	char *service_context_list;
	uint16_t user_info_length;
	char *user_info;
};
 
struct dsi_table {
	/* struct dentry always comes first */
	struct dentry *dentry;
	/* common PSI header */
	PSI_HEADER();
	/* DSI specific bits */
	struct dsmcc_message_header dsmcc_message_header;
	char server_id[20];
	struct dsmcc_compatibility_descriptor compatibility_descriptor;
	uint16_t private_data_length;
	/* the private bytes can hold different structures */
	struct dsi_group_info_indication *group_info_indication;
	struct dsi_service_gateway_info *service_gateway_info;
	bool _linked_to_dii;
	uint32_t crc;
} __attribute__((__packed__));

int dsi_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);

#endif /* __dsi_h */
