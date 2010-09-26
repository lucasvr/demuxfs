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
#include "fsutils.h"
#include "xattr.h"
#include "buffer.h"
#include "hash.h"
#include "fifo.h"
#include "ts.h"
#include "backend.h"
#include "snapshot.h"
#include "tables/descriptors/descriptors.h"
#include "dsm-cc/descriptors/descriptors.h"

/* Defined in demuxfs.c */
extern struct fuse_operations demuxfs_ops;

/* Globals */
static bool main_thread_stopped;

/**
 * ts_parser_thread: consumes transport stream packets from the input and processes them.
 * @userdata: private data
 */
void * ts_parser_thread(void *userdata)
{
	struct demuxfs_data *priv = (struct demuxfs_data *) userdata;
	struct ts_header header;
	void *payload = NULL;
	int ret;
    
	while (priv->backend->keep_alive(priv) && !main_thread_stopped) {
		ret = priv->backend->read(priv);
		if (ret < 0) {
			dprintf("read error");
			break;
		}
		ret = priv->backend->process(&header, &payload, priv);
		if (ret < 0)
			continue;
		ret = ts_parse_packet(&header, payload, priv);
		if (ret < 0 && ret != -ENOBUFS) {
			dprintf("Error processing packet: %s", strerror(-ret));
			break;
		}
	}
	pthread_exit(NULL);
}

static struct dentry * create_rootfs(const char *name, struct demuxfs_data *priv)
{
	struct dentry *dentry = (struct dentry *) calloc(1, sizeof(struct dentry));

	dentry->name = strdup(name);
	dentry->inode = 1;
	dentry->mode = S_IFDIR | 0555;
	INIT_LIST_HEAD(&dentry->children);
	INIT_LIST_HEAD(&dentry->xattrs);
	INIT_LIST_HEAD(&dentry->list);

	return dentry;
}

/**
 * Implements FUSE destroy method.
 */
void demuxfs_destroy(void *data)
{
	struct demuxfs_data *priv = fuse_get_context()->private_data;

	main_thread_stopped = true;
	pthread_join(priv->ts_parser_id, NULL);

	descriptors_destroy(priv->ts_descriptors);
	dsmcc_descriptors_destroy(priv->dsmcc_descriptors);
	hashtable_destroy(priv->pes_parsers, NULL);
	hashtable_destroy(priv->psi_parsers, NULL);
	hashtable_destroy(priv->pes_tables, NULL);
	hashtable_destroy(priv->psi_tables, (hashtable_free_function_t) free);
	hashtable_destroy(priv->packet_buffer, (hashtable_free_function_t) buffer_destroy);
	fsutils_dispose_tree(priv->root);
}

/**
 * Implements FUSE init method.
 */
void * demuxfs_init(struct fuse_conn_info *conn)
{
	struct demuxfs_data *priv = fuse_get_context()->private_data;

#ifdef USE_FFMPEG
	avcodec_init();
	avcodec_register_all();
#endif
	priv->psi_tables = hashtable_new(DEMUXFS_MAX_PIDS);
	priv->pes_tables = hashtable_new(DEMUXFS_MAX_PIDS);
	priv->psi_parsers = hashtable_new(DEMUXFS_MAX_PIDS);
	priv->pes_parsers = hashtable_new(DEMUXFS_MAX_PIDS);
	priv->packet_buffer = hashtable_new(DEMUXFS_MAX_PIDS);
	priv->ts_descriptors = descriptors_init(priv);
	priv->dsmcc_descriptors = dsmcc_descriptors_init(priv);
	priv->root = create_rootfs("/", priv);
	pthread_create(&priv->ts_parser_id, NULL, ts_parser_thread, priv);

	return priv;
}

/**
 * Parse command line options.
 */
enum { KEY_HELP, KEY_VERSION };

#define DEMUXFS_OPT(templ,offset,value) { templ, offsetof(struct demuxfs_data, offset), value }

static struct fuse_opt demuxfs_options[] = {
	DEMUXFS_OPT("backend=%s",   opt_backend, 0),
	DEMUXFS_OPT("parse_pes=%d", opt_parse_pes, 0),
	DEMUXFS_OPT("standard=%s",  opt_standard, 0),
	DEMUXFS_OPT("tmpdir=%s",    opt_tmpdir, 0),
	DEMUXFS_OPT("report=%s",    opt_report, 0),
	FUSE_OPT_KEY("-h",          KEY_HELP),
	FUSE_OPT_KEY("--help",      KEY_HELP),
	FUSE_OPT_END
};

