#ifndef __psi_h
#define __psi_h

#define PSI_HEADER() \
	uint8_t table_id; \
	uint16_t section_syntax_indicator:1; \
	uint16_t reserved_1:1; \
	uint16_t reserved_2:2; \
	uint16_t section_length:12; \
	uint16_t identifier; \
	uint8_t reserved_3:2; \
	uint8_t version_number:5; \
	uint8_t current_next_indicator:1; \
	uint8_t section_number; \
	uint8_t last_section_number; \
	uint16_t _remaining_packets

/**
 * Common header
 */
struct psi_common_header {
	/* struct dentry always comes first */
	struct dentry dentry;
	/* common PSI header */
	PSI_HEADER();
} __attribute__((__packed__));

/* Function prototypes */
void psi_populate(void **table, struct dentry *parent);
int psi_parse(struct psi_common_header *header, const char *payload, uint8_t payload_len);
void psi_dump_header(struct psi_common_header *header);

#endif /* __psi_h */
