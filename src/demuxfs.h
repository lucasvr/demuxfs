#ifndef __demuxfs_h
#define __demuxfs_h

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <limits.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/xattr.h>

#define FUSE_USE_VERSION 26
#include <fuse/fuse_lowlevel.h>
#include <fuse.h>

#include "hash.h"
#include "list.h"

#define dprintf(x...) \
        fprintf(stderr, "%s:%s:%d ", __FILE__, __FUNCTION__, __LINE__); \
        fprintf(stderr, x); \
        fprintf(stderr, "\n");

#define DEMUXFS_SUPER_MAGIC 0xaa55

#define read_lock() do { } while(0)
#define read_unlock() do { } while(0)
#define write_lock() do { } while(0)
#define write_unlock() do { } while(0)

struct input_parser;

struct dentry {
	/* The inode number, which is generated from the transport stream PID and the table_id */
	ino_t inode;
	/* File name */
	char *name;
	/* Mode (file, symlink, directory */
	mode_t mode;
	/* File contents */
	char *contents;
	size_t size;
	/* Private */
	struct list_head list;
	/* List of children dentries, if this dentry happens to represent a directory */
	struct list_head children;
};

#if (__WORDSIZE == 64)
#define FILEHANDLE_TO_DENTRY(fh) ((struct dentry *)(uint64_t)(fh))
#define DENTRY_TO_FILEHANDLE(de) ((uint64_t)(de))
#else
#define FILEHANDLE_TO_DENTRY(fh) ((struct dentry *)(uint32_t)(fh))
#define DENTRY_TO_FILEHANDLE(de) ((uint64_t)(uint32_t)(de))
#endif

/* This definition imposes the maximum size of the hash tables */
#define DEMUXFS_MAX_PIDS 256

struct descriptor;

struct demuxfs_data {
	/* "table" holds PSI structures (ie: PAT, PMT, NIT..) */
	struct hash_table *table;
	/* "pids" holds pointers to parsers of known PSI PIDs */
	struct hash_table *psi_parsers;
	/* "ts_descriptors" holds descriptor tags and the tables that they're allowed to be in */
	struct descriptor *ts_descriptors;
	/* The root dentry ("/") */
	struct dentry *root;
	/* Backend specific data */
	struct input_parser *parser;
	/* General data shared amongst table parsers and descriptor parsers */
	void *shared_data;
};

/* Backend operations */
struct backend_ops {
    int (*create)(struct fuse_args *, struct demuxfs_data *);
    int (*destroy)(struct demuxfs_data *);
    int (*read)(struct demuxfs_data *);
    int (*process)(struct demuxfs_data *);
    bool (*keep_alive)(struct demuxfs_data *);
};

/* Helper functions to traverse the filesystem tree */
#include "fsutils.h"

/* Transport stream parser */
#include "ts.h"

/* Byte convertion */
#include "byteops.h"

/* Transport stream descriptors */
#include "descriptors/descriptors.h"

/* PSI tables */
#include "tables/psi.h"
#include "tables/pat.h"
#include "tables/pmt.h"
#include "tables/nit.h"

/* Platform headers */
#ifdef USE_FILESRC
#include "filesrc.h"
#endif
#ifdef USE_CE2110_GST
#include "ce2110.h"
#endif

enum {
	NUMBER_CONTENTS,
	STRING_CONTENTS,
};

/* Macros to ease the creation of files and directories */
#define CREATE_COMMON(parent,cdentry,out) \
		(cdentry)->inode = 0; \
		INIT_LIST_HEAD(&(cdentry)->children); \
		list_add_tail(&(cdentry)->list, &((parent)->children)); \
		struct dentry **tmp = out; \
		if (out) *tmp = (cdentry);

#define CREATE_FILE_NUMBER(parent,header,member,out) \
	do { \
		struct dentry *dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
		asprintf(&dentry->contents, "%#04x", header->member); \
		dentry->name = strdup(#member); \
		dentry->size = strlen(dentry->contents); \
		dentry->mode = S_IFREG | 0444; \
		CREATE_COMMON(parent,dentry,out); \
	} while(0)

#define CREATE_FILE_STRING(parent,header,member,out) \
	do { \
		struct dentry *dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
		dentry->contents = strdup(header->member); \
		dentry->name = strdup(#member); \
		dentry->size = strlen(dentry->contents); \
		dentry->mode = S_IFREG | 0444; \
		CREATE_COMMON(parent,dentry,out); \
	} while(0)

#define CREATE_SYMLINK(parent,sname,target,out) \
	do { \
		struct dentry *dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
		dentry->contents = strdup(target); \
		dentry->name = strdup(sname); \
		dentry->mode = S_IFLNK | 0777; \
		CREATE_COMMON(parent,dentry,out); \
	} while(0)

#define CREATE_DIRECTORY(parent,dname,out) \
	do { \
		struct dentry *dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
		dentry->name = strdup(dname); \
		dentry->mode = S_IFDIR | 0555; \
		CREATE_COMMON(parent,dentry,out); \
	} while(0)

#endif /* __demuxfs_h */
