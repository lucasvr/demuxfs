/* 
 * Copyright (c) 2008, Lucas C. Villa Real <lucasvr@gobolinux.org>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of GoboLinux nor the names of its contributors may
 * be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "demuxfs.h"
#include "snapshot.h"

#ifdef USE_FFMPEG

int save_pgm(const unsigned char *buf, int wrap, struct snapshot_context *ctx)
{
	char header[128];
	char *contents, *ptr;
	int xsize = ctx->c->width;
	int ysize = ctx->c->height;
    int i, out_size;

	snprintf(header, sizeof(header), "P5\n%d %d\n%d\n", xsize, ysize, 255);
	out_size = strlen(header) + (ysize * xsize);
	contents = malloc(out_size);
	if (! contents) {
		perror("malloc");
		return -ENOMEM;
	}

	strcpy(contents, header);
	ptr = &contents[strlen(header)];

    for (i=0; i<ysize; ++i) {
		memcpy(ptr, buf + i * wrap, xsize);
		ptr += xsize;
	}
	ctx->contents_size = out_size;
	ctx->contents = contents;
	return 0;
}

int snapshot_save_video_frame(const char *inbuf, size_t size, 
		struct snapshot_context *ctx)
{
	uint8_t *inbuf_ptr = (uint8_t *) inbuf;
	int got_picture = 0;
    int len, ret = -1;

	while (size > 0) {
		len = avcodec_decode_video(ctx->c, ctx->picture, &got_picture, inbuf_ptr, size);
		if (len < 0)
			return len;
		if (got_picture)
			break;
		size -= len;
		inbuf_ptr += len;
    }

    /* some codecs, such as MPEG, transmit the I and P frame with a
       latency of one frame. You must do the following to have a
       chance to get the last frame of the video */
    len = avcodec_decode_video(ctx->c, ctx->picture, &got_picture, NULL, 0);
    if (got_picture)
        ret = save_pgm(ctx->picture->data[0], ctx->picture->linesize[0], ctx);

	return ret;
}

void snapshot_destroy_video_context(struct snapshot_context *ctx)
{
	if (ctx->c) {
		avcodec_close(ctx->c);
		av_free(ctx->c);
		ctx->c = NULL;
	}
	if (ctx->picture) {
		av_free(ctx->picture);
		ctx->picture = NULL;
	}
}

struct snapshot_context *snapshot_init_video_context()
{
	struct snapshot_context *ctx = malloc(sizeof(struct snapshot_context));
	if (! ctx)
		return NULL;

    /* find the h.264 video decoder */
    ctx->codec = avcodec_find_decoder(CODEC_ID_H264);
    if (! ctx->codec) {
        dprintf("H.264 codec not found");
		free(ctx);
		return NULL;
    }

    ctx->c = avcodec_alloc_context();
    ctx->picture = avcodec_alloc_frame();

    if (ctx->codec->capabilities & CODEC_CAP_TRUNCATED)
        ctx->c->flags |= CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

    /* open it */
    if (avcodec_open(ctx->c, ctx->codec) < 0) {
        dprintf("could not open H.264 codec");
		av_free(ctx->picture);
		av_free(ctx->c);
		free(ctx);
		return NULL;
    }
	return ctx;
}

#endif /* USE_FFMPEG */
