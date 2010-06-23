#ifndef __backend_h
#define __backend_h

#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include "ts.h"

/* Backend operations */
struct backend_ops {
    int (*create)(struct fuse_args *, struct demuxfs_data *);
    int (*destroy)(struct demuxfs_data *);
	int (*set_frequency)(uint32_t, struct demuxfs_data *);
    int (*read)(struct demuxfs_data *);
	int (*process)(struct ts_header *, void **, struct demuxfs_data *);
    bool (*keep_alive)(struct demuxfs_data *);
	void (*usage)(void);
};

struct backend_ops *backend_load(const char *backend_name, void **backend_handle);
void backend_unload(void *backend_handle);
void backend_print_usage(void);

#endif /* __backend_h */
