/* 
 * Copyright (c) 2008-2010, Lucas C. Villa Real <lucasvr@gobolinux.org>
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
#include "fsutils.h"
#include "fifo.h"
#include "ts.h"
#include "priv.h"

#ifdef USE_FFMPEG
int snapshot_init_video_context(struct dentry *dentry)
{
	(void) dentry;
	return 0;
}

void snapshot_destroy_video_context(struct dentry *dentry)
{
	if (dentry->contents) {
		free(dentry->contents);
		dentry->contents = NULL;
	}
	dentry->size = 0xffffff;
}

static int _snapshot_save_video_frame(struct fifo *fifo, struct dentry *dentry, struct demuxfs_data *priv)
{
	char *cmd[] = {
		"ffmpeg",
		"-i", NULL,
		"-qscale", "0",
		"-vframes", "1",
		"-f", "gif",
		"pipe:1",
		NULL
	};
	/* Hopefully this will be large enough */
	int video_frame_max_size = 1920 * 1080 * 4;
	int video_frame_size = 0;
	char *video_frame;
	int pipe_fds[2];
	pid_t pid;

	const char *path_to_fifo = fifo_get_path(fifo);
	if (! path_to_fifo) {
		dprintf("failed to get path to FIFO");
		return -ENOENT;
	}
	cmd[2] = (char *) path_to_fifo;

	if (pipe(pipe_fds) < 0) {
		perror("pipe");
		return -errno;
	}

	pid = fork();
	if (pid < 0) {
		perror("fork");
		return -errno;
	} else if (pid == 0) {
		/* stdout -> PIPE writer end */
		close(STDOUT_FILENO);
		//close(STDERR_FILENO);
		dup(pipe_fds[1]);
		close(pipe_fds[0]);
		close(pipe_fds[1]);

		execvp(cmd[0], cmd);
		exit(1); /* should never be reached */
	} else if (pid > 0) {
		/* stdin -> PIPE reader end */
		close(STDIN_FILENO);
		dup(pipe_fds[0]);
		close(pipe_fds[0]);
		close(pipe_fds[1]);

		video_frame = (char *) malloc(video_frame_max_size);
		if (! video_frame) {
			perror("malloc");
			return -ENOMEM;
		}
		TS_INFO("Reading GIF from FFMPEG pipe");
		while (true) {
			size_t max = video_frame_max_size - video_frame_size;
			ssize_t n = read(0, &video_frame[video_frame_size], max);
			if (n <= 0) {
				if (video_frame_size == 0)
					video_frame_size = n;
				break;
			}
			video_frame_size += n;
		}
		kill(pid, SIGKILL);
		waitpid(pid, NULL, 0);
		close(STDIN_FILENO);

		if (video_frame_size > 0) {
			if (dentry->contents)
				free(dentry->contents);
			dentry->contents = video_frame;
			dentry->size = video_frame_size;
		} else {
			free(video_frame);
		}
		TS_INFO("Read %d bytes from FFMPEG pipe", video_frame_size);
	}
	return video_frame_size;
}

int snapshot_save_video_frame(struct dentry *dentry, struct demuxfs_data *priv)
{
	struct snapshot_priv *priv_data = (struct snapshot_priv *) dentry->priv;
	struct fifo_priv *fifo_priv = (struct fifo_priv *) priv_data->borrowed_es_dentry->priv;
	struct fifo *fifo = fifo_priv ? fifo_priv->fifo : NULL;

	const int max_attempts = 2;
	int i, ret = -1;

	if (fifo) {
		for (i=0; i<max_attempts; ++i) {
			ret = _snapshot_save_video_frame(fifo, dentry, priv);
			if (ret > 0)
				break;
			ret = -1;
			sleep(1);
		}
	}
	return ret;
}

#endif /* USE_FFMPEG */

