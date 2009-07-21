bin_PROGRAMS = demuxfs

demuxfsSources = demuxfs.c ts.c snapshot.c fsutils.c hash.c xattr.c fifo.c buffer.c crc32.c
noinst_HEADERS = demuxfs.h ts.h snapshot.h fsutils.h hash.h xattr.h fifo.h buffer.h list.h byteops.h crc32.h

demuxfs_SOURCES = $(demuxfsSources)
demuxfs_DEPENDENCIES = backends/libbackends.la tables/libtables.la
demuxfs_LDADD = backends/libbackends.la tables/libtables.la
demuxfs_CPPFLAGS = -I backends

SUBDIRS = backends dsm-cc tables
