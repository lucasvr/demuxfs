#ifndef __eit_h
#define __eit_h

/** 
 * EIT - Event Information Table
 */
typedef struct eit_table {
	/* struct dentry always comes first */
	struct dentry *dentry;
	/* common PSI header */
	PSI_HEADER();
	/* EIT specific bits */
	uint32_t crc;
} __attribute__((__packed__)) eit_table;


int eit_parse(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);
void eit_free(struct eit_table *eit);

#endif /* __eit_h */
