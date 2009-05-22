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

static int _update_dentry_contents(struct dentry *dentry)
{
	struct snapshot_priv *priv_data = (struct snapshot_priv *) dentry->priv;
	struct snapshot_context *ctx = priv_data->snapshot_ctx;
	char header[128];
	char *contents, *ptr;
    int y, out_size;
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
		SWS_BICUBIC, NULL, NULL, NULL);

    sws_scale(img_convert_ctx, ctx->picture->data, ctx->picture->linesize, 0,
        ysize, rgb_picture->data, rgb_picture->linesize);

	/* Prepare the PPM header */
	snprintf(header, sizeof(header), "P6\n%d %d\n%d\n", xsize, ysize, 255);
	out_size = strlen(header) + (ysize * xsize * 3) + 1;
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

	/* Update dentry */
	if (dentry->contents)
		free(dentry->contents);
	dentry->contents = contents;
	dentry->size = out_size;

	dprintf("contents size=%d (strlen=%d)", dentry->size, strlen(contents));
	return 0;
}

static void _save_ppm_to_file(int seq, struct dentry *dentry, struct demuxfs_data *priv)
{
	struct snapshot_priv *priv_data = (struct snapshot_priv *) dentry->priv;
	struct snapshot_context *ctx = priv_data->snapshot_ctx;
	char filename[128];
	int xsize = ctx->c->width;
	int ysize = ctx->c->height;
    int y;
	struct SwsContext *img_convert_ctx;
	AVFrame *rgb_picture;

	/* Convert the picture from YUV420P to RGB24 */
    rgb_picture = avcodec_alloc_frame();
    avpicture_alloc((AVPicture *) rgb_picture, PIX_FMT_RGB24, ctx->c->width, ctx->c->height);

    img_convert_ctx = sws_getContext(
		xsize, ysize, ctx->c->pix_fmt,
		xsize, ysize, PIX_FMT_RGB24,
		SWS_BICUBIC, NULL, NULL, NULL);

    sws_scale(img_convert_ctx, ctx->picture->data, ctx->picture->linesize, 0,
        ysize, rgb_picture->data, rgb_picture->linesize);

	sprintf(filename, "%s/snapshot-%d.ppm", priv->options.tmpdir, seq);
	FILE *f=fopen(filename, "w");
	fprintf(f,"P6\n%d %d\n255\n", xsize, ysize);
	for(y=0; y<ysize; y++)
		fwrite(rgb_picture->data[0] + y * rgb_picture->linesize[0], 1, xsize * 3, f);
	fclose(f);
}

int snapshot_save_video_frame(struct dentry *dentry, struct demuxfs_data *priv)
{
	struct snapshot_priv *priv_data = (struct snapshot_priv *) dentry->priv;
	struct snapshot_context *ctx = priv_data->snapshot_ctx;
	int got_picture = 0, i = 0;
    int ret = -1;

	if (ctx) {
		while (av_read_frame(ctx->format_context, &ctx->packet) >= 0) {
			avcodec_decode_video(ctx->c, ctx->picture, &got_picture, 
					ctx->packet.data, ctx->packet.size);
			if (got_picture && i++ > 10) {
				/* Save a debug PPM to disk */
				_save_ppm_to_file(i, dentry, priv);
				/* Save the PPM to the dentry memory */
				ret = _update_dentry_contents(dentry);
				break;
			}
		}
	} else
		dprintf("invalid snapshot context");

	return ret;
}

void snapshot_destroy_video_context(struct dentry *dentry)
{
	struct snapshot_priv *priv_data = (struct snapshot_priv *) dentry->priv;
	struct snapshot_context *ctx = priv_data->snapshot_ctx;
	if (ctx) {
		if (ctx->format_context)
			av_close_input_file(ctx->format_context);
		if (ctx->c) {
			avcodec_close(ctx->c);
			ctx->c = NULL;
		}
		if (ctx->picture) {
			av_free(ctx->picture);
			ctx->picture = NULL;
		}
		free(ctx);
	}
	priv_data->snapshot_ctx = NULL;
	if (dentry->contents) {
		free(dentry->contents);
		dentry->contents = NULL;
	}
	dentry->size = 0;
}

int snapshot_init_video_context(struct dentry *dentry)
{
	int i, ret;
	struct snapshot_priv *priv_data = (struct snapshot_priv *) dentry->priv;
	struct snapshot_context *ctx = calloc(1, sizeof(struct snapshot_context));
	if (! ctx) {
		perror("calloc");
		return -ENOMEM;
	}
	
	static bool ffmpeg_initialized = false;
	if (! ffmpeg_initialized) {
		av_register_all();
		ffmpeg_initialized = true;
	}

	/* Assign this context to the dentry private data */
	priv_data->snapshot_ctx = ctx;

	ctx->input_format = av_find_input_format("h264");
	if (! ctx->input_format) {
		dprintf("Could not find H.264 input format");
		goto out_free;
	}

	char *path_to_fifo = ((struct snapshot_priv *) dentry->priv)->path_to_es;
	ret = av_open_input_file(&ctx->format_context, path_to_fifo, ctx->input_format, 0, NULL);
	if (ret != 0) {
        dprintf("H.264 input stream not found");
		goto out_free;
	}

	ret = av_find_stream_info(ctx->format_context);
	if (ret < 0) {
		dprintf("Could not find H.264 codec parameters");
		goto out_free;
	}

	ctx->video_stream = -1;
	for (i=0; i < ctx->format_context->nb_streams; i++)
		if (ctx->format_context->streams[i]->codec->codec_type==CODEC_TYPE_VIDEO) {
			ctx->video_stream = i;
			break;
		}
	if (ctx->video_stream == -1) {
		dprintf("Video stream not found");
		goto out_free;
	}

	ctx->c = ctx->format_context->streams[ctx->video_stream]->codec;
    ctx->codec = avcodec_find_decoder(ctx->c->codec_id);
    if (! ctx->codec) {
		dprintf("Could not find H.264 codec");
		goto out_free;
    }

	ret = avcodec_open(ctx->c, ctx->codec);
	if (ret < 0) {
		dprintf("Could not open H.264 codec");
		goto out_free;
	}

    ctx->picture = avcodec_alloc_frame();

	return 0;

out_free:
	/* Free and detach the ctx from the dentry private data */
	snapshot_destroy_video_context(dentry);
	return -ENXIO;
}

#endif /* USE_FFMPEG */
