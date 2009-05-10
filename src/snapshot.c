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

static int _update_frame(struct snapshot_context *ctx)
{
    int y, out_size;
	char header[128];
	char *contents, *ptr;
	int xsize = ctx->c->width;
	int ysize = ctx->c->height;
	struct SwsContext *img_convert_ctx;
	AVFrame *rgb_picture;

	/* Convert the picture from YUV420P to RGB24 */
    rgb_picture = avcodec_alloc_frame();
    avpicture_alloc((AVPicture *) rgb_picture, PIX_FMT_RGB24, ctx->c->width, ctx->c->height);

    img_convert_ctx = sws_getContext(
		xsize, ysize, ctx->c->pix_fmt,
		xsize, ysize, PIX_FMT_RGB24,
		0, NULL, NULL, NULL);

    sws_scale(img_convert_ctx, ctx->picture->data, ctx->picture->linesize, 0,
        ysize, rgb_picture->data, rgb_picture->linesize);

	/* Prepare the PPM header */
	snprintf(header, sizeof(header), "P6\n%d %d\n%d\n", xsize, ysize, 255);
	out_size = strlen(header) + (ysize * xsize * 3);
	contents = malloc(out_size);
	if (! contents) {
		perror("malloc");
		return -ENOMEM;
	}

	strcpy(contents, header);
	ptr = &contents[strlen(header)];

	int n = 0;
    for (y=0; y<ysize; ++y) {
		memcpy(ptr, rgb_picture->data[0] + y * rgb_picture->linesize[0], xsize * 3);
		ptr += xsize * 3;
		n += xsize * 3;
	}

	ctx->contents_size = out_size;
	ctx->contents = contents;
	return 0;
}

static void _save_ppm_to_file(struct snapshot_context *ctx, int seq, struct demuxfs_data *priv)
{
    int y;
	char filename[128];
	int xsize = ctx->c->width;
	int ysize = ctx->c->height;
	struct SwsContext *img_convert_ctx;
	AVFrame *rgb_picture;

	/* Convert the picture from YUV420P to RGB24 */
    rgb_picture = avcodec_alloc_frame();
    avpicture_alloc((AVPicture *) rgb_picture, PIX_FMT_RGB24, ctx->c->width, ctx->c->height);

    img_convert_ctx = sws_getContext(
		xsize, ysize, ctx->c->pix_fmt,
		xsize, ysize, PIX_FMT_RGB24,
		0, NULL, NULL, NULL);

    sws_scale(img_convert_ctx, ctx->picture->data, ctx->picture->linesize, 0,
        ysize, rgb_picture->data, rgb_picture->linesize);

	sprintf(filename, "%s/snapshot-%d.ppm", priv->options.tmpdir, seq);
	FILE *f=fopen(filename, "w");
	fprintf(f,"P6\n%d %d\n255\n", xsize, ysize);
	for(y=0; y<ysize; y++)
		fwrite(rgb_picture->data[0] + y * rgb_picture->linesize[0], 1, xsize * 3, f);
	fclose(f);
}

int snapshot_save_video_frame(const char *inbuf, size_t size, 
		struct snapshot_context *ctx, struct demuxfs_data *priv)
{
	uint8_t *inbuf_ptr = (uint8_t *) inbuf;
	int got_picture = 0, i = 0;
    int len, ret = -1;

	if (ctx) {
		while (size > 0) {
			len = avcodec_decode_video(ctx->c, ctx->picture, &got_picture, inbuf_ptr, size);
			if (len < 0)
				return len;
			if (got_picture)
				_save_ppm_to_file(ctx, i++, priv);
			size -= len;
			inbuf_ptr += len;
		}
		if (got_picture) {
			ret = _update_frame(ctx);
			_save_ppm_to_file(ctx, i++, priv);
		}
	}

	return ret;
}

void snapshot_destroy_video_context(struct snapshot_context *ctx)
{
	if (ctx && ctx->c) {
		avcodec_close(ctx->c);
		av_free(ctx->c);
		ctx->c = NULL;
	}
	if (ctx && ctx->picture) {
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
