#ifndef __fsutils_h
#define __fsutils_h

#define FS_PES_FIFO_NAME    "PES"
#define FS_PAT_NAME         "PAT"
#define FS_PMT_NAME         "PMT"
#define FS_NIT_NAME         "NIT"
#define FS_EIT_NAME         "EIT"
#define FS_PROGRAMS_NAME    "Programs"
#define FS_STREAMS_NAME     "Streams"

char *fsutils_path_walk(struct dentry *dentry, char *buf, size_t size);
void fsutils_dump_tree(struct dentry *dentry);
struct dentry *fsutils_get_child(struct dentry *dentry, char *name);
struct dentry *fsutils_get_dentry(struct dentry *root, const char *path);
struct dentry *fsutils_create_dentry(const char *path, mode_t mode);

/* Macros to ease the creation of files and directories */
#define CREATE_COMMON(_parent,_dentry) \
		INIT_LIST_HEAD(&(_dentry)->children); \
		INIT_LIST_HEAD(&(_dentry)->xattrs); \
		pthread_mutex_init(&(_dentry)->mutex, NULL); \
		pthread_cond_init(&(_dentry)->condition, NULL); \
		(_dentry)->parent = _parent; \
		list_add_tail(&(_dentry)->list, &((_parent)->children));

#define CREATE_FILE_NUMBER(parent,header,member) \
	({ \
		struct dentry *_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
		asprintf(&_dentry->contents, "%#04x", (header)->member); \
		_dentry->name = strdup(#member); \
		_dentry->size = strlen(_dentry->contents); \
		_dentry->mode = S_IFREG | 0444; \
		CREATE_COMMON((parent),_dentry); \
		xattr_add(_dentry, XATTR_FORMAT, XATTR_FORMAT_NUMBER, strlen(XATTR_FORMAT_NUMBER), false); \
	 	_dentry; \
	})

#define CREATE_FILE_STRING(parent,header,member,fmt) \
	({ \
		struct dentry *_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
		_dentry->contents = strdup((header)->member); \
		_dentry->name = strdup(#member); \
		_dentry->size = strlen(_dentry->contents); \
		_dentry->mode = S_IFREG | 0444; \
		CREATE_COMMON((parent),_dentry); \
		xattr_add(_dentry, XATTR_FORMAT, fmt, strlen(fmt), false); \
	 	_dentry; \
	})

#define CREATE_SYMLINK(parent,sname,target) \
	({ \
		struct dentry *_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
		_dentry->contents = strdup(target); \
		_dentry->name = strdup(sname); \
		_dentry->mode = S_IFLNK | 0777; \
		CREATE_COMMON((parent),_dentry); \
	 	_dentry; \
	})

#define CREATE_FIFO(parent,fname) \
	({ \
		struct dentry *_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
		_dentry->name = strdup(fname); \
		_dentry->mode = S_IFREG | 0777; \
		_dentry->fifo = fifo_init(MAX_TS_PACKETS_IN_A_FIFO); \
		CREATE_COMMON((parent),_dentry); \
	 	_dentry; \
	})

#define CREATE_DIRECTORY(parent,dname) \
	({ \
		struct dentry *_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
		_dentry->name = strdup(dname); \
		_dentry->mode = S_IFDIR | 0555; \
		CREATE_COMMON((parent),_dentry); \
	 	_dentry; \
	})


#endif /* __fsutils_h */
