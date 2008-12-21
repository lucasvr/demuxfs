#ifndef __sdt_h
#define __sdt_h

/**
 * SDT - Service Description Table
 */
struct sdt_service_info {
	uint16_t service_id;
	uint8_t reserved_future_use:6;
	uint8_t eit_schedule_flag:1;
	uint8_t eit_present_following_flag:1;
	uint16_t running_status:3;
	uint16_t free_ca_mode:1;
	uint16_t descriptors_loop_length:12;
	/* Descriptors array */
};

struct sdt_table {
	/* struct dentry always comes first */
	struct dentry *dentry;
	/* common PSI header */
	PSI_HEADER();
	/* SDT specific bits */
	uint16_t original_network_id;
	uint8_t reserved_future_use;
	struct sdt_service_info *_services;
	uint32_t _number_of_services;
	uint32_t crc;
} __attribute__((__packed__));

int sdt_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);

#endif /* __sdt_h */
