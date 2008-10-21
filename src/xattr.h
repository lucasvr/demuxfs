#ifndef __xattr_h
#define __xattr_h

#ifndef ENOATTR
#define ENOATTR ENODATA
#endif

#define XATTR_FORMAT                    "system.format"
#define XATTR_FORMAT_NUMBER             "number"
#define XATTR_FORMAT_STRING             "string"
#define XATTR_FORMAT_STRING_AND_NUMBER  "string (number)"

struct xattr *xattr_get(struct dentry *dentry, const char *name);
bool xattr_exists(struct dentry *dentry, const char *name);
int xattr_add(struct dentry *dentry, const char *name, const char *value, size_t size, bool putname);
int xattr_list(struct dentry *dentry, char *buf, size_t size);
int xattr_remove(struct dentry *dentry, const char *name);

#endif /* __xattr_h */