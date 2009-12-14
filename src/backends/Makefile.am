noinst_LTLIBRARIES = libbackends.la

libbackends_la_SOURCES =
libbackends_la_CPPFLAGS = -I. -I..

if USE_FILESRC
libbackends_la_SOURCES += filesrc.c filesrc.h
endif

if USE_LINUXDVB
libbackends_la_SOURCES += linuxdvb.c linuxdvb.h
endif
