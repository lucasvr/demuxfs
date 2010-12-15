noinst_LTLIBRARIES = libdsmcc.la

libdsmcc_la_SOURCES  = ait.c dii.c dsi.c ddb.c dsmcc.c biop.c iop.c
libdsmcc_la_SOURCES += ait.h dii.h dsi.h ddb.h dsmcc.h biop.h iop.h
libdsmcc_la_DEPENDENCIES = descriptors/libdsmcc_descriptors.la
libdsmcc_la_LIBADD = descriptors/libdsmcc_descriptors.la

AM_CPPFLAGS = -I${top_srcdir}/src/dsm-cc -I${top_srcdir}/src -I${top_srcdir}/src/backends -I${top_srcdir}/src/dsm-cc/descriptors

SUBDIRS = descriptors
