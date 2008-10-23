dnl
dnl DemuxFS' configure.ac.
dnl Based on the work done by Iuri Gomes Diniz.
dnl
AC_INIT([DEMUXFS], [0.1], [lucasvr@gobolinux.org])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC_C99
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
	AC_MSG_ERROR([pkg-config wasn't found! Please install from your vendor, or see http://pkg-config.freedesktop.org/wiki/])
fi
PKG_CHECK_MODULES([FUSE_MODULE], 
	[fuse >= 2.6.0], ,
	[ AC_MSG_ERROR([FUSE >= 2.6.0 was not found. Please fetch it from http://fuse.sf.net]) ]
)
FUSE_LIBS=`$PKG_CONFIG --libs fuse`
FUSE_CFLAGS=`$PKG_CONFIG --cflags fuse`


dnl
dnl Check for CE2110 headers (optional)
dnl
AC_ARG_WITH(pdkroot, [  --with-pdkroot[[=PATH]]   Path to CE2110 PDK (default=not set)])
if [ test ! -z "${with_pdkroot}" ]
then
	PDK_ROOT="${with_pdkroot}"
	PDK_CFLAGS="-I${PDK_ROOT}/include -I${PDK_ROOT}/include/linux_user -I${PDK_ROOT}/include -I${PDK_ROOT}/../edl/include"
	PDK_LDFLAGS="-L${PDK_ROOT}/lib -L${PDK_ROOT}/../edl/lib"
	AC_SUBST(PDK_ROOT)
	AC_SUBST(PDK_CFLAGS)
	AC_SUBST(PDK_LDFLAGS)
fi


dnl
dnl Select backend. Available options are "filesrc" and "ce2110-gst".
dnl
backend="filesrc"
AC_ARG_WITH(backend, [  --with-backend[[=filesrc|=ce2110-gst]]   (default=filesrc)])
case "${with_backend}" in
	"" | "filesrc") backend="filesrc"    ;;
	"ce2110-gst")   backend="ce2110-gst" ;;
	*)              AC_MSG_ERROR([invalid option ${with_backend}])
esac

if [ test "${backend}" = "filesrc" ]
then
	dnl
	dnl set USE_FILESRC
	dnl
	CFLAGS="${CFLAGS} -DUSE_FILESRC"
else
	dnl
	dnl Check for GLib
	dnl
	AM_PATH_GLIB_2_0(2.8.0, [true], [AC_MSG_ERROR([cannot find glib])], [gmodule gobject gthread])
	
	dnl
	dnl Check for Gstreamer
	dnl
	PKG_CHECK_MODULES([GST], [gstreamer-0.10], [have_gst="true"], AC_MSG_ERROR([cannot find gstreamer]))

	dnl
	dnl Check for apm_psd_user library
	dnl
	CPPFLAGS_save="${CPPFLAGS}"
	CPPFLAGS="${CPPFLAGS} ${PDK_CFLAGS}"
	AC_CHECK_HEADERS([ismd_audio_defs.h], 
		[have_apm_psd_user="true"],
		AC_MSG_ERROR([ismd_audio_defs.h was not found! Please check the directory passed to '--with-pdkroot='.]))
	CPPFLAGS="${CPPFLAGS_save}"

	LDFLAGS_save="${LDFLAGS}"
	APM_PSD_USER_LIBS="${PDK_LDFLAGS} -lapm_psd_user -lsmd_user_lib_new"
	LDFLAGS="${LDFLAGS} ${APM_PSD_USER_LIBS}"
	AC_CHECK_LIB([apm_psd_user -lsmd_user_lib_new], [ismd_audio_stream_open], 
		[have_apm_psd_user=true], 
		AC_MSG_ERROR([libapm_psd_user was not found! Please check the directory passed to '--with-pdkroot='.]))
	LDFLAGS="${LDFLAGS_save}"
	AC_SUBST(APM_PSD_USER_LIBS)

	dnl
	dnl Check for idl_i2c library
	dnl
	CPPFLAGS_save="${CPPFLAGS}"
	CPPFLAGS="${CPPFLAGS} ${PDK_CFLAGS}"

	AC_CHECK_HEADERS([idl_i2c.h], 
		[have_idl_i2c=true], 
		AC_MSG_ERROR([idl_i2c.h was not found! Please check the directory passed to '--with-pdkroot='.]))

	LDFLAGS_save="${LDFLAGS}"
	IDL_I2C_LIBS="${PDK_LDFLAGS} -lidlerror -lidl_i2c -lidl_gpio -losal -lpthread -lsven_linux_user"
	LDFLAGS="${LDFLAGS} ${IDL_I2C_LIBS}"
	AC_CHECK_LIB([idl_i2c], 
		[idl_i2c_open], 
		[have_idl_i2c=true], 
		AC_MSG_ERROR([libidl_i2c was not found! Please check the directory passed to '--with-pdkroot='.]))
	LDFLAGS="${LDFLAGS_save}"
	AC_SUBST(IDL_I2C_LIBS)
	CPPFLAGS="${CPPFLAGS_save}"

	dnl
	dnl Check for edl_rtc library
	dnl
	CPPFLAGS_save="${CPPFLAGS}"
	CPPFLAGS="${CPPFLAGS} ${PDK_CFLAGS}"
	AC_CHECK_HEADERS([edl_rtc.h], 
		[have_edl_rtc=true], 
		AC_MSG_ERROR([edl_rtc.h was not found! Please check the directory passed to '--with-pdkroot='.]))
	CPPFLAGS="${CPPFLAGS_save}"
	EDL_RTC_LIBS="${PDK_LDFLAGS} -ledl_rtc -losal_linux_user -lpal -lpthread"
	AC_SUBST(EDL_RTC_LIBS)

	dnl
	dnl set USE_CE2110_GST
	dnl
	CFLAGS="${CFLAGS} -DUSE_CE2110_GST"
fi
AM_CONDITIONAL(USE_FILESRC, test "${backend}" = "filesrc")
AM_CONDITIONAL(USE_CE2110_GST, test "${backend}" = "ce2110-gst")


dnl
dnl Update flags
dnl
CFLAGS="${CFLAGS} ${FUSE_CFLAGS} -ggdb -O0 -Wall -std=c99"
LDFLAGS="${LDFLAGS} ${FUSE_LIBS}"


dnl
dnl Output files
dnl
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
    Makefile
    src/Makefile
	src/backends/Makefile
	src/descriptors/Makefile
	src/tables/Makefile
])
AC_OUTPUT
