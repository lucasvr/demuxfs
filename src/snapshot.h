#ifndef __snapshot_h
#define __snapshot_h

#ifdef USE_FFMPEG

#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/mathematics.h>

#define SNAPSHOT_INBUF_SIZE (256*1024)
#define SNAPSHOT_BUFFER_SIZE (SNAPSHOT_INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE)

struct snapshot_context {
    AVCodec *codec;
    AVCodecContext *c;
    AVFrame *picture;
	char *contents;
	size_t contents_size;
};

struct snapshot_context *snapshot_init_video_context();
void snapshot_destroy_video_context(struct snapshot_context *ctx);

/**
 * Given a buffer containing a PES video packet, soft-decode it and return a pointer
 * to a PGM allocated buffer which can be opened by a picture viewer application.
 * 
 * @param inbuf input buffer. Needs to be [SNAPSHOT_BUF_SIZE+FF_INPUT_BUFFER_PADDING_SIZE] 
 * bytes long.
 * @param size how many bytes are used in the input buffer.
 * @param out_size decoded buffer size in bytes.
 * @return 0 on success or a negative number on error.
 */
int snapshot_save_video_frame(const char *inbuf, size_t size, 
		struct snapshot_context *ctx, struct demuxfs_data *priv);

#else

#define SNAPSHOT_INBUF_SIZE (256*1024)
#define SNAPSHOT_BUFFER_SIZE (SNAPSHOT_INBUF_SIZE + 8)

struct snapshot_context {
	char *contents;
	size_t contents_size;
};

#define snapshot_init_video_context() ({ NULL; })
#define snapshot_destroy_video_context(ctx) do { ; } while(0)
#define snapshot_save_video_frame(i,s,ctx,priv) ({ 0; })

#endif /* USE_FFMPEG */

#endif /* __snapshot_h */
