noinst_LTLIBRARIES = libdsmcc_descriptors.la

parsers = \
	parser_0x01.c \
    parser_0x02.c \
    parser_0x03.c \
    parser_0x04.c \
    parser_0x05.c \
    parser_0x06.c \
    parser_0x07.c \
	parser_0xc2.c

libdsmcc_descriptors_la_SOURCES  = descriptors.c broadcaster.c $(parsers)
libdsmcc_descriptors_la_SOURCES += descriptors.h

AM_CPPFLAGS = -I. -I../.. -I../../backends
