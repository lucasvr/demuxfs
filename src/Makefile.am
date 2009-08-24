bin_PROGRAMS = demuxfs

demuxfsSources = main.c demuxfs.c ts.c snapshot.c fsutils.c hash.c xattr.c buffer.c crc32.c
noinst_HEADERS = demuxfs.h ts.h snapshot.h fsutils.h hash.h xattr.h fifo.h buffer.h list.h byteops.h crc32.h

if USE_POSIX_FIFOS
demuxfsSources += fifo_posix.c
else
demuxfsSources += fifo_generic.c
endif

demuxfs_SOURCES = $(demuxfsSources)
demuxfs_DEPENDENCIES = backends/libbackends.la tables/libtables.la
demuxfs_LDADD = backends/libbackends.la tables/libtables.la
demuxfs_CPPFLAGS = -I backends

SUBDIRS = backends dsm-cc tables
