dnl Use autoconf 2.53
dnl (Things tend to break across versions, they're neither upwards nor
dnl downward compatible. Blame GNU.)

AC_INIT(dgamelaunch, [1.4.6])
AC_PREREQ(2.53)

AC_CONFIG_HEADER(config.h)

AC_PROG_CC(cc gcc)
AC_STDC_HEADERS
AC_PROG_LEX

LIBS="$LIBS $LEXLIB"

AC_PROG_YACC

case "$(uname -s)" in
  Linux | *BSD)
    LIBS="$LIBS -lutil -lcrypt"
    AC_DEFINE(NOSTREAMS, 1, [Don't use SVR4 streams support in ttyrec.])
  ;;
esac

AC_MSG_CHECKING([for Solaris cc])
case "`$CC -V 2>&1`" in
  *Sun*WorkShop* | *Forte*Developer*)
    AC_MSG_RESULT(yes)
    suncc=yes
  ;;
  *)
    AC_MSG_RESULT(no)
  ;;
esac

AC_PATH_PROG(MKDEP, mkdep)
AC_PATH_PROG(MAKEDEPEND, makedepend)

AC_MSG_CHECKING([how to generate dependency info])

if test "$ac_cv_c_compiler_gnu" = yes; then
  CFLAGS="$CFLAGS -Wall -Wno-unused"
  MKDEP="$CC -MM"
  MKDEP_DIRECT="> .depend"
  AC_MSG_RESULT(gcc)
elif test "$suncc" = yes; then
  MKDEP="$CC -xM"
  # -xM is noisy
  MKDEP_DIRECT="> .depend 2>/dev/null"
  AC_MSG_RESULT([Solaris cc])
elif test -n "$MKDEP"; then
  AC_MSG_RESULT(mkdep)
elif test -n "$MAKEDEPEND"; then
  MKDEP=":> .depend; $MAKEDEPEND -f.depend"
  MKDEP_DIRECT=">/dev/null 2>&1" # noisy prick of a program
  AC_MSG_RESULT(makedepend)
else
  MKDEP=:
  AC_MSG_RESULT([nothing!])
fi

AC_SUBST(MKDEP)
AC_SUBST(MKDEP_DIRECT)

AC_PROG_INSTALL
AC_SEARCH_LIBS(initscr, [curses ncurses])

AC_ARG_ENABLE(enable-virus,
[AC_HELP_STRING([--enable-virus], [Use the 'virus' vi clone instead of the friendly ee editor.])],
[EDITOR=virus.c], [EDITOR=virus.c])

AC_SUBST(EDITOR)

AC_ARG_WITH(config-file,
[AC_HELP_STRING([--with-config-file=PATH], [Define the path to the default configuration file.])],
[configfile=$withval], [configfile="/etc/dgamelaunch.conf"])

AC_DEFINE_UNQUOTED(DEFCONFIG, "$configfile", [Path to the default config file.])

AC_CHECK_FUNCS([openpty setenv setproctitle])
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
