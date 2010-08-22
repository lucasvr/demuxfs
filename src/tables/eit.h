#ifndef __eit_h
#define __eit_h

struct eit_event {
	struct eit_event *next;
	uint16_t event_id;
	uint64_t start_time:40;
	uint64_t duration:24;
	uint16_t running_status:3;
	uint16_t free_ca_mode:1;
	uint16_t descriptors_loop_length:12;
	/* descriptors loop */
};

/** 
 * EIT - Event Information Table
 */
typedef struct eit_table {
	/* struct dentry always comes first */
	struct dentry *dentry;
	/* common PSI header */
	PSI_HEADER();
	/* EIT specific bits */
	uint16_t transport_stream_id;
	uint16_t original_network_id;
	uint8_t segment_last_section_number;
	uint8_t last_table_id;
	struct eit_event *eit_event;
	uint32_t crc;
} __attribute__((__packed__)) eit_table;


int eit_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);
void eit_free(struct eit_table *eit);

#endif /* __eit_h */
