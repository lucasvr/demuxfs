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
#include "backends/ce2110.h"

/* FUSE methods implemented in main.c */
extern void * demuxfs_init(struct fuse_conn_info *conn);
extern void demuxfs_destroy(void *data);

static int do_getattr(struct dentry *dentry, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	stbuf->st_ino = dentry->inode;
	stbuf->st_mode = dentry->mode;
	stbuf->st_size = dentry->size;
	stbuf->st_atime = dentry->atime;
	stbuf->st_ctime = dentry->ctime;
	stbuf->st_mtime = dentry->mtime;
	stbuf->st_nlink = 1;
	stbuf->st_blksize = 128;
	stbuf->st_dev = DEMUXFS_SUPER_MAGIC;
	stbuf->st_rdev = 0;
	return 0;
}

static int demuxfs_getattr(const char *path, struct stat *stbuf)
{
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	struct dentry *dentry;

	dentry = fsutils_get_dentry(priv->root, path);
	if (! dentry)
		return -ENOENT;
	return do_getattr(dentry, stbuf);
}

static int demuxfs_fgetattr(const char *path, struct stat *stbuf,
		struct fuse_file_info *fi)
{
	struct dentry *dentry = FILEHANDLE_TO_DENTRY(fi->fh);
	return do_getattr(dentry, stbuf);
}

static int demuxfs_open(const char *path, struct fuse_file_info *fi)
{
	int ret = 0;
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	struct dentry *dentry = fsutils_get_dentry(priv->root, path);
	if (! dentry)
		return -ENOENT;

	pthread_mutex_lock(&dentry->mutex);
	if (DEMUXFS_IS_FIFO(dentry)) {
		struct fifo_priv *fifo_priv = (struct fifo_priv *) dentry->priv;
		struct fifo *fifo = fifo_priv->fifo;

		/* 
		 * Mimic a FIFO. This is a special file which gets filled up periodically
		 * by the PES parsers and consumed on read. Tell FUSE as such. Also, we
		 * only support one reader at a time.
		 */
		ret = fifo_open(fifo);
		if (ret < 0)
			goto out;

		/* Make sure the FIFO is flushed */
		fifo_flush(fifo);
		fi->direct_io = 1;
#if FUSE_USE_VERSION >= 29
		fi->nonseekable = 1;
#endif
	} else if (DEMUXFS_IS_SNAPSHOT(dentry)) {
		/* Make sure the FIFO is flushed */
		struct snapshot_priv *snapshot_priv = (struct snapshot_priv *) dentry->priv;
		struct fifo_priv *fifo_priv = snapshot_priv->borrowed_es_dentry->priv;
		fifo_flush(fifo_priv->fifo);
	}
	dentry->refcount++;
	fi->fh = DENTRY_TO_FILEHANDLE(dentry);
out:
	pthread_mutex_unlock(&dentry->mutex);
	return ret;
}

static int demuxfs_flush(const char *path, struct fuse_file_info *fi)
{
	return 0;
}

static int demuxfs_release(const char *path, struct fuse_file_info *fi)
{
	struct dentry *dentry = FILEHANDLE_TO_DENTRY(fi->fh);
	pthread_mutex_lock(&dentry->mutex);
	dentry->refcount--;
	if (DEMUXFS_IS_FIFO(dentry)) {
		struct fifo_priv *fifo_priv = (struct fifo_priv *) dentry->priv;
		fifo_close(fifo_priv->fifo);
	} else if (DEMUXFS_IS_SNAPSHOT(dentry)) {
		struct snapshot_priv *snapshot_priv = (struct snapshot_priv *) dentry->priv;
		struct fifo_priv *fifo_priv = snapshot_priv->borrowed_es_dentry->priv;
		snapshot_destroy_video_context(dentry);
		fifo_flush(fifo_priv->fifo);
	}
	pthread_mutex_unlock(&dentry->mutex);
	return 0;
}

static int _demuxfs_read_from_fifo(struct dentry *dentry, char *buf, size_t size)
{
	struct fifo_priv *priv = (struct fifo_priv *) dentry->priv;
	size_t n;
	int ret;

	pthread_mutex_lock(&dentry->mutex);
	while (fifo_is_empty(priv->fifo)) {
		pthread_mutex_unlock(&dentry->mutex);
		ret = sem_wait(&dentry->semaphore);
		if (ret < 0 && errno == EINTR)
			return -EINTR;
		pthread_mutex_lock(&dentry->mutex);
	}
	pthread_mutex_unlock(&dentry->mutex);

	n = fifo_read(priv->fifo, buf, size);
	return n;
}

static int demuxfs_read(const char *path, char *buf, size_t size, off_t offset, 
		struct fuse_file_info *fi)
{
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	struct dentry *dentry = FILEHANDLE_TO_DENTRY(fi->fh);
	size_t read_size = 0;
	int ret = 0;

	if (! dentry)
		return -ENOENT;

