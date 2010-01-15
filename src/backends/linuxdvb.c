/* 
 * Copyright (c) 2008,2009 Lucas C. Villa Real <lucasvr@gobolinux.org>
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
#include "linuxdvb.h"
#include "fsutils.h"
#include "byteops.h"
#include "backend.h"
#include "buffer.h"
#include "ts.h"

#define LINUXDVB_DEFAULT_DEMUX_DEVICE "/dev/dvb/adapter0/demux0"
#define LINUXDVB_DEFAULT_DVR_DEVICE   "/dev/dvb/adapter0/dvr0"

struct input_parser {
	char *demux_device;
	char *dvr_device;
	int demux_fd;
	int dvr_fd;
	char *packet;
	bool packet_valid;
	uint8_t packet_size;
};

/**
 * Command line parsing routines.
 */
void linuxdvb_usage(void)
{
	fprintf(stderr, "\nLINUXDVB options:\n"
			"    -o demux_device=FILE   demux device (default=%s)\n"
			"    -o dvr_device=FILE     DVR device (default=%s)\n",
			LINUXDVB_DEFAULT_DEMUX_DEVICE,
			LINUXDVB_DEFAULT_DVR_DEVICE);
}

#define LINUXDVB_OPT(templ,offset,value) { templ, offsetof(struct input_parser, offset), value }

static struct fuse_opt linuxdvb_opts[] = {
	LINUXDVB_OPT("demux_device=%s", demux_device, 0),
	LINUXDVB_OPT("demux_device=%s", demux_device, 0),
	FUSE_OPT_END
};

static int linuxdvb_parse_opts(void *priv, const char *arg, int key, struct fuse_args *outargs)
{
	return 1;
}

/**
 * linuxdvb_create_parser: backend's create() method.
 */
int linuxdvb_create_parser(struct fuse_args *args, struct demuxfs_data *priv)
{
	struct input_parser *p = calloc(1, sizeof(struct input_parser));
	assert(p);

	int ret = fuse_opt_parse(args, p, linuxdvb_opts, linuxdvb_parse_opts);
	if (ret < 0) {
		free(p);
		return -1;
	}

	if (! p->demux_device)
		p->demux_device = LINUXDVB_DEFAULT_DEMUX_DEVICE;

	p->demux_fd = open(p->demux_device, O_RDWR|O_NONBLOCK);
	if (p->demux_fd < 0) {
		perror(p->demux_device);
		free(p);
		return -1;
	}

	if (! p->dvr_device)
		p->dvr_device = LINUXDVB_DEFAULT_DVR_DEVICE;

	p->dvr_fd = open(p->dvr_device, O_RDONLY);
	if (p->dvr_fd < 0) {
		perror(p->dvr_device);
		close(p->demux_fd);
		free(p);
		return -1;
	}

	struct dmx_pes_filter_params pes_filter;
	pes_filter.pid      = 0x2000;
	pes_filter.input    = DMX_IN_FRONTEND;
	pes_filter.output   = DMX_OUT_TS_TAP;
	pes_filter.pes_type = DMX_PES_OTHER;
	pes_filter.flags    = DMX_IMMEDIATE_START;

	ret = ioctl(p->demux_fd, DMX_SET_PES_FILTER, &pes_filter);
	if (ret < 0) {
		perror("DMX_SET_PES_FILTER");
		close(p->demux_fd);
		close(p->dvr_fd);
		free(p);
		return -1;
	}
	
	/* Configure packet size */
	p->packet_size = 188;
	p->packet = (char *) malloc(p->packet_size);

	priv->options.packet_size = p->packet_size;
	priv->options.packet_error_correction_bytes = p->packet_size - 188;

	priv->parser = p;
	return 0;
}

/**
 * linuxdvb_destroy_parser: backend's destroy() method.
 */
int linuxdvb_destroy_parser(struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
	int ret;

	ret = ioctl(p->demux_fd, DMX_STOP);
	if (ret < 0)
		perror("DMX_STOP");

	close(p->demux_fd);
	close(p->dvr_fd);
	free(p->packet);
	free(p);
	return 0;
}

/**
 * linuxdvb_read_parser: backend's read() method.
 */
int linuxdvb_read_packet(struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
	ssize_t n = read(p->dvr_fd, p->packet, p->packet_size);
	if (n <= 0) {
		p->packet_valid = false;
		return 0;
	}
	p->packet_valid = true;
	return 0;
}

/**
 * linuxdvb_process_parser: backend's process() method.
 */
int linuxdvb_process_packet(struct ts_header *header, void **payload, struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
	if (! p->packet_valid)
		return -EINVAL;
	else if (! header || ! payload)
		return -EINVAL;

	*payload = (void *) &p->packet[4];

	header->sync_byte                    =  p->packet[0];
	header->transport_error_indicator    = (p->packet[1] >> 7) & 0x01;
	header->payload_unit_start_indicator = (p->packet[1] >> 6) & 0x01;
	header->transport_priority           = (p->packet[1] >> 5) & 0x01;
	header->pid                          = CONVERT_TO_16(p->packet[1], p->packet[2]) & 0x1fff;
	header->transport_scrambling_control = (p->packet[3] >> 6) & 0x03;
	header->adaptation_field             = (p->packet[3] >> 4) & 0x03;
	header->continuity_counter           = (p->packet[3]) & 0x0f;

	return 0;
}

/**
 * linuxdvb_keep_alive: backend's keep_alive() method.
 */
bool linuxdvb_keep_alive(struct demuxfs_data *priv)
{
	return true;
}

struct backend_ops linuxdvb_backend_ops = {
	.create = linuxdvb_create_parser,
	.destroy = linuxdvb_destroy_parser,
	.read = linuxdvb_read_packet,
	.process = linuxdvb_process_packet,
	.keep_alive = linuxdvb_keep_alive,
	.usage = linuxdvb_usage,
};

struct backend_ops *backend_get_ops(void)
{
	return &linuxdvb_backend_ops;
}
