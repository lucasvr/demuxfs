#ifndef __sdtt_h
#define __sdtt_h

/**
 * SDTT - Software Download Trigger Table
 */
struct sdtt_schedule {
	uint64_t start_time:40;
	uint64_t duration:24;
} __attribute__((__packed__));

struct sdtt_contents {
	uint16_t group:4;
	uint16_t target_version:12;
	uint16_t new_version:12;
	uint16_t download_level:2;
	uint16_t version_indicator:2;
	uint16_t content_descriptor_length:12;
	uint16_t reserved:4;
	uint16_t schedule_descriptor_length:12;
	uint16_t schedule_time_shift_information:4;
	uint16_t _sched_entries;
	struct sdtt_schedule *sched;
	/* descriptors */
	uint32_t _descriptors_length;
	uint32_t _num_descriptors;
} __attribute__((__packed__));

struct sdtt_table {
	/* struct dentry always comes first */
	struct dentry *dentry;
	/* common PSI header */
	PSI_HEADER();
	/* SDTT specific bits */
	uint8_t  maker_id;
	uint8_t  model_id;
	uint16_t transport_stream_id;
	uint16_t original_network_id;
	uint16_t service_id;
	uint8_t num_of_contents;
	struct sdtt_contents *contents;
	uint32_t crc;
} __attribute__((__packed__));

int sdtt_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);
void sdtt_free(struct sdtt_table *sdtt);

#endif /* __sdtt_h */
