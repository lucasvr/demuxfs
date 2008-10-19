#ifndef __ce2110_h
#define __ce2110_h

#include <gst/gst.h>

/* Maximum UNIX process priority */
#define MAX_PRIORITY -20

#ifndef GST_MESSAGE_PSI_PACKET
#define GST_MESSAGE_PSI_PACKET 1001
#endif

int ce2110_create_parser(struct fuse_args *args, struct demuxfs_data *priv);
int ce2110_destroy_parser(struct demuxfs_data *priv);
int ce2110_read_packet(struct demuxfs_data *priv);
int ce2110_process_packet(struct demuxfs_data *priv);
bool ce2110_keep_alive(struct demuxfs_data *priv);

struct backend_ops ce2110_backend_ops;

#endif /* __ce2110_h */
