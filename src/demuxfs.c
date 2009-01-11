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

/* Platform headers */
#include "backends/filesrc.h"
#include "backends/ce2110.h"

static void * demuxfs_init(struct fuse_conn_info *conn);
static void demuxfs_destroy(void *data);

static int do_getattr(struct dentry *dentry, struct stat *stbuf)
{
	memset(stbuf, 0, sizeof(struct stat));
	stbuf->st_ino = dentry->inode;
	stbuf->st_mode = dentry->mode;
	stbuf->st_size = dentry->borrowed_pes_dentry ? 0xffffff : dentry->size;
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
	if (dentry->fifo) {
		/* 
		 * Mimic a FIFO. This is a special file which gets filled up periodically
		 * by the PES parsers and consumed on read. Tell FUSE as such. Also, we
		 * only support one reader at a time.
		 */
		if (dentry->refcount > 0) {
			ret = -EBUSY;
			goto out;
		}
		/* Make sure the FIFO is flushed */
		fifo_flush(dentry->fifo);
		fi->direct_io = 1;
#if FUSE_USE_VERSION >= 29
		fi->nonseekable = 1;
#endif
	} else if (dentry->borrowed_pes_dentry) {
		/* Need to borrow the PES dentry while the FIFO is read */
		struct dentry *pes = dentry->borrowed_pes_dentry;
		pthread_mutex_lock(&pes->mutex);
		if (pes->refcount > 0) {
			pthread_mutex_unlock(&pes->mutex);
			ret = -EBUSY;
			goto out;
		}
		pes->refcount++;
		pthread_mutex_unlock(&pes->mutex);

		/* Make sure the FIFO is flushed */
		fifo_flush(pes->fifo);
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
	if (dentry->fifo)
		fifo_flush(dentry->fifo);
	if (dentry->borrowed_pes_dentry) {
		struct dentry *pes = dentry->borrowed_pes_dentry;
		pthread_mutex_lock(&pes->mutex);
		pes->refcount--;
		pthread_mutex_unlock(&pes->mutex);
		if (dentry->contents) {
			/* The snapshot is valid until the file is closed */
			free(dentry->contents);
			dentry->contents = NULL;
			dentry->size = 0;
		}
	}
	pthread_mutex_unlock(&dentry->mutex);
	return 0;
}

static int _read_from_fifo(struct dentry *dentry, char *buf, size_t size)
{
	int ret;
	size_t n;

	pthread_mutex_lock(&dentry->mutex);
	while (fifo_empty(dentry->fifo)) {
		pthread_mutex_unlock(&dentry->mutex);
		ret = sem_wait(&dentry->semaphore);
		if (ret < 0 && errno == EINTR)
			return -EINTR;
		pthread_mutex_lock(&dentry->mutex);
	}
	pthread_mutex_unlock(&dentry->mutex);

	n = fifo_read(dentry->fifo, buf, size);
	return n;
}

static int demuxfs_read(const char *path, char *buf, size_t size, off_t offset, 
		struct fuse_file_info *fi)
{
	int ret;
	size_t read_size = 0;
	struct dentry *dentry = FILEHANDLE_TO_DENTRY(fi->fh);
	if (! dentry)
		return -ENOENT;

	if (dentry->fifo) {
		while (read_size < size) {
			ret = _read_from_fifo(dentry, buf+read_size, size-read_size);
			if (ret == -EINTR)
				return 0;
			read_size += ret;
		}
		return read_size;
	}
	if (dentry->borrowed_pes_dentry) {
		/* 
		 * This is a snapshot file. We read from the FIFO lended to us
		 * and pass that data to the software decoder which will fill
		 * in the user buffer.
		 */
		if (! dentry->contents) {
			struct dentry *pes = dentry->borrowed_pes_dentry;
			char snapshot_buf[SNAPSHOT_BUFFER_SIZE];
			struct snapshot_context *ctx;
			
			ctx = snapshot_init_video_context();
			if (! ctx)
				return -ENXIO;

			while (true) {
				read_size = 0;
				memset(snapshot_buf, 0, sizeof(snapshot_buf));
				while (read_size < SNAPSHOT_INBUF_SIZE) {
					ret = _read_from_fifo(pes, 
							snapshot_buf+read_size, 
							SNAPSHOT_INBUF_SIZE-read_size);
					if (ret < 0) {
						snapshot_destroy_video_context(ctx);
						return -EIO;
					}
					read_size += ret;
				}
				ret = snapshot_save_video_frame(snapshot_buf, read_size, ctx);
				if (ret == 0)
					break;
			}
			dentry->contents = ctx->contents;
			dentry->size = ctx->contents_size;
			snapshot_destroy_video_context(ctx);
		}
	}
	if (dentry->contents) {
		pthread_mutex_lock(&dentry->mutex);
		read_size = (dentry->size > size) ? size : dentry->size;
		memcpy(buf, dentry->contents, read_size);
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
	return (mode & W_OK) ? -EACCES : 0;
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

	write_lock();
	xattr_remove(dentry, name);
	ret = xattr_add(dentry, name, value, size, true);
	write_unlock();
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

static struct fuse_operations demuxfs_ops = {
	.init        = demuxfs_init,
	.destroy     = demuxfs_destroy,
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
#if 0
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

static struct dentry * create_rootfs(const char *name)
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

static void demuxfs_destroy(void *data)
{
	struct demuxfs_data *priv = fuse_get_context()->private_data;
	descriptors_destroy(priv->ts_descriptors);
	hashtable_destroy(priv->pes_parsers, NULL);
	hashtable_destroy(priv->psi_parsers, NULL);
	hashtable_destroy(priv->pes_tables, NULL);
	hashtable_destroy(priv->psi_tables, (hashtable_free_function_t) free);
	hashtable_destroy(priv->packet_buffer, (hashtable_free_function_t) buffer_destroy);
	fsutils_dispose_tree(priv->root);
}

static void * demuxfs_init(struct fuse_conn_info *conn)
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

	fuse_opt_add_arg(&args, "-ointr");
	return fuse_main(args.argc, args.argv, &demuxfs_ops, priv);
}
