dnl Use autoconf 2.53
dnl (Things tend to break across versions, they're neither upwards nor
dnl downward compatible. Blame GNU.)

AC_INIT(dgamelaunch, [1.5.1])
AC_PREREQ(2.53)

AC_CONFIG_HEADER(config.h)

AC_PROG_CC(cc gcc)
AC_STDC_HEADERS

AC_PROG_LEX
if test -z "$LEX"; then
   AC_MSG_ERROR([lex or flex not found.])
fi

LIBS="$LIBS $LEXLIB"

AC_CHECK_LIB(pthread, pthread_create, [PTHREAD_CFLAGS+=-lpthread])

AC_CHECK_PROGS(YACC, 'bison -y' byacc yacc)
if test -z "$YACC"; then
   AC_MSG_ERROR([bison or yacc not found.])
fi

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
  CFLAGS="$CFLAGS -Wall -Wno-unused $PTHREAD_CFLAGS"
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

AC_SEARCH_LIBS(use_default_colors, [ncursesw],
[
   AC_MSG_RESULT([Using ncursesw and color.])
   AC_DEFINE(USE_NCURSES_COLOR,1,[Use ncursesw and color])
],
[
   AC_MSG_RESULT([Not using ncursesw, not using color.])
])

AC_PROG_INSTALL
AC_SEARCH_LIBS(initscr, [ncursesw cursesw], [], [
   AC_MSG_ERROR([Cannot find cursesw or ncursesw devel libs.])
])



AC_ARG_ENABLE(debugfile,
[AC_HELP_STRING([--enable-debugfile], [Enable debugging output to a file.])],
[], [])

if test "$enable_debugfile" = yes; then
   AC_MSG_RESULT([Enabling debugging output to a file.])
   AC_DEFINE(USE_DEBUGFILE,1,[Output debugging into to a file in chroot.])
fi


AC_ARG_ENABLE(sqlite,
[AC_HELP_STRING([--enable-sqlite], [Use SQLite for the database instead of flat text file.])],
[], [])

AC_ARG_WITH(sqlite-db,
[AC_HELP_STRING([--with-sqlite-db=PATH], [Define the path and filename of the SQLite database.])],
[dgl_sqlite_db=$withval; enable_sqlite=yes], [dgl_sqlite_db="/dgldir/dgamelaunch.db"])

if test "$enable_sqlite" = yes; then
   AC_MSG_RESULT([Using SQLite for the database, located at $dgl_sqlite_db])
   AC_DEFINE(USE_SQLITE3,1,[Use SQLite for the database instead of flat text file.])
   LIBS="$LIBS -lsqlite3"
   AC_DEFINE_UNQUOTED(USE_SQLITE_DB, "$dgl_sqlite_db", [Path and filename of the SQLite database.])
fi




dgl_rlimit_core_default=157286400
AC_ARG_WITH(rlimit-core,
[AC_HELP_STRING([--with-rlimit-core=SIZE], [Enable and set the core dump maximum size.])],
[dgl_rlimit_core=$withval; enable_rlimit=yes], [dgl_rlimit_core=$dgl_rlimit_core_default])

if test "$enable_rlimit" = yes; then
   if test "$dgl_rlimit_core" = yes; then
      dgl_rlimit_core=$dgl_rlimit_core_default
   fi
   AC_CHECK_HEADERS([sys/resource.h], [], [AC_MSG_ERROR([sys/resource.h not found.])], [])
   AC_MSG_RESULT([Enabled and set maximum core dump size to $dgl_rlimit_core])
   AC_DEFINE(USE_RLIMIT,1,[Use getrlimit/setrlimit])
   AC_DEFINE_UNQUOTED(USE_RLIMIT_CORE, $dgl_rlimit_core, [Maximum core dump size])
   enable_rlimit=no
fi

dgl_rlimit_as_default=104857600
AC_ARG_WITH(rlimit-as,
[AC_HELP_STRING([--with-rlimit-as=SIZE], [Enable and set the maximum memory usage.])],
[dgl_rlimit_as=$withval; enable_rlimit=yes], [dgl_rlimit_as=$dgl_rlimit_as_default])

if test "$enable_rlimit" = yes; then
   if test "$dgl_rlimit_as" = yes; then
      dgl_rlimit_as=$dgl_rlimit_as_default
   fi
   AC_CHECK_HEADERS([sys/resource.h], [], [AC_MSG_ERROR([sys/resource.h not found.])], [])
   AC_MSG_RESULT([Enabled and set maximum memory usage limit to $dgl_rlimit_as])
   AC_DEFINE(USE_RLIMIT,1,[Use getrlimit/setrlimit])
   AC_DEFINE_UNQUOTED(USE_RLIMIT_AS, $dgl_rlimit_as, [Maximum mem usage])
   enable_rlimit=no
fi


AC_ARG_ENABLE(shmem,
[AC_HELP_STRING([--enable-shmem], [Use a shared memory block to show number of watchers.])],
[enable_shmem=yes], [])

if test "$enable_shmem" = yes; then
   AC_CHECK_HEADERS([semaphore.h], [], [AC_MSG_ERROR([semaphore.h not found.])], [])
   AC_CHECK_HEADERS([sys/ipc.h], [], [AC_MSG_ERROR([sys/ipc.h not found.])], [])
   AC_CHECK_HEADERS([sys/shm.h], [], [AC_MSG_ERROR([sys/shm.h not found.])], [])
   AC_MSG_RESULT([Enabled showing number of watchers.])
   AC_DEFINE(USE_SHMEM,1,[Use shared memory block])
   LIBS="$LIBS -lrt"
   # or -pthread?
fi


AC_ARG_WITH(config-file,
[AC_HELP_STRING([--with-config-file=PATH], [Define the path to the default configuration file.])],
[configfile=$withval], [configfile="/etc/dgamelaunch.conf"])

AC_DEFINE_UNQUOTED(DEFCONFIG, "$configfile", [Path to the default config file.])

AC_CHECK_HEADERS([sys/pstat.h])
AC_CHECK_FUNCS([openpty setenv setproctitle pstat kqueue])

if test "$ac_cv_func_setproctitle" = no; then
  SETPROCTITLE_C=setproctitle.c
  if test "$ac_cv_func_pstat" = yes; then
    AC_DEFINE(SPT_TYPE,SPT_PSTAT,[Type of setproctitle emulation to use.])
  else
    AC_DEFINE(SPT_TYPE,SPT_REUSEARGV)
  fi
else
  SETPROCTITLE_C=""
fi



AC_SUBST(SETPROCTITLE_C)

AC_CONFIG_FILES(Makefile)
AC_OUTPUT