static void demuxfs_usage(struct demuxfs_data *priv)
{
	fprintf(stderr, "\nDEMUXFS options:\n"
			"    -o backend=MODULE      full path to the backend or the backend's basename (eg: linuxdvb, filesrc)\n"
			"    -o parse_pes=1|0       parse PES packets (default: 0)\n"
			"    -o standard=TYPE       transmission type: SBTVD, ISDB, DVB or ATSC (default: SBTVD)\n"
			"    -o tmpdir=DIR          temporary directory in which to store DSM-CC files (default: %s)\n"
			"    -o report=MASK         colon-separated list of errors to report: NONE,CRC,CONTINUITY or ALL (default: NONE)\n",
			FS_DEFAULT_TMPDIR);
	backend_print_usage();
}

static int demuxfs_parse_options(void *priv, const char *arg, int key, struct fuse_args *outargs)
{
	struct fuse_operations fake_ops;
	memset(&fake_ops, 0, sizeof(fake_ops));

	switch (key) {
		case FUSE_OPT_KEY_OPT:
		case FUSE_OPT_KEY_NONOPT:
			break;
		case KEY_HELP:
		default:
			fuse_opt_add_arg(outargs, "-ho");
			fuse_main(outargs->argc, outargs->argv, &fake_ops, NULL);
			demuxfs_usage((struct demuxfs_data *) priv);
			exit(key == KEY_HELP ? 0 : 1);
	}
	return 1;
}

int main(int argc, char **argv)
{
	struct demuxfs_data *priv = (struct demuxfs_data *) calloc(1, sizeof(struct demuxfs_data));
	assert(priv);

	/* Parse command line options */
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	int ret = fuse_opt_parse(&args, priv, demuxfs_options, demuxfs_parse_options);
	if (ret < 0)
		goto out_free;

	if (! priv->opt_standard || ! strcasecmp(priv->opt_standard, "SBTVD"))
		priv->options.standard = SBTVD_STANDARD;
	else if (! strcasecmp(priv->opt_standard, "ISDB"))
		priv->options.standard = ISDB_STANDARD;
	else if (! strcasecmp(priv->opt_standard, "DVB"))
		priv->options.standard = DVB_STANDARD;
	else if (! strcasecmp(priv->opt_standard, "ATSC"))
		priv->options.standard = ATSC_STANDARD;
	else {
		fprintf(stderr, "Error: %s is not a valid standard option.\n", priv->opt_standard);
		ret = 1;
		goto out_free;
	}

	if (! priv->opt_backend) {
		fprintf(stderr, "Error: no backend was supplied\n");
		ret = 1;
		goto out_free;
	}

	if (priv->opt_report) {
		char *opt_copy = strdup(priv->opt_report);
		char *opt = opt_copy;
		while (opt) {
			char *colon = strstr(opt, ":");
			if (colon)
				*colon = '\0';
			if (! strcasecmp(opt, "NONE"))
				priv->options.verbose_mask = 0;
			else if (! strcasecmp(opt, "ALL"))
				priv->options.verbose_mask = ALL_ERRORS;
			else {
				bool valid_opt = false;
				if (! strcasecmp(opt, "CRC")) {
					priv->options.verbose_mask |= CRC_ERROR;
					valid_opt = true;
				}
				if (! strcasecmp(opt, "CONTINUITY")) {
					priv->options.verbose_mask |= CONTINUITY_ERROR;
					valid_opt = true;
				}
				if (! valid_opt) {
					fprintf(stderr, "Invalid value '%s' for '-o report'\n", opt);
					free(opt_copy);
					goto out_free;
				}
			}
			opt = colon ? ++colon : NULL;
		}
		free(opt_copy);
	}

	priv->options.tmpdir = strdup(priv->opt_tmpdir ? priv->opt_tmpdir : FS_DEFAULT_TMPDIR);
	priv->options.parse_pes = priv->opt_parse_pes;

	/* Load the chosen backend */
	void *backend_handle = NULL;
	priv->backend = backend_load(priv->opt_backend, &backend_handle);
	if (! priv->backend) {
		fprintf(stderr, "Invalid backend, cannot continue.\n");
		ret = 1;
		goto out_free;
	}

	/* Initialize the backend */
	ret = priv->backend->create(&args, priv);
	if (ret < 0)
		goto out_unload;

	if (priv->options.frequency) {
		ret = priv->backend->set_frequency(priv->options.frequency, priv);
		if (ret < 0) {
			fprintf(stderr, "Failed to set frequency: %s\n", strerror(-ret));
			goto out_destroy;
		}
	}

	/* Start the FUSE services */
	priv->mount_point = strdup(argv[argc-1]);
	fuse_opt_add_arg(&args, "-ointr");
	ret = fuse_main(args.argc, args.argv, &demuxfs_ops, priv);

out_destroy:
	/* Destroy the backend private data */
	priv->backend->destroy(priv);

out_unload:
	/* Unload the backend */
	backend_unload(backend_handle);

out_free:
	fuse_opt_free_args(&args);
	if (priv) {
		if (priv->mount_point)
			free(priv->mount_point);
		if (priv->options.tmpdir)
			free(priv->options.tmpdir);
		free(priv);
	}

	return ret;
}
