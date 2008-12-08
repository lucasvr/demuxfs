#ifndef __fsutils_h
#define __fsutils_h

#define FS_PES_FIFO_NAME    "PES"
#define FS_PAT_NAME         "PAT"
#define FS_PMT_NAME         "PMT"
#define FS_NIT_NAME         "NIT"
#define FS_DESCRIPTORS_NAME "Descriptors"
#define FS_STREAMS_NAME     "Streams"

char *fsutils_path_walk(struct dentry *dentry, char *buf, size_t size);
void fsutils_dump_tree(struct dentry *dentry, int spaces);
struct dentry *fsutils_has_children(struct dentry *dentry, char *name);
struct dentry *fsutils_get_dentry(struct dentry *root, const char *path);
struct dentry *fsutils_create_dentry(const char *path, mode_t mode);

#endif /* __fsutils_h */
