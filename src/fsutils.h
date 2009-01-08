#ifndef __fsutils_h
#define __fsutils_h

#define FS_ES_FIFO_NAME                 "ES"
#define FS_PES_FIFO_NAME                "PES"
#define FS_PAT_NAME                     "PAT"
#define FS_PMT_NAME                     "PMT"
#define FS_NIT_NAME                     "NIT"
#define FS_EIT_NAME                     "EIT"
#define FS_SDT_NAME                     "SDT"
#define FS_SDTT_NAME                    "SDTT"
#define FS_TOT_NAME                     "TOT"
#define FS_DII_NAME                     "DII"
#define FS_DDB_NAME                     "DDB"

#define FS_PROGRAMS_NAME                "Programs"
#define FS_CURRENT_NAME                 "Current"
#define FS_PRIMARY_NAME                 "Primary"
#define FS_SECONDARY_NAME               "Secondary"

#define FS_VIDEO_SNAPSHOT_NAME          "snapshot.ppm"
#define FS_STREAMS_NAME                 "Streams"
#define FS_AUDIO_STREAMS_NAME           "AudioStreams"
#define FS_VIDEO_STREAMS_NAME           "VideoStreams"
#define FS_ONE_SEG_AUDIO_STREAMS_NAME   "AudioStreams-OneSeg"
#define FS_ONE_SEG_VIDEO_STREAMS_NAME   "VideoStreams-OneSeg"
#define FS_CLOSED_CAPTION_STREAMS_NAME  "ClosedCaptionStreams"
#define FS_SUPERIMPOSED_STREAMS_NAME    "SuperImposedStreams"
#define FS_OBJECT_CAROUSEL_STREAMS_NAME "ObjectCarouselStreams"
#define FS_DATA_CAROUSEL_STREAMS_NAME   "DataCarouselStreams"
#define FS_EVENT_MESSAGE_STREAMS_NAME   "EventMessageStreams"
#define FS_MPE_STREAMS_NAME             "MPEStreams"
#define FS_RESERVED_STREAMS_NAME        "ReservedStreams"

char *fsutils_path_walk(struct dentry *dentry, char *buf, size_t size);
void fsutils_dump_tree(struct dentry *dentry);
struct dentry *fsutils_get_child(struct dentry *dentry, const char *name);
struct dentry *fsutils_get_dentry(struct dentry *root, const char *path);
struct dentry *fsutils_create_dentry(const char *path, mode_t mode);
struct dentry * fsutils_create_version_dir(struct dentry *parent, int version);
void fsutils_dispose_tree(struct dentry *dentry);
void fsutils_dispose_node(struct dentry *dentry);
void fsutils_migrate_children(struct dentry *source, struct dentry *target);

/* Macros to ease the creation of files and directories */
#define CREATE_COMMON(_parent,_dentry) \
		INIT_LIST_HEAD(&(_dentry)->children); \
		INIT_LIST_HEAD(&(_dentry)->xattrs); \
		pthread_mutex_init(&(_dentry)->mutex, NULL); \
		sem_init(&(_dentry)->semaphore, 0, 0); \
		(_dentry)->parent = _parent; \
		list_add_tail(&(_dentry)->list, &((_parent)->children));

#define UPDATE_COMMON(_dentry,_new_contents,_new_size) \
 		pthread_mutex_lock(&_dentry->mutex); \
 		if (_dentry->size != _new_size) { \
 			free(_dentry->contents); \
			_dentry->contents = malloc(_new_size); \
			memcpy(_dentry->contents, _new_contents, _new_size); \
 			_dentry->size = _new_size; \
 		} else \
 			memcpy(_dentry->contents, _new_contents, _dentry->size); \
 		pthread_mutex_unlock(&_dentry->mutex);

