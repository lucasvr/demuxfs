#ifndef __dsmcc_descriptors_h
#define __dsmcc_descriptors_h

struct dsmcc_descriptor {
	uint8_t tag;
	char *name;
	int (*parser)(const char *, int, struct dentry *, struct demuxfs_data *);
};

/* Function prototypes */
bool dsmcc_descriptor_is_parseable(struct dentry *dentry, uint8_t tag, int expected, int found);
struct dsmcc_descriptor *dsmcc_descriptors_init(struct demuxfs_data *priv);
void dsmcc_descriptors_destroy(struct dsmcc_descriptor *descriptor_list);
struct dsmcc_descriptor *dsmcc_descriptors_find(uint8_t tag, struct demuxfs_data *priv);
uint32_t dsmcc_descriptors_parse(const char *payload, uint8_t num_descriptors, 
		struct dentry *parent, struct demuxfs_data *priv);

/* Descriptor parsers */
int dsmcc_descriptor_0x00_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int dsmcc_descriptor_0x01_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int dsmcc_descriptor_0x02_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int dsmcc_descriptor_0x03_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int dsmcc_descriptor_0x04_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int dsmcc_descriptor_0x05_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int dsmcc_descriptor_0x06_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int dsmcc_descriptor_0x07_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int dsmcc_descriptor_0xc2_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int dsmcc_descriptor_broadcaster_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);

#endif /* __dsmcc_descriptors_h */
