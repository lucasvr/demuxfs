#ifndef __pat_h
#define __pat_h

/** 
 * PAT - Program Allocation Table
 */
struct pat_program {
	uint16_t program_number;
	uint16_t reserved:3;
	uint16_t pid:13;
} __attribute__((__packed__));

struct pat_table {
	/* struct dentry always comes first */
	struct dentry *dentry;
	/* common PSI header */
	PSI_HEADER();
	/* PAT specific bits */
	struct pat_program *programs;
	uint16_t num_programs;
	uint32_t crc;
} __attribute__((__packed__));

bool pat_announces_service(uint16_t service_id, struct demuxfs_data *priv);
int pat_parse(const struct ts_header *header, const char *payload, uint32_t payload_len, 
		struct demuxfs_data *priv);
void pat_free(struct pat_table *pat);

#endif /* __pat_h */
