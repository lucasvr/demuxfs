bin_PROGRAMS = demuxfs

demuxfsSources = demuxfs.c ts.c fsutils.c hash.c xattr.c fifo.c
demuxfsHeaders = demuxfs.h ts.h fsutils.h hash.h xattr.h fifo.h list.h byteops.h

demuxfs_SOURCES = $(demuxfsSources) $(demuxfsHeaders)
demuxfs_DEPENDENCIES = backends/libbackends.la descriptors/libdescriptors.la tables/libtables.la
demuxfs_LDADD = backends/libbackends.la descriptors/libdescriptors.la tables/libtables.la
demuxfs_CPPFLAGS = -I backends

SUBDIRS = backends descriptors tables
