bin_PROGRAMS = demuxfs

demuxfsSources = demuxfs.c ts.c fsutils.c hash.c
demuxfsHeaders = demuxfs.h ts.h fsutils.h hash.h list.h byteops.h

demuxfs_SOURCES = $(demuxfsSources) $(demuxfsHeaders)
demuxfs_DEPENDENCIES = descriptors/libdescriptors.la tables/libtables.la
demuxfs_LDADD = descriptors/libdescriptors.la tables/libtables.la
demuxfs_CPPFLAGS = 

if USE_FILESRC
demuxfs_SOURCES += filesrc.c filesrc.h
endif

if USE_CE2110_GST
demuxfs_SOURCES  += ce2110.c ce2110.h
demuxfs_CPPFLAGS += @GLIB_CFLAGS@ @OLO_CFLAGS@  @GST_CFLAGS@
demuxfs_LDADD   += @GLIB_LIBS@ @IDL_I2C_LIBS@ @EDL_RTC_LIBS@ @GST_LIBS@
endif

SUBDIRS = descriptors tables
