AC_INIT([zerogw], [1.0.0])
AC_CONFIG_SRCDIR([src/commands.c])
AM_INIT_AUTOMAKE([1.14 subdir-objects])
orig_CFLAGS="$CFLAGS"
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([aconfig.h])
AM_MAINTAINER_MODE
AC_PROG_CC

# if <sys/endian.h> is found, have to check which version
AC_CHECK_HEADER([sys/endian.h], [HAVE_SYS_ENDIAN_H="yes"], [HAVE_SYS_ENDIAN_H="no"])
AC_CHECK_HEADER([endian.h], [HAVE_ENDIAN_H="yes"], [HAVE_ENDIAN_H="no"])

dnl Supply default CFLAGS, if not specified
if test -z "$orig_CFLAGS"; then
  if test x$GCC = xyes; then
    CFLAGS="-g -O3"
  fi
fi

AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_CONFIG_FILES([Makefile ])
AC_OUTPUT
