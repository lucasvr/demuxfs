#ifndef __descriptors_h
#define __descriptors_h

struct descriptor {
	uint8_t tag;
	char *name;
	int (*parser)(const char *, int, struct dentry *, struct demuxfs_data *);
};

/* Function prototypes */
bool descriptor_is_parseable(struct dentry *dentry, uint8_t tag, int expected, int found);
struct descriptor *descriptors_init(struct demuxfs_data *priv);
void descriptors_destroy(struct descriptor *descriptor_list);
struct descriptor *descriptors_find(uint8_t tag, struct demuxfs_data *priv);
int descriptors_count(const char *payload, uint16_t program_information_length);
uint8_t descriptors_parse(const char *payload, uint8_t num_descriptors, 
		struct dentry *parent, struct demuxfs_data *priv);

/* Descriptor parsers */
int descriptor_0x02_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x03_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x04_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x05_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x06_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x07_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x08_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x09_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x0a_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x0b_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x0c_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x0d_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x0e_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x0f_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x10_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x11_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x12_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x13_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x14_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x15_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x1b_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x1c_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x1d_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x1e_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x1f_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x20_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x21_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x22_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x23_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x28_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x2a_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x40_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x41_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x42_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x47_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x48_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x49_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x4a_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x4b_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x4c_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x4d_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x4e_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x4f_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x50_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x51_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x52_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x53_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x54_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x55_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x58_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x63_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0x7c_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xc0_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xc1_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xc2_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xc3_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xc4_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xc5_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xc6_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xc7_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xc8_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xc9_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xca_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xcb_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xcc_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xcd_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xce_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xcf_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xd0_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xd1_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xd2_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xd3_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xd4_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xd5_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xd6_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xd7_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xd8_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xd9_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xda_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xdb_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xdc_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xdd_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xde_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xe0_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xf7_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xf8_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xfa_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xfb_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xfc_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xfd_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xfe_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);
int descriptor_0xff_parser(const char *payload, int len, struct dentry *parent, struct demuxfs_data *priv);

#endif /* __descriptors_h */
