dnl Process this file with autoconf to produce a configure script.
AC_INIT(cups-bjnp, 0.5.5, louis.lagendijk@gmail.com)

AC_CONFIG_SRCDIR([bjnp.c])
AM_INIT_AUTOMAKE
AC_LIBSOURCES([cups-bjnp.spec])

## Check for programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

##
## Library directories
##
SRCH_LIB="/usr/local/lib /usr/lib /opt/cups/lib /opt/lib"
ac_save_IFS=$IFS
IFS="${IFS}:"
## LIBRARY_DIRS comes from command line, SRCH_LIB s defined above.
for dir in $LIBRARY_DIRS $SRCH_LIB; do
  if test -d "$dir"; then
    LIBDIRS="$LIBDIRS -L$dir"
## else
##    AC_MSG_WARN([*** Library directory $dir does not exist.])
  fi
done
IFS=$ac_save_IFS

## find cups lib

AC_CHECK_LIB(cups, cupsDoRequest,, AC_MSG_ERROR(CUPS library not found),)

## determine cups backend directory

AC_ARG_WITH(cupsbackenddir,
  AC_HELP_STRING([--with-cupsbackenddir=DIR],
                 [ cups-backends directory (auto)]),
[
  cupsbackenddir="${withval}"
  AC_SUBST([cupsbackenddir])dnl
], [
    dirs="/usr/lib/cups/backend /usr/local/lib/cups/backend /opt/lib/cups/backend\
          /usr/local/libexec/cups/backend /usr/libexec/cups/backend"
    cupsbackenddir=""
    for dir in $dirs; do
      if test -d "$dir" -a -z "$cupsbackenddir"; then
        cupsbackenddir=$dir
      fi
    done
    if test -n "$cupsbackenddir"; then
      echo found Cups backend directory $cupsbackenddir
      AC_SUBST([cupsbackenddir])dnl
    else
      AC_MSG_ERROR([Cups backend directory not specified and auto-detection failed!])
    fi
])

## Check if we have rpmbuild, so we can build rpm's
AC_PATH_PROG([RPMBUILD],rpmbuild)
AC_ARG_VAR(RPMBUILD, rpmbuild command)
if test -n "$RPMBUILD"; then
  rpmtarget=$srcdir/conf/rpmbuild
else
  rpmtarget=$srcdir/conf/norpm
fi
AC_SUBST_FILE(rpmtarget)
AC_SUBST(RPMBUILD)

## header file that contains output from configure

AC_CONFIG_HEADERS([config.h])
## AC_USE_SYSTEM_EXTENSIONS
dnl Checks for programs.
AC_SEARCH_LIBS([strerror],[cposix])

## determine libraries that need to get linked

AC_SEARCH_LIBS([gethostbyname], [nsl])
AC_SEARCH_LIBS([socket], [socket])

## Checks for header files.
AC_HEADER_STDC
AC_FUNC_SELECT_ARGTYPES
AC_CHECK_HEADERS(string.h fcntl.h arpa/inet.h netdb.h netinet/in.h \
                sys/socket.h sys/time.h sys/timeb.h wchar.h cups/cups.h \
		cups/backend.h cups/http.h \
                ,, AC_MSG_ERROR( required header file missing ))

AC_HEADER_RESOLV

AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

dnl Checks for required library functions.
AC_CHECK_FUNCS(ftime gethostbyaddr gethostname inet_ntoa memset\
               select socket strcasecmp strchr strerror strncasecmp \
               ,,AC_MSG_ERROR( required library function missing ))
AC_CHECK_FUNCS(getifaddrs)

## dnl internationalization macros
## AM_GNU_GETTEXT
AM_MAINTAINER_MODE
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
