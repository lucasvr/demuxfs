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

static void xattr_free(struct xattr *xattr)
{
	if (! xattr)
		return;
	if (xattr->putname) {
		free(xattr->name);
		free(xattr->value);
	}
	list_del(&xattr->list);
	free(xattr);
}

struct xattr *xattr_get(struct dentry *dentry, const char *name)
{
	struct xattr *xattr;
	list_for_each_entry(xattr, &dentry->xattrs, list)
		if (! strcmp(xattr->name, name))
			return xattr;
	return NULL;
}

bool xattr_exists(struct dentry *dentry, const char *name)
{
	struct xattr *xattr;
	list_for_each_entry(xattr, &dentry->xattrs, list)
		if (! strcmp(xattr->name, name))
			return true;
	return false;
}

int xattr_add(struct dentry *dentry, const char *name, const char *value, size_t size, bool putname)
{
	struct xattr *xattr = malloc(sizeof(struct xattr));
	if (! xattr)
		return -ENOMEM;

	if (putname) {
		xattr->name = strdup(name);
		xattr->value = calloc(size, sizeof(char));
		memcpy(xattr->value, value, size);
	} else {
		xattr->name = (char *) name;
		xattr->value = (char *) value;
	}
	xattr->size = size;
	xattr->putname = putname;

	list_add_tail(&xattr->list, &dentry->xattrs);
	return 0;
}

int xattr_list(struct dentry *dentry, char *buf, size_t size)
{
	size_t required = 0, copied = 0;
	struct xattr *xattr;
	char zero = 0;

	list_for_each_entry(xattr, &dentry->xattrs, list)
		required += strlen(xattr->name) + 1;

	if (size == 0)
		return required;
	else if (size < required)
		return -ERANGE;

	list_for_each_entry(xattr, &dentry->xattrs, list) {
		memcpy(buf+copied, xattr->name, strlen(xattr->name));
		copied += strlen(xattr->name);
		memcpy(buf+copied, &zero, 1);
		copied++;
	}
	return copied;
}

int xattr_remove(struct dentry *dentry, const char *name)
{
	struct xattr *xattr, *aux;
	list_for_each_entry_safe(xattr, aux, &dentry->xattrs, list)
		if (! strcmp(xattr->name, name)) {
			xattr_free(xattr);
			return 0;
		}
	return -ENOENT;
}
