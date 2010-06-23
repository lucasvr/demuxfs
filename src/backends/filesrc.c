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
#include "filesrc.h"
#include "fsutils.h"
#include "byteops.h"
#include "backend.h"
#include "ts.h"

struct input_parser {
	char *filesrc;				/**< File source (cmdline option) */
	FILE *fp;					/**< File source handle */
	char *packet;				/**< Current TS packet being processed */
	bool packet_valid;			/**< True if TS packet is valid, False if it's not */
	uint8_t packet_size;		/**< Packet size (188, 204, 208 bytes) */
	int fileloop;				/**< How many times to loop the file on EOF (cmdline option) */
};

/**
 * Command line parsing routines.
 */
void filesrc_usage(void)
{
	fprintf(stderr, "\nFILESRC options:\n"
			"    -o filesrc=FILE          transport stream input file\n"
			"    -o fileloop=<count>      how many times to loop on EOF, -1 means infinite (default: 0)\n");
}

#define FILESRC_OPT(templ,offset,value) { templ, offsetof(struct input_parser, offset), value }

static struct fuse_opt filesrc_opts[] = {
	FILESRC_OPT("filesrc=%s",   filesrc, 0),
	FILESRC_OPT("fileloop=%d",  fileloop, 0),
	FUSE_OPT_END
};

static int filesrc_parse_opts(void *priv, const char *arg, int key, struct fuse_args *outargs)
{
	return 1;
}


static bool search_sync_byte(struct input_parser *p, uint8_t packet_size)
{
	int attempts = 5;
	char data1, data2;
	long offset = ftell(p->fp);

	while (attempts > 0 && ! feof(p->fp)) {
		fread(&data1, 1, sizeof(char), p->fp);
		fseek(p->fp, packet_size-1, SEEK_CUR);
		fread(&data2, 1, sizeof(char), p->fp);
		if (data1 != TS_SYNC_BYTE || data2 != TS_SYNC_BYTE) {
			rewind(p->fp);
			return false;
		}
		fseek(p->fp, -1, SEEK_CUR);
		attempts--;
	}
	fseek(p->fp, offset, SEEK_SET);
	return true;
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

	/* Search for 188, 204 and 208-byte packets */
	uint8_t packet_size[] = { 188, 204, 208 };
	bool found_sync_byte = false;
	for (int i=0; i<sizeof(packet_size)/sizeof(uint8_t); ++i) {
		found_sync_byte = search_sync_byte(p, packet_size[i]);
		if (found_sync_byte) {
			p->packet_size = packet_size[i];
			break;
		}
	}
	if (! found_sync_byte) {
		fprintf(stderr, "Error: %s doesn't seem to be a valid transport stream.\n", p->filesrc);
		fclose(p->fp);
		free(p);
		return -1;
	}
	p->packet = (char *) malloc(p->packet_size * sizeof(char));

	/* Configure packet size */
	priv->options.packet_size = p->packet_size;
	priv->options.packet_error_correction_bytes = p->packet_size - 188;

	priv->parser = p;
	return 0;
}

/**
 * filesrc_destroy_parser: backend's destroy() method.
 */
int filesrc_destroy_parser(struct demuxfs_data *priv)
{
	free(priv->parser->packet);
	fclose(priv->parser->fp);
    free(priv->parser);
	return 0;
}
	
/**
 * filesrc_set_frequency: no-op.
 */
int filesrc_set_frequency(uint32_t frequency, struct demuxfs_data *priv)
{
	(void) frequency;
	(void) priv;
	return -ENOSYS;
}

/**
 * filesrc_read_parser: backend's read() method.
 */
int filesrc_read_packet(struct demuxfs_data *priv)
{
	struct input_parser *p = priv->parser;
	size_t n = fread(p->packet, p->packet_size, 1, p->fp);
	if (n <= 0 && feof(p->fp)) {
		p->packet_valid = false;
		if (p->fileloop == -1 || p->fileloop--) {
			dprintf("Rewinding TS file");
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
int filesrc_process_packet(struct ts_header *header, void **payload, struct demuxfs_data *priv)
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
 * filesrc_keep_alive: backend's keep_alive() method.
 */
bool filesrc_keep_alive(struct demuxfs_data *priv)
{
	return !feof(priv->parser->fp);
}

struct backend_ops filesrc_backend_ops = {
    .create = filesrc_create_parser,
    .destroy = filesrc_destroy_parser,
	.set_frequency = filesrc_set_frequency,
    .read = filesrc_read_packet,
    .process = filesrc_process_packet,
    .keep_alive = filesrc_keep_alive,
	.usage = filesrc_usage,
};

struct backend_ops *backend_get_ops(void)
{
	return &filesrc_backend_ops;
}
