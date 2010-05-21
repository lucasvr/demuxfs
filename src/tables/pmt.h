#ifndef __pmt_h
#define __pmt_h

/**
 * PMT - Program Map Table
 */
struct pmt_program {
	uint8_t stream_type;
	uint16_t reserved_1:3;
	uint16_t elementary_pid:13;
	uint16_t reserved_2:4;
	uint16_t es_info_length:12;
	/* descriptors */
} __attribute__((__packed__));

struct pmt_stream {
	uint8_t stream_type_identifier;
	uint16_t reserved_1:3;
	uint16_t elementary_stream_pid:13;
	uint16_t reserved_2:4;
	uint16_t es_information_length:12;
	/* descriptors */
} __attribute__((__packed__));

struct pmt_table {
	/* struct dentry always comes first */
	struct dentry *dentry;
	/* common PSI header */
	PSI_HEADER();
	/* PMT specific bits */
	uint16_t reserved_4:3;
	uint16_t pcr_pid:13;
	uint16_t reserved_5:4;
	uint16_t program_information_length:12;
	uint16_t num_descriptors;
	uint16_t num_programs;
	struct pmt_program *programs;
	uint32_t crc;
} __attribute__((__packed__));

int pmt_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);
void pmt_free(struct pmt_table *pmt);

#endif /* __pmt_h */
