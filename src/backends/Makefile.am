noinst_LTLIBRARIES = libbackends.la

libbackends_la_SOURCES =
libbackends_la_CPPFLAGS = -I. -I..

if USE_FILESRC
libbackends_la_SOURCES += filesrc.c filesrc.h
endif

if USE_LINUXDVB
libbackends_la_SOURCES += linuxdvb.c linuxdvb.h
endif

if USE_CE2110_GST
libbackends_la_SOURCES  += ce2110.c ce2110.h
libbackends_la_CPPFLAGS += @GLIB_CFLAGS@ @OLO_CFLAGS@ @GST_CFLAGS@
libbackends_la_LIBADD    = @GLIB_LIBS@ @IDL_I2C_LIBS@ @EDL_RTC_LIBS@ @GST_LIBS@
endif
