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

struct input_parser {
	char *filesrc;
	FILE *fp;
	char packet[TS_PACKET_SIZE];
	bool packet_valid;
	bool fileloop;
    struct ts_status ts_status;
};

/**
 * Command line parsing routines.
 */
static void filesrc_usage(void)
{
	fprintf(stderr, "FILESRC options:\n"
			"    -o filesrc=FILE        transport stream input file\n"
			"    -o fileloop=1|0        loop back on EOF\n\n");
}

#define FILESRC_OPT(templ,offset,value) { templ, offsetof(struct input_parser, offset), value }

enum { KEY_HELP };

static struct fuse_opt filesrc_opts[] = {
	FILESRC_OPT("filesrc=%s",   filesrc, 0),
	FILESRC_OPT("fileloop=%d",  fileloop, 0),
	FUSE_OPT_KEY("-h",          KEY_HELP),
	FUSE_OPT_KEY("--help",      KEY_HELP),
	FUSE_OPT_END
};

static int filesrc_parse_opts(void *priv, const char *arg, int key, struct fuse_args *outargs)
{
	struct fuse_operations fake_ops;
	memset(&fake_ops, 0, sizeof(fake_ops));

	switch (key) {
		case FUSE_OPT_KEY_OPT:
		case FUSE_OPT_KEY_NONOPT:
			break;
		case KEY_HELP:
		default:
			filesrc_usage();
			fuse_opt_add_arg(outargs, "-ho");
			fuse_main(outargs->argc, outargs->argv, &fake_ops, NULL);
			exit(key == KEY_HELP ? 0 : 1);
	}
	return 1;
}

/**
 * filesrc_create_parser: backend's create() method.
 */
int filesrc_create_parser(struct fuse_args *args, struct demuxfs_data *priv)
{
    struct input_parser *p = calloc(1, sizeof(struct input_parser));
    assert(p);

	int ret = fuse_opt_parse(args, p, filesrc_opts, filesrc_parse_opts);
	if (ret < 0) {
		free(p);
		return -1;
	}
	if (! p->filesrc) {
		fprintf(stderr, "Error: missing '-o filesrc=FILE' option\n");
		free(p);
		return -1;
	}
	p->fp = fopen(p->filesrc, "r");
	if (! p->fp) {
		perror(p->filesrc);
		free(p);
		return -1;
	}
	bool found_sync_byte = false;
	while (! feof(p->fp)) {
		long off = ftell(p->fp);
		char data1, data2;
		fread(&data1, 1, sizeof(char), p->fp);
		fseek(p->fp, sizeof(p->packet)-1, SEEK_CUR);
		fread(&data2, 1, sizeof(char), p->fp);
		if (data1 == TS_SYNC_BYTE && data2 == data1) {
			fseek(p->fp, off, SEEK_SET);
			found_sync_byte = true;
			break;
		}
	}
	if (! found_sync_byte) {
		fprintf(stderr, "Error: %s doesn't seem to be a valid transport stream.\n", p->filesrc);
		fclose(p->fp);
		free(p);
		return -1;
	}
	priv->parser = p;
	return 0;
}

/**
 * filesrc_destroy_parser: backend's destroy() method.
 */
int filesrc_destroy_parser(struct demuxfs_data *priv)
{
	fclose(priv->parser->fp);
    free(priv->parser);
	return 0;
}

/**
 * filesrc_read_parser: backend's read() method.
 */
int filesrc_read_packet(struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
	size_t n = fread(p->packet, sizeof(p->packet), 1, p->fp);
	if (n <= 0 && feof(p->fp)) {
		p->packet_valid = false;
		if (p->fileloop) {
			rewind(p->fp);
			return 0;
		}
		return -1;
	} else if (n < 1) {
		p->packet_valid = false;
		perror("fread");
		return -1;
	}
	p->packet_valid = true;
	return 0;
}

/**
 * filesrc_process_parser: backend's process() method.
 */
int filesrc_process_packet(struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
	void *payload = (void *) &p->packet[4];
	if (! p->packet_valid)
		return -1;

    struct ts_header header;
	header.sync_byte                    =  p->packet[0];
	header.transport_error_indicator    = (p->packet[1] >> 7) & 0x01;
	header.payload_unit_start_indicator = (p->packet[1] >> 6) & 0x01;
	header.transport_priority           = (p->packet[1] >> 5) & 0x01;
	header.pid                          = ((p->packet[1] << 8) | p->packet[2]) & 0x1fff;
	header.transport_scrambling_control = (p->packet[3] >> 6) & 0x03;
	header.adaptation_field             = (p->packet[3] >> 4) & 0x03;
	header.continuity_counter           = (p->packet[3]) & 0x0f;
	return ts_parse_packet(&header, payload, priv);
}

/**
 * filesrc_keep_alive: backend's keep_alive() method.
 */
bool filesrc_keep_alive(struct demuxfs_data *priv)
{
	return !feof(priv->parser->fp);
}

struct backend_ops filesrc_backend_ops = {
    .create = filesrc_create_parser,
    .destroy = filesrc_destroy_parser,
    .read = filesrc_read_packet,
    .process = filesrc_process_packet,
    .keep_alive = filesrc_keep_alive,
};
