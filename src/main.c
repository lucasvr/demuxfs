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
#include "fsutils.h"
#include "xattr.h"
#include "buffer.h"
#include "hash.h"
#include "fifo.h"
#include "ts.h"
#include "snapshot.h"
#include "tables/descriptors/descriptors.h"
#include "dsm-cc/descriptors/descriptors.h"

/* Platform headers */
#include "backends/filesrc.h"
#include "backends/linuxdvb.h"

/**
 * Available backends
 */
#if defined(USE_FILESRC)
static struct backend_ops *backend = &filesrc_backend_ops;
#elif defined(USE_LINUXDVB)
static struct backend_ops *backend = &linuxdvb_backend_ops;
#endif

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
	int ret;
    
	while (backend->keep_alive(priv) && !main_thread_stopped) {
		ret = backend->read(priv);
		if (ret < 0) {
			dprintf("read error");
			break;
		}
		ret = backend->process(priv);
		if (ret < 0 && ret != -ENOBUFS) {
			dprintf("Error processing packet: %s", strerror(-ret));
			break;
		}
	}
	backend->destroy(priv);
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

int main(int argc, char **argv)
{
	struct demuxfs_data *priv = (struct demuxfs_data *) calloc(1, sizeof(struct demuxfs_data));
	assert(priv);

	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	int ret = backend->create(&args, priv);
	if (ret < 0)
		return 1;

	priv->mount_point = strdup(argv[argc-1]);
	fuse_opt_add_arg(&args, "-ointr");
	return fuse_main(args.argc, args.argv, &demuxfs_ops, priv);
}
