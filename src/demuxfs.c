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
#include "snapshot.h"
#include "tables/descriptors/descriptors.h"
#include "dsm-cc/descriptors/descriptors.h"

/* Platform headers */
#include "backends/filesrc.h"

/* FUSE methods implemented in main.c */
extern void * demuxfs_init(struct fuse_conn_info *conn);
extern void demuxfs_destroy(void *data);

static int do_getattr(struct dentry *dentry, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	stbuf->st_ino = dentry->inode;
	stbuf->st_mode = dentry->mode;
	stbuf->st_size = dentry->size;
	stbuf->st_atime = dentry->atime ? dentry->ctime : time(NULL);
	stbuf->st_ctime = dentry->ctime ? dentry->ctime : time(NULL);
	stbuf->st_mtime = dentry->mtime ? dentry->mtime : time(NULL);
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
	dentry->refcount++;
	fi->fh = DENTRY_TO_FILEHANDLE(dentry);
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
	if (DEMUXFS_IS_SNAPSHOT(dentry))
		snapshot_destroy_video_context(dentry);
	pthread_mutex_unlock(&dentry->mutex);
	return 0;
}

static int demuxfs_read(const char *path, char *buf, size_t size, off_t offset, 
		struct fuse_file_info *fi)
{
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	struct dentry *dentry = FILEHANDLE_TO_DENTRY(fi->fh);
	ssize_t read_size = 0;
	int ret = 0;

	if (! dentry)
		return -ENOENT;

	if (DEMUXFS_IS_SNAPSHOT(dentry)) {
		pthread_mutex_lock(&dentry->mutex);
		if (! dentry->contents) {
			/* Initialize software decoder context */
			ret = snapshot_init_video_context(dentry);
			if (ret < 0) {
				pthread_mutex_unlock(&dentry->mutex);
				return ret;
			}
			/* Request FFmpeg to decode a video frame out of the ES dentry lended to us */
			ret = snapshot_save_video_frame(dentry, priv);
		}
		if (ret == 0 && (ssize_t) offset < dentry->size) {
			read_size = ((dentry->size - (ssize_t) offset) > (ssize_t) size)
				? size : dentry->size - (ssize_t) offset;
			memcpy(buf, &dentry->contents[offset], read_size);
		}
		pthread_mutex_unlock(&dentry->mutex);
	} else if (dentry->contents && dentry->size != 0xffffff) {
		pthread_mutex_lock(&dentry->mutex);
		if (offset < dentry->size) {
			read_size = ((dentry->size - (ssize_t) offset) > (ssize_t) size)
				? size : dentry->size - (ssize_t) offset;
			memcpy(buf, &dentry->contents[offset], read_size);
		}
		pthread_mutex_unlock(&dentry->mutex);
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
