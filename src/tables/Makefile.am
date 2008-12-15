noinst_LTLIBRARIES = libtables.la

libtables_la_SOURCES  = psi.c pat.c pmt.c nit.c pes.c
libtables_la_SOURCES += psi.h pat.h pmt.h nit.h pes.h
libtables_la_DEPENDENCIES = ../descriptors/libdescriptors.la
libtables_la_LIBADD = ../descriptors/libdescriptors.la

AM_CPPFLAGS = -I. -I.. -I../backends -I../descriptors
