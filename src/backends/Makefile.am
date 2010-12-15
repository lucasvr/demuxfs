libdir = @libdir@/demuxfs/backends

lib_LTLIBRARIES = 

if USE_FILESRC
lib_LTLIBRARIES += libfilesrc.la
libfilesrc_la_SOURCES = filesrc.c filesrc.h
libfilesrc_la_CPPFLAGS = -I${top_srcdir}/src/backends -I${top_srcdir}/src
endif

if USE_LINUXDVB
lib_LTLIBRARIES += liblinuxdvb.la
liblinuxdvb_la_SOURCES = linuxdvb.c linuxdvb.h
liblinuxdvb_la_CPPFLAGS = -I${top_srcdir}/src/backends -I${top_srcdir}/src
endif