#if 0
static int _snapshot_save_to_dentry(struct dentry *dentry)
{
	struct snapshot_priv *priv_data = (struct snapshot_priv *) dentry->priv;
	struct snapshot_context *ctx = priv_data->snapshot_ctx;
	char header[128];
	char *contents, *ptr;
	int y, out_size, ret;
	int xsize = ctx->c->width;
	int ysize = ctx->c->height;
	struct SwsContext *img_convert_ctx;
	AVFrame *pic;

	/* Convert the picture from YUV420P to RGB24 */
	pic = av_frame_alloc();
	if (! pic) {
		fprintf(stderr, "av_frame_alloc error\n");
		return -ENOMEM;
	}
	ret = av_image_alloc(pic->data, pic->linesize, ctx->c->width, ctx->c->height,
		AV_PIX_FMT_RGB24, 1);
	if (ret < 0) {
		fprintf(stderr, "av_image_alloc: failed with error %d\n", ret);
		return ret;
	}

	img_convert_ctx = sws_getCachedContext(
		NULL, /* no context to reuse */
		xsize, ysize, ctx->c->pix_fmt,
		xsize, ysize, AV_PIX_FMT_RGB24,
		SWS_FAST_BILINEAR, NULL, NULL, NULL);
	if (! img_convert_ctx) {
		fprintf(stderr, "img_convert_ctx: failed to get context\n");
		return -1;
	}

	ret = sws_scale(
		img_convert_ctx,
		(const uint8_t * const *) ctx->picture->data,
		ctx->picture->linesize,
		0, ysize,
		pic->data,
		pic->linesize);
	if (ret < 0) {
		fprintf(stderr, "sws_scale: failed with error %d\n", ret);
		return ret;
	}

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
		memcpy(ptr, pic->data[0] + y * pic->linesize[0], xsize * 3);
		ptr += xsize * 3;
		n += xsize * 3;
	}

	/* Update dentry */
	if (dentry->contents)
		free(dentry->contents);
	dentry->contents = contents;
	dentry->size = out_size;

	return 0;
}

int snapshot_save_video_frame(struct dentry *dentry, struct demuxfs_data *priv)
{
	struct snapshot_priv *priv_data = (struct snapshot_priv *) dentry->priv;
	struct snapshot_context *ctx = priv_data->snapshot_ctx;
	int got_picture = 0;
	int ret = -1;

	if (ctx) {
		while (av_read_frame(ctx->format_context, &ctx->packet) >= 0) {
			avcodec_decode_video2(ctx->c, ctx->picture, &got_picture, &ctx->packet);
			if (got_picture) {
				/* Store the decoded picture in the dentry memory */
				ret = _snapshot_save_to_dentry(dentry);
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
			avformat_close_input(&ctx->format_context);
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
	int i, ret, video_stream = -1;
	bool ffmpeg_initialized = false;
	struct snapshot_priv *priv_data = (struct snapshot_priv *) dentry->priv;
	struct snapshot_context *ctx = calloc(1, sizeof(struct snapshot_context));
	if (! ctx) {
		perror("calloc");
		return -ENOMEM;
	}
	
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

	struct fifo_priv *fifo_priv = (struct fifo_priv *) priv_data->borrowed_es_dentry->priv;
	struct fifo *fifo = fifo_priv ? fifo_priv->fifo : NULL;
	const char *path_to_fifo = fifo_get_path(fifo);
	if (! path_to_fifo)
		goto out_free;

	ctx->format_context = NULL;
	ret = avformat_open_input(&ctx->format_context, path_to_fifo, ctx->input_format, NULL);
	if (ret != 0) {
	    dprintf("H.264 input stream not found");
		goto out_free;
	}

	ret = avformat_find_stream_info(ctx->format_context, NULL);
	if (ret < 0) {
		dprintf("Could not find H.264 codec parameters");
		goto out_free;
	}

	for (i=0; i < ctx->format_context->nb_streams; i++)
		if (ctx->format_context->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
			TS_INFO("%s: video_stream found at index %d\n", __func__, i);
			video_stream = i;
			break;
		}
	if (video_stream == -1) {
		dprintf("Video stream not found");
		goto out_free;
	}

	ctx->c = ctx->format_context->streams[video_stream]->codec;
	ctx->codec = avcodec_find_decoder(ctx->c->codec_id);
	if (! ctx->codec) {
		dprintf("Could not find H.264 codec");
		goto out_free;
	}

	ret = avcodec_open2(ctx->c, ctx->codec, NULL);
	if (ret < 0) {
		dprintf("Could not open H.264 codec");
		goto out_free;
	}

	ctx->picture = av_frame_alloc();

	return 0;

out_free:
	/* Free and detach the ctx from the dentry private data */
	snapshot_destroy_video_context(dentry);
	return -ENXIO;
}
#endif /* 0 */
