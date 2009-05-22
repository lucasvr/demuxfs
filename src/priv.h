#ifndef __priv_h
#define __priv_h

struct fifo_priv {
	struct fifo *fifo;
};

struct video_fifo_priv {
	struct fifo *fifo; /* This needs to come first */
	bool pes_packet_initialized;
	uint32_t pes_packet_length;
	uint32_t pes_packet_parsed_length;
};

struct snapshot_priv {
	char *path_to_es;
	struct dentry *borrowed_es_dentry;
	struct snapshot_context *snapshot_ctx;
};

#endif /* __priv_h */
