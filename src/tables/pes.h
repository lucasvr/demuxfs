#ifndef __es_h
#define __es_h

#ifdef DEBUG
#define PES_WARNING(x...) dprintf(x)
#define pes_dprintf(x...) do { ; } while(0)
#else
#define PES_WARNING(x...) do { ; } while(0)
#define pes_dprintf(x...) do { ; } while(0)
#endif

enum {
	PES_PROGRAM_STREAM_MAP,
	PES_PRIVATE_STREAM_1,
	PES_PADDING_STREAM,
	PES_PRIVATE_STREAM_2,
	PES_AUDIO_STREAM,
	PES_VIDEO_STREAM,
	PES_ECM_STREAM,
	PES_EMM_STREAM,
	PES_DSMCC_STREAM,
	PES_ISO_IEC_13522_STREAM,
	PES_H222_1_TYPE_A,
	PES_H222_1_TYPE_B,
	PES_H222_1_TYPE_C,
	PES_H222_1_TYPE_D,
	PES_H222_1_TYPE_E,
	PES_ANCILLARY_STREAM,
	PES_SL_PACKETIZED_STREAM,
	PES_FLEXMUX_STREAM,
	PES_RESERVED_DATA_STREAM,
	PES_PROGRAM_STREAM_DIRECTORY,
	PES_UNKNOWN_STREAM
};

int pes_identify_stream_id(uint8_t stream_id);
int pes_parse_audio(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);
int pes_parse_video(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);
int pes_parse_data(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);
int pes_parse_other(const struct ts_header *header, const char *payload, uint32_t payload_len,
		struct demuxfs_data *priv);

#endif /* __es_h */
