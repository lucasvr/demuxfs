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

void fsutils_dump_tree(struct dentry *dentry, int spaces)
{
	int i;
	if (! dentry)
		return;
	if (spaces == 0) {
		for (i=0; i<spaces; ++i)
			printf(" ");
		printf("%s [%s]\n", dentry->name, 
				dentry->mode & S_IFDIR ? "dir" : 
				dentry->mode & S_IFREG ? "file" : "symlink");
		spaces += 2;
	}
	struct dentry *ptr;
	list_for_each_entry(ptr, &dentry->children, list) {
		for (i=0; i<spaces; ++i)
			printf(" ");
		printf("%s [%s]\n", ptr->name, 
				ptr->mode & S_IFDIR ? "dir" : 
				ptr->mode & S_IFREG ? "file" : "symlink");
		if (ptr->mode & S_IFDIR)
			fsutils_dump_tree(ptr, spaces+2);
	}
}

#define TRUNCATE_STRING(end) do { if ((end)) *(end) = '\0'; } while(0)
#define RESTORE_STRING(end)  do { if ((end)) *(end) =  '/'; } while(0)

struct dentry * fsutils_has_children(struct dentry *dentry, char *name)
{
	struct dentry *ptr;
	list_for_each_entry(ptr, &dentry->children, list)
		if (! strcmp(ptr->name, name))
			return ptr;
	return NULL;
}

struct dentry * fsutils_get_dentry(struct dentry *root, const char *cpath)
{
	char *end, *ptr;
	char *path = (char *) cpath;
	char *start = strstr(path, "/");
	struct dentry *prev = root;
	struct dentry *cached = NULL;

	if (path && path[1] == '\0')
		return root;

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
		if ((cached = fsutils_has_children(prev, start))) {
			//dprintf("--> found '%s' in the tree", start);
			RESTORE_STRING(end);
			prev = cached;
			continue;
		}
		RESTORE_STRING(end);
		return NULL;
	}
	//printf("returning '%s'\n", prev->name);
	return prev;
}
