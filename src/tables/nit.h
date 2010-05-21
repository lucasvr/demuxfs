#ifndef __nit_h
#define __nit_h

/** 
 * NIT - Network Information Table
 */
struct nit_ts_data {
	uint16_t transport_stream_id;
	uint16_t original_network_id;
	uint16_t reserved_future_use:4;
	uint16_t transport_descriptors_length:12;
	uint16_t num_descriptors;
	// descriptors[]
} __attribute__((__packed__));

typedef struct nit_table {
	/* struct dentry always comes first */
	struct dentry *dentry;
	/* common PSI header */
	PSI_HEADER();
	/* NIT specific bits */
	uint16_t reserved_4:4;
	uint16_t network_descriptors_length:12;
	uint16_t num_descriptors;
	// descriptors[]
	uint16_t reserved_5:4;
	uint16_t transport_stream_loop_length:12;
	struct nit_ts_data *nit_ts_data;
	uint32_t crc;
} __attribute__((__packed__)) nit_table;


int nit_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);
void nit_free(struct nit_table *nit);

#endif /* __nit_h */
