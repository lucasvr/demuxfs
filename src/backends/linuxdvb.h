#ifndef __linuxdvb_h
#define __linuxdvb_h

#ifdef USE_LINUXDVB

#define _GNU_SOURCE
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/dvb/dmx.h>

int linuxdvb_create_parser(struct fuse_args *args, struct demuxfs_data *priv);
int linuxdvb_destroy_parser(struct demuxfs_data *priv);
int linuxdvb_read_packet(struct demuxfs_data *priv);
int linuxdvb_process_packet(struct demuxfs_data *priv);
bool linuxdvb_keep_alive(struct demuxfs_data *priv);

struct backend_ops linuxdvb_backend_ops;

#endif /* USE_LINUXDVB */

#endif /* __linuxdvb_h */
