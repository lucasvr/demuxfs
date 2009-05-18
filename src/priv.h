#ifndef __priv_h
#define __priv_h

struct fifo_priv {
	struct fifo *fifo;
};

struct video_fifo_priv {
	struct fifo *fifo; /* This needs to come first */
	struct buffer *es_buffer;
	struct pes_header *pes_header;
};

struct snapshot_priv {
	struct dentry *borrowed_es_dentry;
	struct snapshot_context *snapshot_ctx;
};

#endif /* __priv_h */
