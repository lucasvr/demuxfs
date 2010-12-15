noinst_LTLIBRARIES = libtables.la

libtables_la_SOURCES  = psi.c pat.c pmt.c nit.c pes.c sdt.c sdtt.c tot.c eit.c
libtables_la_SOURCES += psi.h pat.h pmt.h nit.h pes.h sdt.h sdtt.c tot.h eit.h
libtables_la_DEPENDENCIES = descriptors/libdescriptors.la ../dsm-cc/libdsmcc.la
libtables_la_LIBADD = descriptors/libdescriptors.la ../dsm-cc/libdsmcc.la

AM_CPPFLAGS = -I${top_srcdir}/src/tables -I${top_srcdir}/src -I${top_srcdir}/src/backends -I${top_srcdir}/src/dsm-cc -I${top_srcdir}/src/tables/descriptors

SUBDIRS = descriptors
