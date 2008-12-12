bin_PROGRAMS = demuxfs

demuxfsSources = demuxfs.c ts.c fsutils.c hash.c xattr.c fifo.c buffer.c
noinst_HEADERS = demuxfs.h ts.h fsutils.h hash.h xattr.h fifo.h buffer.h list.h byteops.h

demuxfs_SOURCES = $(demuxfsSources)
demuxfs_DEPENDENCIES = backends/libbackends.la descriptors/libdescriptors.la tables/libtables.la
demuxfs_LDADD = backends/libbackends.la descriptors/libdescriptors.la tables/libtables.la
demuxfs_CPPFLAGS = -I backends -I descriptors

SUBDIRS = backends descriptors tables
