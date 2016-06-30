dnl
dnl DemuxFS' configure.ac.
dnl Based on the work done by Iuri Gomes Diniz.
dnl
AC_INIT([DEMUXFS], [0.1], [lucasvr@gobolinux.org])
AC_CONFIG_AUX_DIR([build-aux])
AC_PROG_CC_C99
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_LIBTOOL

dnl
dnl Check for LibFUSE (mandatory)
dnl
AC_MSG_CHECKING([fuse compatibility])
if test -z "$PKG_CONFIG"
then
	AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
fi
if test "x${PKG_CONFIG}" = "xno"
then
	AC_MSG_ERROR([pkg-config was not found! Please install from your vendor, or see http://pkg-config.freedesktop.org/wiki/])
fi
PKG_CHECK_MODULES([FUSE_MODULE], 
	[fuse >= 2.6.0], ,
	[ AC_MSG_ERROR([FUSE >= 2.6.0 was not found. Please fetch it from http://fuse.sf.net]) ]
)
FUSE_LIBS=`$PKG_CONFIG --libs fuse`
FUSE_CFLAGS=`$PKG_CONFIG --cflags fuse`


dnl
dnl Check for FFMPEG (optional)
dnl
jpeg_found=
ffmpeg_found=
AC_MSG_CHECKING([ffmpeg])
PKG_CHECK_MODULES([LIBAVCODEC_MODULE], [libavcodec >= 52.7.0], ffmpeg_found="yes", 
				  AC_MSG_RESULT([Support for snapshots will be disabled.]))
if test ! -z "${ffmpeg_found}"
then
	dnl
	dnl Check for libavcodec
	dnl
	libavcodec=no
	AC_MSG_CHECKING([libavcodec])
	FFMPEG_LIBS=`$PKG_CONFIG --libs libavcodec`
	FFMPEG_CFLAGS=`$PKG_CONFIG --cflags libavcodec`
	PKG_CHECK_MODULES([LIBAVCODEC_MODULE], [libavcodec >= 0.7.1], libavcodec_found="yes", 
					  AC_MSG_RESULT([FFmpeg was compiled with no support for libavcodec.]))
	AVCODEC_LIBS=`$PKG_CONFIG --libs libavcodec`

	dnl
	dnl Check for libavutil
	dnl
	libavutil_found=no
	AC_MSG_CHECKING([libavutil])
	FFMPEG_LIBS=`$PKG_CONFIG --libs libavutil`
	FFMPEG_CFLAGS=`$PKG_CONFIG --cflags libavutil`
	PKG_CHECK_MODULES([LIBAVUTIL_MODULE], [libavutil >= 0.7.1], libavutil_found="yes", 
					  AC_MSG_RESULT([FFmpeg was compiled with no support for libavutil.]))
	AVUTIL_LIBS=`$PKG_CONFIG --libs libavutil`

	dnl
	dnl Check for libswscale
	dnl
	libswscale_found=no
	AC_MSG_CHECKING([libswscale])
	FFMPEG_LIBS=`$PKG_CONFIG --libs libavcodec`
	FFMPEG_CFLAGS=`$PKG_CONFIG --cflags libavcodec`
	PKG_CHECK_MODULES([LIBSWSCALE_MODULE], [libswscale >= 0.7.1], libswscale_found="yes", 
					  AC_MSG_RESULT([FFmpeg was compiled with no support for libswscale.]))
	SWSCALE_LIBS=`$PKG_CONFIG --libs libswscale`
	
	dnl
	dnl Check for libavformat
	dnl
	libavformat_found=no
	AC_MSG_CHECKING([libavformat])
	FFMPEG_LIBS=`$PKG_CONFIG --libs libavformat`
	FFMPEG_CFLAGS=`$PKG_CONFIG --cflags libavformat`
	PKG_CHECK_MODULES([LIBAVFORMAT_MODULE], [libavformat >= 0.7.1], libavformat_found="yes", 
					  AC_MSG_RESULT([FFmpeg libavformat was not found.]))
	AVFORMAT_LIBS=`$PKG_CONFIG --libs libavformat`

	if test "x$libswscale_found" != "xno" && test "x$libavformat_found" != "xno" &&
       test "x$libavutil_found" != "xno" && test "x$libavcodec_found" != "xno"
	then
		CFLAGS="${CFLAGS} -DUSE_FFMPEG"
	fi
fi
AM_CONDITIONAL(USE_FFMPEG, test "${ffmpeg_found}" = "yes")

dnl
dnl Select backend. Available options are "filesrc"  and "linuxdvb".
dnl
validbackend=false
AC_ARG_WITH(backend, [  --with-backend=[[filesrc|linuxdvb|all] (default=all)]])

use_filesrc=false
if test "${with_backend}" = "filesrc" -o "${with_backend}" = "all" -o "${with_backend}" = ""
then
	dnl
	dnl set USE_FILESRC
	dnl
	CFLAGS="${CFLAGS} -DUSE_FILESRC"
	use_filesrc=true
	validbackend=true
fi

use_linuxdvb=false
if test "${with_backend}" = "linuxdvb" -o "${with_backend}" = "all" -o "${with_backend}" = ""
then
	dnl
	dnl set USE_LINUXDVB
	dnl
	CFLAGS="${CFLAGS} -DUSE_LINUXDVB"
	use_linuxdvb=true
	validbackend=true

	AC_MSG_CHECKING([LinuxDVB API])
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM([[#include <linux/dvb/frontend.h>]],[[int x = SYS_DVBC_ANNEX_A;]])],
		[AC_MSG_RESULT(up-to-date)],
		[AC_MSG_RESULT(too old to be true!); use_linuxdvb=false]
	)
fi

AM_CONDITIONAL(USE_FILESRC, test "${use_filesrc}" = "true")
AM_CONDITIONAL(USE_LINUXDVB, test "${use_linuxdvb}" = "true")

if test -z "$validbackend"
then
	AC_MSG_ERROR([Invalid backend selected.])
fi

dnl
dnl Update flags
dnl
CFLAGS="${CFLAGS} ${FUSE_CFLAGS} ${FFMPEG_CFLAGS} -ggdb -O3 -Wall"
LDFLAGS="${LDFLAGS} ${FUSE_LIBS} ${FFMPEG_LIBS} ${AVCODEC_LIBS} ${AVUTIL_LIBS} ${SWSCALE_LIBS} ${AVFORMAT_LIBS}"

dnl
dnl Output files
dnl
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
    Makefile
    src/Makefile
	src/backends/Makefile
	src/dsm-cc/Makefile
	src/dsm-cc/descriptors/Makefile
	src/tables/Makefile
	src/tables/descriptors/Makefile
])
AC_OUTPUT
