#ifndef __tot_h
#define __tot_h

/**
 * TOT - Time Offset Table
 */
struct tot_table {
	/* struct dentry always comes first */
	struct dentry *dentry;
	/* common PSI header */
	PSI_HEADER();
	/* TOT specific bits */
	char utc3_time[60];
	uint64_t _utc3_time;
	uint16_t reserved_4:4;
	uint16_t descriptors_loop_length:12;
	uint32_t crc;
} __attribute__((__packed__));

int tot_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);

#endif /* __tot_h */
