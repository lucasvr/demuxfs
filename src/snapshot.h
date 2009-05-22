#ifndef __snapshot_h
#define __snapshot_h

#ifdef USE_FFMPEG

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/mathematics.h>

#define SNAPSHOT_INBUF_SIZE (1*1024*1024)
#define SNAPSHOT_BUFFER_SIZE (SNAPSHOT_INBUF_SIZE)

struct snapshot_context {
    AVCodec *codec;
    AVCodecContext *c;
	AVInputFormat *input_format;
	AVFormatContext *format_context;
	AVPacket packet;
    AVFrame *picture;
	int video_stream;
};

/**
 * Initialize the FFmpeg video context. 
 * @param dentry dentry to attach the context to.
 * @return 0 on success or a negative number on error.
 */
int snapshot_init_video_context(struct dentry *dentry);

/**
 * Destroy the FFmpeg video context. 
 * @param dentry dentry to attach the context to.
 */
void snapshot_destroy_video_context(struct dentry *dentry);

/**
 * Given a dentry containing an initialized PES video context, soft-decode its contents
 * and return a pointer to a PGM allocated buffer which can be opened by an image viewer 
 * application.
 * 
 * @param dentry dentry in which decoded contents will be saved.
 * @param priv demuxfs private data.
 * @return 0 on success or a negative number on error.
 */
int snapshot_save_video_frame(struct dentry *dentry, struct demuxfs_data *priv);

#else

#define SNAPSHOT_INBUF_SIZE (256*1024)
#define SNAPSHOT_BUFFER_SIZE (SNAPSHOT_INBUF_SIZE + 8)

struct snapshot_context {
	int dummy;
};

#define snapshot_init_video_context(d) ({ NULL; })
#define snapshot_destroy_video_context(d) do { ; } while(0)
#define snapshot_save_video_frame(i,s,d,priv) ({ 0; })

#endif /* USE_FFMPEG */

#endif /* __snapshot_h */
