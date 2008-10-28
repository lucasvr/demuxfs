#ifndef __nit_h
#define __nit_h

int nit_parse(const struct ts_header *header, const char *payload, uint8_t payload_len,
		struct demuxfs_data *priv);

#endif /* __nit_h */
