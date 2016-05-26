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
#include <fuse_lowlevel.h>
#include <fuse.h>

#include "list.h"
#include "priv.h"
#include "colors.h"

#define dprintf(x...) do { \
        fprintf(stderr, x); \
        fprintf(stderr, colorGray " (%s:%s:%d)" colorNormal, __FILE__, __FUNCTION__, __LINE__); \
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
	ssize_t size;
	/* Should name+value be freed, putname is set to true */
	bool putname;
	/* Private */
	struct list_head list;
};

enum {
	OBJ_TYPE_FILE        = (1 << 0),
	OBJ_TYPE_DIR         = (1 << 1),
	OBJ_TYPE_SYMLINK     = (1 << 2),
	OBJ_TYPE_FIFO        = (1 << 3),
	OBJ_TYPE_AUDIO_FIFO  = (1 << 4) | OBJ_TYPE_FIFO,
	OBJ_TYPE_VIDEO_FIFO  = (1 << 5) | OBJ_TYPE_FIFO,
	OBJ_TYPE_SNAPSHOT    = (1 << 6),
};

#define DEMUXFS_IS_FILE(d)       (d->obj_type == OBJ_TYPE_FILE)
#define DEMUXFS_IS_DIR(d)        (d->obj_type == OBJ_TYPE_DIR)
#define DEMUXFS_IS_SYMLINK(d)    (d->obj_type == OBJ_TYPE_SYMLINK)
#define DEMUXFS_IS_AUDIO_FIFO(d) (d->obj_type == OBJ_TYPE_AUDIO_FIFO)
#define DEMUXFS_IS_VIDEO_FIFO(d) (d->obj_type == OBJ_TYPE_VIDEO_FIFO)
#define DEMUXFS_IS_SNAPSHOT(d)   (d->obj_type == OBJ_TYPE_SNAPSHOT)

struct dentry {
	/* The inode number, generated from the transport stream PID and the table_id */
	ino_t inode;
	/* File name */
	char *name;
	/* UNIX mode (file, symlink, directory) */
	mode_t mode;
	/* Timestamps */
	time_t atime;
	time_t ctime;
	time_t mtime;
	/* DemuxFS object type (FIFO, snapshot, regular file, directory) */
	int obj_type;
	/* Reference count */
	uint32_t refcount;
	/* File contents */
	char *contents;
	ssize_t size;

	/* Extended attributes */
	struct list_head xattrs;
	/* Protection for concurrent access */
	pthread_mutex_t mutex;
	/* Backpointer to parent */
	struct dentry *parent;
	/* List of children dentries, if any */
	struct list_head children;
	/* List in which this dentry is linked in */
	struct list_head list;

	/* Private data */
	void *priv;
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

enum transmission_type {
	SBTVD_STANDARD,
	ATSC_STANDARD,
	ISDB_STANDARD,
	DVB_STANDARD,
};

/* Error types the user wants to see reported to stderr */
enum error_type {
	CRC_ERROR        = 1,
	CONTINUITY_ERROR = 2,
	ALL_ERRORS       = 0xff,
};

struct descriptor;
struct dsmcc_descriptor;
struct backend_ops;

struct user_options {
	bool parse_pes;
	uint8_t packet_size;
	uint8_t packet_error_correction_bytes;
	enum transmission_type standard;
	uint32_t frequency;
	char *tmpdir;
	enum error_type verbose_mask;
};

struct demuxfs_data {
    /* command line options */
	bool opt_parse_pes;
	char *opt_standard;
	char *opt_tmpdir;
	char *opt_backend;
	char *opt_report;
	/* "psi_tables" holds PSI structures (ie: PAT, PMT, NIT..) */
	struct hash_table *psi_tables;
	/* "pes_tables" holds structures from PES packets that we're parsing */
	struct hash_table *pes_tables;
	/* "psi_parsers" holds pointers to parsers of known PSI PIDs */
	struct hash_table *psi_parsers;
	/* "pes_parsers" holds pointers to parsers of known PES PIDs */
	struct hash_table *pes_parsers;
	/* "packet_buffer" holds incomplete TS packets, which cannot be parsed yet */
	struct hash_table *packet_buffer;
	/* "ts_descriptors" holds descriptor tags and the tables that they're allowed to be in */
	struct descriptor *ts_descriptors;
	/* "dsmcc_descriptors" holds DSM-CC descriptor tags and their parsers */
	struct dsmcc_descriptor *dsmcc_descriptors;
	/* The root dentry ("/") */
	struct dentry *root;
	/* Backend specific data */
	struct input_parser *parser;
	/* General data shared amongst table parsers and descriptor parsers */
	void *shared_data;
	/* DemuxFS mount point */
	char *mount_point;
	/* TS parser thread handle */
	pthread_t ts_parser_id;
	/* User-defined options */
	struct user_options options;
	/* Backend implementation */
	struct backend_ops *backend;
};

#endif /* __demuxfs_h */