#define CREATE_FILE_BIN(parent,header,member,_size) \
	({ \
	 	struct dentry *_dentry = fsutils_get_child(parent, #member); \
	 	if (_dentry) { \
	 		UPDATE_COMMON(_dentry, (header)->member, _size); \
		} else { \
	 		_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
	 		_dentry->contents = malloc(_size); \
	 		memcpy(_dentry->contents, (header)->member, _size); \
			_dentry->name = strdup((char *) #member); \
			_dentry->size = _size; \
			_dentry->mode = S_IFREG | 0444; \
			CREATE_COMMON((parent),_dentry); \
			xattr_add(_dentry, XATTR_FORMAT, XATTR_FORMAT_BIN, strlen(XATTR_FORMAT_BIN), false); \
	 	} \
	 	_dentry; \
	})

#define CREATE_FILE_NUMBER(parent,header,member) \
	({ \
	 	uint64_t member64 = (uint64_t) (header)->member; \
	 	struct dentry *_dentry = fsutils_get_child(parent, #member); \
	 	if (_dentry) { \
	 		pthread_mutex_lock(&_dentry->mutex); \
	 		free(_dentry->contents); \
			asprintf(&_dentry->contents, "%#04llx", member64); \
			_dentry->size = strlen(_dentry->contents); \
	 		pthread_mutex_unlock(&_dentry->mutex); \
	 	} else { \
			_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
			asprintf(&_dentry->contents, "%#04llx", member64); \
			_dentry->name = strdup(#member); \
			_dentry->size = strlen(_dentry->contents); \
			_dentry->mode = S_IFREG | 0444; \
			CREATE_COMMON((parent),_dentry); \
			xattr_add(_dentry, XATTR_FORMAT, XATTR_FORMAT_NUMBER, strlen(XATTR_FORMAT_NUMBER), false); \
	 	} \
	 	_dentry; \
	})

#define CREATE_FILE_STRING(parent,header,member,fmt) \
	({ \
	 	struct dentry *_dentry = fsutils_get_child(parent, #member); \
	 	if (_dentry) { \
	 		UPDATE_COMMON(_dentry, (header)->member, strlen((header)->member)); \
	 	} else { \
			_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
			_dentry->contents = strdup((header)->member); \
			_dentry->name = strdup(#member); \
			_dentry->size = strlen(_dentry->contents); \
			_dentry->mode = S_IFREG | 0444; \
			CREATE_COMMON((parent),_dentry); \
			xattr_add(_dentry, XATTR_FORMAT, fmt, strlen(fmt), false); \
	 	} \
	 	_dentry; \
	})

#define CREATE_SYMLINK(parent,sname,target) \
	({ \
	 	struct dentry *_dentry = fsutils_get_child(parent, sname); \
	 	if (! _dentry) { \
			struct dentry *_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
			_dentry->contents = strdup(target); \
			_dentry->name = strdup(sname); \
			_dentry->mode = S_IFLNK | 0777; \
			CREATE_COMMON((parent),_dentry); \
	 	} \
	 	_dentry; \
	})

#define CREATE_SNAPSHOT_FILE(parent,fname,pes_dentry) \
	({ \
	 	struct dentry *_dentry = fsutils_get_child(parent, fname); \
	 	if (! _dentry) { \
			_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
			_dentry->name = strdup(fname); \
			_dentry->mode = S_IFREG | 0444; \
	 		_dentry->borrowed_pes_dentry = pes_dentry; \
			CREATE_COMMON((parent),_dentry); \
	 	} \
	 	_dentry; \
	})

#define CREATE_FIFO(parent,fname) \
	({ \
	 	struct dentry *_dentry = fsutils_get_child(parent, fname); \
	 	if (! _dentry) { \
	 		_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
	 		_dentry->name = strdup(fname); \
	 		_dentry->mode = S_IFREG | 0777; \
	 		_dentry->fifo = fifo_init(MAX_TS_PACKETS_IN_A_FIFO); \
	 		CREATE_COMMON((parent),_dentry); \
	 	} \
	 	_dentry; \
	})

#define CREATE_DIRECTORY(parent,dname) \
	({ \
	 	struct dentry *_dentry = fsutils_get_child(parent, dname); \
	 	if (! _dentry) { \
			_dentry = (struct dentry *) calloc(1, sizeof(struct dentry)); \
			_dentry->name = strdup(dname); \
			_dentry->mode = S_IFDIR | 0555; \
			CREATE_COMMON((parent),_dentry); \
	 	} \
	 	_dentry; \
	})

#endif /* __fsutils_h */
