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

#include "list.h"

#define dprintf(x...) do { \
        fprintf(stderr, "%s:%s:%d ", __FILE__, __FUNCTION__, __LINE__); \
        fprintf(stderr, x); \
        fprintf(stderr, "\n"); \
	} while(0)

#define DEMUXFS_SUPER_MAGIC 0xaa55

#define read_lock() do { } while(0)
#define read_unlock() do { } while(0)
#define write_lock() do { } while(0)
#define write_unlock() do { } while(0)

struct input_parser;

struct xattr {
	/* Extended attribute name */
	char *name;
	/* Extended attribute value */
	char *value;
	size_t size;
	/* Should name+value be freed, putname is set to true */
	bool putname;
	/* Private */
	struct list_head list;
};

struct dentry {
	/* The inode number, which is generated from the transport stream PID and the table_id */
	ino_t inode;
	/* File name */
	char *name;
	/* Mode (file, symlink, directory) */
	mode_t mode;
	/* Reference count to this dentry (used to mimic FIFO behavior in special files) */
	uint32_t refcount;
	/* Contents from FIFO files */
	struct fifo *fifo;
	/* File contents */
	char *contents;
	size_t size;
	/* Extended attributes */
	struct list_head xattrs;
	/* Protection for concurrent access */
	pthread_mutex_t mutex;
	pthread_cond_t condition;
	/* Backpointer to parent */
	struct dentry *parent;
	/* List of children dentries, if this dentry happens to represent a directory */
	struct list_head children;
	/* Private */
	struct list_head list;
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

/* Maximum number of TS packets to keep in a FIFO (1MB) */
//#define MAX_TS_PACKETS_IN_A_FIFO 5698
#define MAX_TS_PACKETS_IN_A_FIFO 682663 /* 128mb */

enum transmission_type {
	SBTVD_STANDARD,
	ATSC_STANDARD,
	ISDB_STANDARD,
	DVB_STANDARD,
};

struct descriptor;

struct user_options {
	bool parse_pes;
	enum transmission_type standard;
};

struct demuxfs_data {
	/* "table" holds PSI structures (ie: PAT, PMT, NIT..) */
	struct hash_table *table;
	/* "psi_parsers" holds pointers to parsers of known PSI PIDs */
	struct hash_table *psi_parsers;
	/* "pes_parsers" holds pointers to parsers of known PES PIDs */
	struct hash_table *pes_parsers;
	/* "ts_descriptors" holds descriptor tags and the tables that they're allowed to be in */
	struct descriptor *ts_descriptors;
	/* The root dentry ("/") */
	struct dentry *root;
	/* This filesystem's instance mountpoint */
	char *mountpoint;
	/* Backend specific data */
	struct input_parser *parser;
	/* General data shared amongst table parsers and descriptor parsers */
	void *shared_data;
	/* User-defined options */
	struct user_options options;
};

/* Backend operations */
struct backend_ops {
    int (*create)(struct fuse_args *, struct demuxfs_data *);
    int (*destroy)(struct demuxfs_data *);
    int (*read)(struct demuxfs_data *);
    int (*process)(struct demuxfs_data *);
    bool (*keep_alive)(struct demuxfs_data *);
};

#endif /* __demuxfs_h */
