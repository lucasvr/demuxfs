#ifndef __filesrc_h
#define __filesrc_h

#ifdef USE_FILESRC

#define _GNU_SOURCE
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <getopt.h>
#include <arpa/inet.h>

int filesrc_create_parser(struct fuse_args *args, struct demuxfs_data *priv);
int filesrc_destroy_parser(struct demuxfs_data *priv);
int filesrc_read_packet(struct demuxfs_data *priv);
int filesrc_process_packet(struct demuxfs_data *priv);
bool filesrc_keep_alive(struct demuxfs_data *priv);

struct backend_ops filesrc_backend_ops;

#endif /* USE_FILESRC */

#endif /* __filesrc_h */
