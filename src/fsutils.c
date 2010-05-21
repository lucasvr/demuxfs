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
#include "buffer.h"
#include "xattr.h"
#include "fifo.h"

static void _fsutils_dump_tree(struct dentry *dentry, int spaces);

/**
 * Resolve the full pathname for a given dentry up to DemuxFS' root dentry.
 * @dentry: dentry to resolve.
 * @buf: input work buffer.
 * @size: input work buffer size.
 *
 * Returns a pointer to the start of the string in the input buffer on
 * success or NULL if the dentry could not be resolved to a path.
 */
char *fsutils_path_walk(struct dentry *dentry, char *buf, size_t size)
{
	struct dentry *d = dentry;
	char *ptr = &buf[size-1];
	int roomsize = size;
	bool append_slash = false;

	memset(buf, 0, size);
	while (d) {
		int len = strlen(d->name);
		if (len+1 > roomsize) {
			dprintf("Not enough room in buffer to resolve dentry's pathname.");
			return NULL;
		}
		if (! d->parent)
			append_slash = false;
		ptr -= len;
		if (append_slash) {
			char slash = '/';
			ptr--;
			memcpy(ptr, d->name, len);
			memcpy(ptr+len, &slash, 1);
			roomsize -= len + 1;
		} else {
			memcpy(ptr, d->name, len);
			roomsize -= len;
		}
		append_slash = true;
		d = d->parent;
	}
	return ptr;
}

/**
 * Resolve the full pathname for a given dentry up to the real rootfs directory.
 * @dentry: dentry to resolve.
 * @buf: input work buffer.
 * @size: input work buffer size.
 *
 * Returns a pointer to the start of the string in the input buffer on
 * success or NULL if the dentry could not be resolved to a path.
 */
char *fsutils_realpath(struct dentry *dentry, char *buf, size_t size, struct demuxfs_data *priv)
{
	char *start = fsutils_path_walk(dentry, buf, size);
	if (start) {
		start -= strlen(priv->mount_point);
		memcpy(start, priv->mount_point, strlen(priv->mount_point));
	}
	return start;
}


/**
 * Dump the filesystem tree starting at a given dentry.
 * @dentry: starting point.
 */
void fsutils_dump_tree(struct dentry *dentry)
{
	return _fsutils_dump_tree(dentry, 0);
}

static void _fsutils_dump_tree(struct dentry *dentry, int spaces)
{
	int i;
	if (! dentry)
		return;
	if (spaces == 0) {
		fprintf(stderr, "%s [%s] inode=%#llx\n", 
				dentry->name ? dentry->name : "(null)", 
				dentry->mode & S_IFDIR ? "dir" : 
				dentry->mode & S_IFREG ? "file" : "symlink",
				dentry->inode);
		spaces += 2;
	}
	struct dentry *ptr;
	list_for_each_entry(ptr, &dentry->children, list) {
		for (i=0; i<spaces; ++i)
			fprintf(stderr, " ");
		fprintf(stderr, "%s [%s] inode=%#llx\n", 
				ptr->name ? ptr->name : "(null)", 
				ptr->mode & S_IFDIR ? "dir" : 
				ptr->mode & S_IFREG ? "file" : "symlink",
				ptr->inode);
		if (ptr->mode & S_IFDIR)
			_fsutils_dump_tree(ptr, spaces+2);
	}
}

/**
 * Dispose a dentry and its allocated memory.
 * @dentry: dentry to deallocate.
 */
void fsutils_dispose_node(struct dentry *dentry)
{
	struct xattr *xattr, *aux;

	if (dentry->priv) {
		switch (dentry->obj_type) {
			case OBJ_TYPE_SNAPSHOT: {
				struct snapshot_priv *priv = (struct snapshot_priv *) dentry->priv;
				priv->borrowed_es_dentry = NULL;
				priv->snapshot_ctx = NULL;
				if (priv->path)
					free(priv->path);
				free(priv);
				break;
			}
			case OBJ_TYPE_FIFO:
			case OBJ_TYPE_AUDIO_FIFO: {
				struct fifo_priv *priv = dentry->priv;
				if (priv->fifo)
					fifo_destroy(priv->fifo);
				free(priv);
				break;
			}
			case OBJ_TYPE_VIDEO_FIFO: {
				struct video_fifo_priv *priv = (struct video_fifo_priv *) dentry->priv;
				if (priv->fifo)
					fifo_destroy(priv->fifo);
				free(priv);
				break;
			}
		}
	}

	if (dentry->contents)
		free(dentry->contents);
	list_for_each_entry_safe(xattr, aux, &dentry->xattrs, list)
		xattr_free(xattr);
	if (dentry->name)
		free(dentry->name);
	pthread_mutex_destroy(&dentry->mutex);
	sem_destroy(&dentry->semaphore);
	list_del(&dentry->list);
	free(dentry);
}

/**
 * Dispose all nodes and their memory starting at a given dentry.
 * @dentry: starting point
 */
void fsutils_dispose_tree(struct dentry *dentry)
{
	struct dentry *ptr, *aux;
	
	if (! dentry)
		return;
	
	list_for_each_entry_safe(ptr, aux, &dentry->children, list) {
		if (ptr->mode & S_IFDIR)
			fsutils_dispose_tree(ptr);
		else
			fsutils_dispose_node(ptr);
	}
	fsutils_dispose_node(dentry);
}

