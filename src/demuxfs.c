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

static void * demuxfs_init(struct fuse_conn_info *conn);

static int demuxfs_getattr(const char *path, struct stat *stbuf)
{
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	struct dentry *dentry;

	dentry = fsutils_get_dentry(priv->root, path);
	if (! dentry)
		return -ENOENT;

	memset(stbuf, 0, sizeof(struct stat));
	stbuf->st_ino = dentry->inode;
	stbuf->st_mode = dentry->mode;
	stbuf->st_size = dentry->size;
	stbuf->st_nlink = 1;
    stbuf->st_blksize = 128;
    stbuf->st_dev = DEMUXFS_SUPER_MAGIC;
    stbuf->st_rdev = 0;

	return 0;
}

static int demuxfs_open(const char *path, struct fuse_file_info *fi)
{
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	struct dentry *dentry = fsutils_get_dentry(priv->root, path);
	if (! dentry)
		return -ENOENT;
	fi->fh = DENTRY_TO_FILEHANDLE(dentry);
	return 0;
}

static int demuxfs_flush(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

static int demuxfs_release(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

static int demuxfs_read(const char *path, char *buf, size_t size, off_t offset, 
		struct fuse_file_info *fi)
{
	struct dentry *dentry = FILEHANDLE_TO_DENTRY(fi->fh);
	if (! dentry)
		return -ENOENT;
	if (! dentry->contents) {
		fprintf(stderr, "Error: dentry for '%s' doesn't have any contents set\n", path);
		return -ENOTSUP;
	}
	size_t read_size = (dentry->size > size) ? size : dentry->size;
	memcpy(buf, dentry->contents, read_size);
	return read_size;
}

static int demuxfs_opendir(const char *path, struct fuse_file_info *fi)
{
	return demuxfs_open(path, fi);
}

static int demuxfs_releasedir(const char *path, struct fuse_file_info *fi)
{
	return demuxfs_release(path, fi);
}

static int demuxfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
		off_t offset, struct fuse_file_info *fi)
{
	struct dentry *dentry = FILEHANDLE_TO_DENTRY(fi->fh);

	if (filler(buf, ".",  NULL, 0))
		return -ENOBUFS;
	if (filler(buf, "..", NULL, 0))
		return -ENOBUFS;
	if (! dentry)
		return -ENOENT;

	struct dentry *entry;
	list_for_each_entry(entry, &dentry->children, list)
		if (filler(buf, entry->name, NULL, 0))
			return -ENOBUFS;

	return 0;
}

static int demuxfs_readlink(const char *path, char *buf, size_t size)
{
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	struct dentry *dentry = fsutils_get_dentry(priv->root, path);
	if (! dentry)
		return -ENOENT;
	if (dentry->mode & S_IFLNK) {
		snprintf(buf, size, "%s", dentry->contents);
		return 0;
	}
	return -EINVAL;
}

static int demuxfs_access(const char *path, int mode)
{
	return (mode & W_OK) ? -EACCES : 0;
}

static struct fuse_operations demuxfs_ops = {
	.init        = demuxfs_init,
	.getattr     = demuxfs_getattr,
	.open        = demuxfs_open,
	.flush       = demuxfs_flush,
	.release     = demuxfs_release,
	.read        = demuxfs_read,
	.opendir     = demuxfs_opendir,
	.releasedir  = demuxfs_releasedir,
	.readdir     = demuxfs_readdir,
	.readlink    = demuxfs_readlink,
	.access      = demuxfs_access,
#if 0
	.fgetattr    = demuxfs_fgetattr,
	.setxattr    = demuxfs_setxattr,
	.getxattr    = demuxfs_getxattr,
	.listxattr   = demuxfs_listxattr,
	.removexattr = demuxfs_removexattr,
	.statfs      = demuxfs_statfs,
	/* These should not be supported anytime soon */
	.fsync       = demuxfs_fsync,
	.utimens     = demuxfs_utimens,
	.symlink     = demuxfs_symlink,
	.link        = demuxfs_link,
	.chmod       = demuxfs_chmod,
	.chown       = demuxfs_chown,
	.create      = demuxfs_create,
	.truncate    = demuxfs_truncate,
	.ftruncate   = demuxfs_ftruncate,
	.unlink      = demuxfs_unlink,
	.rename      = demuxfs_rename,
	.mkdir       = demuxfs_mkdir,
	.rmdir       = demuxfs_rmdir,
	.fsyncdir    = demuxfs_fsyncdir,
	.write       = demuxfs_write,
#endif
};

/**
 * Available backends
 */
#if defined(USE_FILESRC)
static struct backend_ops *backend = &filesrc_backend_ops;
#elif defined(USE_CE2110_GST)
static struct backend_ops *backend = &ce2110_backend_ops;
#endif

/**
 * ts_parser_thread: consumes transport stream packets from the input and processes them.
 * @userdata: private data
 */
void * ts_parser_thread(void *userdata)
{
	struct demuxfs_data *priv = (struct demuxfs_data *) userdata;
	int ret;
    
	while (backend->keep_alive(priv)) {
        ret = backend->read(priv);
		if (ret == 0)
			backend->process(priv);
    }

	printf("destroying backend.\n");
    backend->destroy(priv);
	pthread_exit(NULL);
}

static struct dentry * create_rootfs(const char *name)
{
	struct dentry *dentry = (struct dentry *) calloc(1, sizeof(struct dentry));
	dentry->inode = 1;
	dentry->name = strdup(name);
	dentry->mode = S_IFDIR | 0555;
	INIT_LIST_HEAD(&dentry->children);
	return dentry;
}

static void * demuxfs_init(struct fuse_conn_info *conn)
{
	struct demuxfs_data *priv = fuse_get_context()->private_data;

	priv->table = hashtable_new(DEMUXFS_MAX_PIDS);
	priv->psi_parsers = hashtable_new(DEMUXFS_MAX_PIDS);
	priv->ts_descriptors = descriptors_init(priv);
	priv->root = create_rootfs("/");

	pthread_t tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&tid, &attr, ts_parser_thread, priv);

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

	return fuse_main(args.argc, args.argv, &demuxfs_ops, priv);
}