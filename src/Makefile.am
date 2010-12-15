noinst_HEADERS = demuxfs.h ts.h snapshot.h fsutils.h hash.h xattr.h fifo.h buffer.h list.h byteops.h crc32.h backend.h

# DemuxFS Library
noinst_LTLIBRARIES = libdemuxfs.la
libdemuxfs_la_SOURCES = demuxfs.c ts.c snapshot.c fsutils.c hash.c xattr.c buffer.c crc32.c
#if USE_POSIX_FIFOS
#libdemuxfs_la_SOURCES += fifo_posix.c
#else
libdemuxfs_la_SOURCES += fifo_generic.c
#endif
libdemuxfs_la_DEPENDENCIES = tables/libtables.la 
libdemuxfs_la_LIBADD = tables/libtables.la 

# Executable
bin_PROGRAMS = demuxfs
demuxfs_SOURCES = main.c backend.c
demuxfs_DEPENDENCIES = libdemuxfs.la
demuxfs_LDADD = libdemuxfs.la
demuxfs_CPPFLAGS = -I${top_srcdir}/src/backends -I${top_srcdir}/src/tables -DLIBDIR="\"@libdir@\""

SUBDIRS = dsm-cc tables backends