/**
 * Migrate children from 'source' to 'target'. Children whose dentry names
 * are already contained within 'target' are skipped. The moved dentries will
 * no longer be available in source's child list.
 * @source: source dentry
 * @target: target dentry
 */
void fsutils_migrate_children(struct dentry *source, struct dentry *target)
{
	struct dentry *ptr_source, *ptr_target, *aux;

	list_for_each_entry_safe(ptr_source, aux, &source->children, list) {
		bool already_exists = false;
		list_for_each_entry(ptr_target, &target->children, list)
			if (! strcmp(ptr_target->name, ptr_source->name)) {
				already_exists = true;
				break;
			}
		if (! already_exists) {
			list_del(&ptr_source->list);
			list_add_tail(&ptr_source->list, &target->children);
		} else if (S_ISDIR(ptr_target->mode) && S_ISDIR(ptr_source->mode))
			fsutils_migrate_children(ptr_source, ptr_target);
	}
}

#define TRUNCATE_STRING(end) do { if ((end)) *(end) = '\0'; } while(0)
#define RESTORE_STRING(end)  do { if ((end)) *(end) =  '/'; } while(0)

/**
 * Given a parent and a child name, return a pointer to the child dentry.
 * @dentry: parent
 * @name: child name
 *
 * Returns the child dentry on success or NULL if no such child exist.
 */
struct dentry * fsutils_get_child(struct dentry *dentry, const char *name)
{
	struct dentry *ptr;
	if (! strcmp(name, "."))
		return dentry;
	if (! strcmp(name, ".."))
		return dentry->parent ? dentry->parent : dentry;
	list_for_each_entry(ptr, &dentry->children, list)
		if (! strcmp(ptr->name, name))
			return ptr;
	return NULL;
}

/**
 * Converts a path to a dentry.
 * @root: search starting point
 * @cpath: complete pathname. Must point to a real variable.
 *
 * Returns the resolved dentry on success or NULL on failure.
 */
struct dentry * fsutils_get_dentry(struct dentry *root, const char *cpath)
{
	char *start, *end, *ptr;
	char path[strlen(cpath)+1];
	struct dentry *prev = root;
	struct dentry *cached = NULL;

	if (cpath && cpath[1] == '\0')
		return root;

	/* We cannot change the input path in any way */
	strcpy(path, cpath);
	start = strstr(path, "/");
	if (! start && S_ISLNK(root->mode)) {
		/* One-level symlink */
		struct dentry *parent = root->parent;
		if (! parent)
			return NULL;
		return fsutils_get_child(parent, cpath);
	}

	while (start) {
		ptr = start;
		start = strstr(start, "/");
		if (! start)
			break;
		start++;
		end = strstr(start, "/");
		TRUNCATE_STRING(end);
		if (strlen(start) == 0) {
			RESTORE_STRING(end);
			dprintf("could not resolve '%s'", path);
			return NULL;
		}
		if ((cached = fsutils_get_child(prev, start))) {
			//dprintf("--> found '%s' in the tree", start);
			RESTORE_STRING(end);
			prev = cached;
			continue;
		}
		RESTORE_STRING(end);

		if (prev->mode & S_IFLNK) {
			/* Follow symlink */
			cached = fsutils_get_dentry(prev, prev->contents);
			if (cached) {
				prev = cached;
				start--;
				continue;
			}
		}
		return NULL;
	}
	//printf("returning '%s'\n", prev->name);
	return prev;
}

struct dentry * fsutils_find_by_inode(struct dentry *root, ino_t inode)
{
	struct dentry *ptr, *ret = NULL;
	
	if (! root)
		return NULL;
	else if (root->inode == inode)
		return root;

	list_for_each_entry(ptr, &root->children, list) {
		if (ptr->mode & S_IFDIR)
			ret = fsutils_find_by_inode(ptr, inode);
		else if (ptr->inode == inode)
			ret = ptr;
		if (ret)
			return ret;
	}
	return NULL;
}

struct dentry * fsutils_create_version_dir(struct dentry *parent, int version)
{
	char version_dir[32];
	struct dentry *child;
	struct dentry *current;

	snprintf(version_dir, sizeof(version_dir), "Version_%d", version);
	child = CREATE_DIRECTORY(parent, version_dir);
	
	/* Update the 'Current' symlink if it exists or create a new symlink if it doesn't */
	current = fsutils_get_child(parent, FS_CURRENT_NAME);
	if (! current)
		current = CREATE_SYMLINK(parent, FS_CURRENT_NAME, version_dir);
	else {
		pthread_mutex_lock(&current->mutex);
		free(current->contents);
		current->contents = strdup(version_dir);
		pthread_mutex_unlock(&current->mutex);
	}

	return child;
}

struct dentry * fsutils_get_current(struct dentry *parent)
{
	struct dentry *target = NULL;
	struct dentry *current = fsutils_get_child(parent, FS_CURRENT_NAME);
	if (current)
		target = fsutils_get_child(parent, current->contents);
	return target;
}
