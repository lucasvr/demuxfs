noinst_LTLIBRARIES = libtables.la

libtables_la_SOURCES  = psi.c pat.c pmt.c nit.c pes.c sdt.c sdtt.c tot.c
libtables_la_SOURCES += psi.h pat.h pmt.h nit.h pes.h sdt.h sdtt.c tot.h
libtables_la_DEPENDENCIES = ../descriptors/libdescriptors.la ../dsm-cc/libdsmcc.la
libtables_la_LIBADD = ../descriptors/libdescriptors.la ../dsm-cc/libdsmcc.la

AM_CPPFLAGS = -I. -I.. -I../backends -I../descriptors -I../dsm-cc
