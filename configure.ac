AC_INIT([opennop], [1.0])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([-Wall foreign subdir-objects tar-pax no-dist-gzip dist-xz])
AC_PROG_CC
AM_PROG_CC_C_O

PKG_CHECK_MODULES([libnetfilter_queue], [libnetfilter_queue >= 0.0.17])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
