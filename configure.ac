AC_INIT(dgamelaunch, [1.4.6])
AC_PREREQ(2.50)

AC_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_STDC_HEADERS
AC_PROG_LEX

LIBS="$LIBS $LEXLIB"

AC_PROG_YACC

case "$(uname -s)" in
  Linux | *BSD)
    LIBS="$LIBS -lutil -lcrypt"
    AC_DEFINE(NOSTREAMS, 1, [Use SVR4 streams support in ttyrec.])
  ;;
esac

if test "$ac_cv_c_compiler_gnu" = yes; then
  CFLAGS="$CFLAGS -Wall -Wno-unused"
fi

AC_PROG_INSTALL
AC_SEARCH_LIBS(initscr, [ncurses curses termcap])

AC_ARG_ENABLE(enable-virus,
[AS_HELP_STRING([--enable-virus], [Use the 'virus' vi clone instead of the friendly ee editor.])],
[AC_SUBST(EDITOR, virus.c)], [AC_SUBST(EDITOR, ee.c)])

AC_ARG_WITH(config-file,
[AS_HELP_STRING([--with-config-file=PATH], [Define the path to the default configuration file.])],
[configfile=$withval], [configfile="/etc/dgamelaunch.conf"])

AC_DEFINE_UNQUOTED(DEFCONFIG, "$configfile", [Path to the default config file.])

AC_CHECK_FUNCS(openpty)
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