	if (DEMUXFS_IS_FIFO(dentry)) {
		while (read_size < size) {
			ret = _demuxfs_read_from_fifo(dentry, buf+read_size, size-read_size);
			if (ret == -EINTR)
				return 0;
			read_size += ret;
		}
	} else if (DEMUXFS_IS_SNAPSHOT(dentry)) {
		if (! dentry->contents) {
			/* Initialize software decoder context */
			ret = snapshot_init_video_context(dentry);
			if (ret < 0)
				return ret;
			/* Request FFmpeg to decode a video frame out of the ES dentry lended to us */
			ret = snapshot_save_video_frame(dentry, priv);
		}

		if (ret == 0) {
			pthread_mutex_lock(&dentry->mutex);
			read_size = ((dentry->size - offset) > size) ? size : dentry->size - offset;
			memcpy(buf, &dentry->contents[offset], read_size);
			pthread_mutex_unlock(&dentry->mutex);
		}
	} else if (dentry->contents) {
		pthread_mutex_lock(&dentry->mutex);
		read_size = ((dentry->size - offset) > size) ? size : dentry->size - offset;
		memcpy(buf, &dentry->contents[offset], read_size);
		pthread_mutex_unlock(&dentry->mutex);
	} else {
		dprintf("Error: dentry for '%s' doesn't have any contents set", path);
		return -ENOTSUP;
	}
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
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	struct dentry *dentry = fsutils_get_dentry(priv->root, path);
	if (! dentry)
		return -ENOENT;
	else if (mode & W_OK)
		return -EACCES;
	else if (mode & X_OK && !S_ISDIR(dentry->mode))
		return -EACCES;
	return 0;
}

static int demuxfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
	int ret;
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	struct dentry *dentry = fsutils_get_dentry(priv->root, path);
	if (! dentry)
		return -ENOENT;

	if (strncmp(name, "user.", 5))
		return -EPERM;
	else if ((flags & XATTR_CREATE) && xattr_exists(dentry, name))
		return -EEXIST;
	else if ((flags & XATTR_REPLACE) && !xattr_exists(dentry, name))
		return -ENOATTR;

	pthread_mutex_lock(&dentry->mutex);
	xattr_remove(dentry, name);
	ret = xattr_add(dentry, name, value, size, true);
	if (ret == 0 && DEMUXFS_IS_FIFO(dentry) && !strcmp(name, XATTR_FIFO_SIZE)) {
		struct fifo_priv *fifo_priv = (struct fifo_priv *) dentry->priv;
		fifo_set_max_elements(fifo_priv->fifo, atoi(value));
	}
	pthread_mutex_unlock(&dentry->mutex);
	return ret;
}

static int demuxfs_getxattr(const char *path, const char *name, char *value, size_t size)
{
	int ret;
	struct xattr *xattr;
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	struct dentry *dentry = fsutils_get_dentry(priv->root, path);
	if (! dentry)
		return -ENOENT;

	read_lock();
	xattr = xattr_get(dentry, name);
	if (! xattr) {
		ret = -ENOATTR;
		goto out;
	}
	if (size == 0) {
		ret = xattr->size;
		goto out;
	} else if (size < xattr->size) {
		ret = -ERANGE;
		goto out;
	}
	memcpy(value, xattr->value, xattr->size);
	ret = xattr->size;
out:
	read_unlock();
	return ret;
}

static int demuxfs_listxattr(const char *path, char *list, size_t size)
{
	int ret;
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	struct dentry *dentry = fsutils_get_dentry(priv->root, path);
	if (! dentry)
		return -ENOENT;
	
	read_lock();
	ret = xattr_list(dentry, list, size);
	read_unlock();

	return ret;
}

static int demuxfs_removexattr(const char *path, const char *name)
{
	int ret;
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	struct dentry *dentry = fsutils_get_dentry(priv->root, path);
	if (! dentry)
		return -ENOENT;

	write_lock();
	ret = xattr_remove(dentry, name);
	write_unlock();

	return ret;
}

struct fuse_operations demuxfs_ops = {
	/* Implemented in main.c */
	.init        = demuxfs_init,
	.destroy     = demuxfs_destroy,
	/* Implemented in this file */
	.getattr     = demuxfs_getattr,
	.fgetattr    = demuxfs_fgetattr,
	.open        = demuxfs_open,
	.flush       = demuxfs_flush,
	.release     = demuxfs_release,
	.read        = demuxfs_read,
	.opendir     = demuxfs_opendir,
	.releasedir  = demuxfs_releasedir,
	.readdir     = demuxfs_readdir,
	.readlink    = demuxfs_readlink,
	.access      = demuxfs_access,
	.setxattr    = demuxfs_setxattr,
	.getxattr    = demuxfs_getxattr,
	.listxattr   = demuxfs_listxattr,
	.removexattr = demuxfs_removexattr,
	.statfs      = NULL,
	/* Not implemented on DemuxFS */
	.fsync       = NULL,
	.utimens     = NULL,
	.symlink     = NULL,
	.link        = NULL,
	.chmod       = NULL,
	.chown       = NULL,
	.create      = NULL,
	.truncate    = NULL,
	.ftruncate   = NULL,
	.unlink      = NULL,
	.rename      = NULL,
	.mkdir       = NULL,
	.rmdir       = NULL,
	.fsyncdir    = NULL,
	.write       = NULL,
};
