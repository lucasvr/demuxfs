#ifndef __fsutils_h
#define __fsutils_h

char *fsutils_path_walk(struct dentry *dentry, char *buf, size_t size);
void fsutils_dump_tree(struct dentry *dentry, int spaces);
struct dentry *fsutils_has_children(struct dentry *dentry, char *name);
struct dentry *fsutils_get_dentry(struct dentry *root, const char *path);
struct dentry *fsutils_create_dentry(const char *path, mode_t mode);

#endif /* __fsutils_h */
