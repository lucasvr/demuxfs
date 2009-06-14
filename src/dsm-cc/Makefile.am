noinst_LTLIBRARIES = libdsmcc.la

libdsmcc_la_SOURCES  = ait.c dii.c ddb.c dsmcc.c
libdsmcc_la_SOURCES += ait.h dii.h ddb.h dsmcc.h
libdsmcc_la_DEPENDENCIES = descriptors/libdsmcc_descriptors.la
libdsmcc_la_LIBADD = descriptors/libdsmcc_descriptors.la

AM_CPPFLAGS = -I. -I.. -I../backends -Idescriptors

SUBDIRS = descriptors
