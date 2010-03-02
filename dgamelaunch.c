/* dgamelaunch.c
 *
 * (c)2001-4 M. Drew Streib <dtype@dtype.org>
 * also parts (c) 2003-4 Joshua Kwan <joshk@triplehelix.org>,
 * Brett Carrington <brettcar@segvio.org>,
 * Jilles Tjoelker <jilles@stack.nl>
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * See this program in action at http://alt.org/nethack/
 *
 * This is a little wrapper for nethack (and soon other programs) that
 * will allow them to be run from a telnetd session, chroot, shed privs,
 * make a simple login, then play the game.
 */

#define _GNU_SOURCE

#include "dgamelaunch.h"
#include "config.h"
#include "ttyplay.h"
#include "ttyrec.h"

/* a request from the author: please leave some remnance of
 * 'based on dgamelaunch version xxx' in any derivative works, or
 * even keep the line the same altogether. I'm probably happy 
 * to make any changes you need. */

/* ************************************************************* */
/* ************************************************************* */
/* ************************************************************* */

/* program stuff */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/ioctl.h>          /* ttyrec */
#include <sys/stat.h>
#ifdef USE_RLIMIT
#include <sys/resource.h>
#endif
#include <libgen.h>
#include <stdlib.h>
#include <curses.h>

#ifdef USE_SQLITE3
# include <sqlite3.h>
#endif

#ifndef __FreeBSD__
# ifdef __APPLE__
#  include <unistd.h>
# else
#  include <crypt.h>
# endif
#else
# include <libutil.h>
#endif

#ifdef __linux__
# include <pty.h>
#endif

#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>

extern FILE* yyin;
extern int yyparse ();

extern int editor_main (int argc, char **argv);

/* global variables */

char * __progname;
int  g_idle_alarm_enabled = 0;

#ifndef USE_SQLITE3
int f_num = 0;
struct dg_user **users = NULL;
#endif
struct dg_user *me = NULL;
struct dg_banner banner;

struct dg_user *
cpy_me(struct dg_user *me)
{
    struct dg_user *tmp = malloc(sizeof(struct dg_user));

    if (tmp && me) {
#ifdef USE_SQLITE3
	tmp->id = me->id;
#endif
	if (me->username) tmp->username = strdup(me->username);
	if (me->email)    tmp->email    = strdup(me->email);
	if (me->env)      tmp->env      = strdup(me->env);
	if (me->password) tmp->password = strdup(me->password);
	tmp->flags = me->flags;
    }
    return tmp;
}

#ifndef HAVE_SETENV
int
mysetenv (const char* name, const char* value, int overwrite)
{
  int retval;
  char *buf = NULL;
  
  if (getenv(name) == NULL || overwrite)
  {
    size_t len = strlen(name) + 1 + strlen(value) + 1; /* NAME=VALUE\0 */
    buf = malloc(len);
    snprintf(buf, len, "%s=%s", name, value);
    retval = putenv(buf);
  }
  else
    retval = -1;
  
  return retval;  
}
#else /* use native setenv */
# define mysetenv setenv
#endif

/* ************************************************************* */
/* for ttyrec */

void
ttyrec_getpty ()
{
#ifdef HAVE_OPENPTY
    if (openpty (&master, &slave, NULL, NULL, NULL) == -1) {
	debug_write("cannot openpty");
	graceful_exit (62);
    }
#else
    if ((master = open ("/dev/ptmx", O_RDWR)) < 0) {
	debug_write("cannot open /dev/ptmx");
	graceful_exit (62);
    }
  grantpt (master);
  unlockpt (master);
  if ((slave = open ((const char *) ptsname (master), O_RDWR)) < 0)
    {
	debug_write("cannot open master ptsname");
      graceful_exit (65);
    }
#endif
  ioctl (slave, TIOCSWINSZ, (char *) &win);
  tcsetattr(slave, TCSANOW, &tt);
}

/* ************************************************************* */

char*
gen_ttyrec_filename ()
{
  time_t rawtime;
  struct tm *ptm;
  char *ttyrec_filename = calloc(100, sizeof(char));

  /* append time to filename */
  time (&rawtime);
  ptm = gmtime (&rawtime);
  snprintf (ttyrec_filename, 100, "%04i-%02i-%02i.%02i:%02i:%02i.ttyrec",
            ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
            ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  return ttyrec_filename;
}

/* ************************************************************* */

char*
gen_nhext_filename ()
{
  time_t rawtime;
  struct tm *ptm;
  char *nhext_filename = calloc(100, sizeof(char));

  /* append time to filename */
  time (&rawtime);
  ptm = gmtime (&rawtime);
  snprintf (nhext_filename, 100, "%04i-%02i-%02i.%02i:%02i:%02i.nhext",
            ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
            ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  return nhext_filename;
}

/* ************************************************************* */

char*
gen_inprogress_lock (int game, pid_t pid, char* ttyrec_filename)
{
  char *lockfile = NULL, filebuf[80];
  int fd;
  size_t len;
  struct flock fl = { 0 };

  snprintf (filebuf, sizeof(filebuf), "%d\n%d\n%d\n",
		  pid, win.ws_row, win.ws_col);

  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  len = strlen(dgl_format_str(game, me, myconfig[game]->inprogressdir, NULL)) + strlen(me->username) + strlen(ttyrec_filename) + 13;
  lockfile = calloc(len, sizeof(char));

  snprintf (lockfile, len, "%s%s:%s", dgl_format_str(game, me, myconfig[game]->inprogressdir, NULL),
            me->username, ttyrec_filename);

  fd = open (lockfile, O_WRONLY | O_CREAT, 0644);
  if (fcntl (fd, F_SETLKW, &fl) == -1) {
      debug_write("cannot fnctl inprogress-lock");
    graceful_exit (68);
  }

  write (fd, filebuf, strlen (filebuf));

  return lockfile;
}

/* ************************************************************* */

void
catch_sighup (int signum)
{
  if (child)
    {
      sleep (10);
      kill (child, SIGHUP);
      sleep (5);
    }
  debug_write("catchup sighup");
  graceful_exit (2);
}

/* ************************************************************* */

int
dgl_getch(void)
{
    const int c = getch();
    idle_alarm_reset();
    return c;
}

/* ************************************************************* */

static void
dgl_idle_kill(int signal)
{
    kill(0, SIGHUP);
}

void
idle_alarm_set_enabled(int enabled)
{
    signal(SIGALRM, SIG_IGN);
    g_idle_alarm_enabled = enabled;
    idle_alarm_reset();
    if (enabled)
        signal(SIGALRM, dgl_idle_kill);
}

void
idle_alarm_reset(void)
{
    if (g_idle_alarm_enabled && globalconfig.menu_max_idle_time > 0)
        alarm(globalconfig.menu_max_idle_time);
}

/* ************************************************************* */


char *
bannerstrmangle(char *buf, char *fromstr, char *tostr)
{
    static char bufnew[81];
    char *loc;
    char *b = buf;

    memset (bufnew, 0, 80);

    if (strstr(b, fromstr)) {
	int i = 0;
	while ((loc = strstr (b, fromstr)) != NULL) {
	    for (; i < 80; i++) {
		if (loc != b)
		    bufnew[i] = *(b++);
		else {
		    strlcat (bufnew, tostr, 80);
		    b += strlen(fromstr);
		    i += strlen(tostr);
		    break;
                }

		if (strlen (b) == 0)
		    break;
	    }
	}

	if (*b)
	    strlcat(bufnew, b, 80);
    } else strncpy(bufnew, buf, 80);

    return bufnew;
}

void
loadbanner (char *fname, struct dg_banner *ban)
{
  FILE *bannerfile;
  char buf[80];

  memset (buf, 0, 80);

  if (ban->lines) return;

  bannerfile = fopen (fname, "r");

  if (!bannerfile)
    {
#define NOTE_NO_DGL_BANNER "### NOTE: administrator has not installed a %s file"
      size_t len;
      ban->len = 2;
      ban->lines = malloc (sizeof (char *));
      ban->lines[0] =
        strdup ("### dgamelaunch " PACKAGE_VERSION
                " - network console game launcher");
      len = strlen(fname) + ARRAY_SIZE(NOTE_NO_DGL_BANNER);
      ban->lines[1] = malloc(len);
      snprintf(ban->lines[1], len, NOTE_NO_DGL_BANNER, fname);
#undef NOTE_NO_DGL_BANNER
      return;
    }

  ban->len = 0;

  while (fgets (buf, 80, bannerfile) != NULL)
    {
      char bufnew[80];
      int slen;

      memset (bufnew, 0, 80);

      slen = strlen(buf);
      if ((slen > 0) && (buf[slen-1] == '\n')) buf[slen-1] = '\0';

      ban->len++;
      ban->lines = realloc (ban->lines, sizeof (char *) * ban->len);

      strncpy(bufnew, buf, 80);
      strncpy(bufnew, bannerstrmangle(bufnew, "$VERSION", PACKAGE_STRING), 80);
      strncpy(bufnew, bannerstrmangle(bufnew, "$SERVERID", globalconfig.server_id ? globalconfig.server_id : ""), 80);
      if (me && loggedin) {
	  strncpy(bufnew, bannerstrmangle(bufnew, "$USERNAME", me->username), 80);
      } else {
	  strncpy(bufnew, bannerstrmangle(bufnew, "$USERNAME", "[Anonymous]"), 80);
      }
      ban->lines[ban->len - 1] = strdup(bufnew);

      memset (buf, 0, 80);

      if (ban->len == 24)
	  break;
  }

  fclose (bannerfile);
}

void
drawbanner (struct dg_banner *ban, unsigned int start_line, unsigned int howmany)
{
  unsigned int i;

  if (!ban) return;

  if (howmany > ban->len || howmany == 0)
    howmany = ban->len;

  for (i = 0; i < howmany; i++)
    mvaddstr (start_line + i, 1, ban->lines[i]);
}

void
inprogressmenu (int gameid)
{
    const char *selectorchars = "abcdefghijklmnoprstuvwxyzABCDEFGHIJKLMNOPRSTUVWXYZ";
  int i, menuchoice, len = 20, offset = 0;
  static dg_sortmode sortmode = NUM_SORTMODES;
  time_t ctime;
  struct dg_game **games = NULL;
  char ttyrecname[130], gametype[10];
  int *is_nhext;
  sigset_t oldmask, toblock;
  int idx = -1;
  int max_height = -1;
  int selected = -1;

  int resizex = -1;
  int resizey = -1;

  char *selectedgame = NULL;

  int abs_max_height;
  int top_banner_hei = 5;
  int btm_banner_hei = 3;
  int btm;

  int title_attr = A_STANDOUT;
  int selected_attr = A_BOLD;

  int require_enter = 0; /* TODO: make configurable */

  if (sortmode == NUM_SORTMODES)
      sortmode = globalconfig.sortmode;

  abs_max_height = strlen(selectorchars);
  is_nhext = (int *)calloc(abs_max_height+1, sizeof(int));

  if (!is_nhext) {
      debug_write("could not calloc is_nhext");
      graceful_exit(70);
  }

  games = populate_games (gameid, &len, NULL); /* FIXME: should be 'me' instead of 'NULL' */
  games = sort_games (games, len, sortmode);

  while (1)
    {
	term_resize_check();
	max_height = dgl_local_LINES - (top_banner_hei + btm_banner_hei) - 1;
	if (max_height < 2) {
	    free(is_nhext);
	    free_populated_games(games, len);
	    return;
	}
	if (max_height > abs_max_height) max_height = abs_max_height;

      if (len == 0)
        offset = 0;
      else
        {
	  while (offset >= len && offset >= max_height)
	    offset -= max_height;

	  while ((offset > 0) && (offset + max_height > len))
	      offset--;
	}

      erase ();
      drawbanner (&banner, 1, 1);

      if (len > 0) {
	  mvaddstr (3, 1, "The following games are in progress:");

#define dgl_sortprintw(mode, x, str)                 \
	  if (sortmode == mode) attron(title_attr);  \
	  mvprintw(top_banner_hei,x,str);            \
	  if (sortmode == mode) attroff(title_attr);

	  mvprintw(top_banner_hei,1," ");

	  dgl_sortprintw(SORTMODE_USERNAME,    4, "Username")
	  dgl_sortprintw(SORTMODE_GAMENUM,    21, "Game")
	  dgl_sortprintw(SORTMODE_WINDOWSIZE, 29, "Size")
	  dgl_sortprintw(SORTMODE_STARTTIME,  37, "Start date & time")
	  dgl_sortprintw(SORTMODE_IDLETIME,   58, "Idle time")

#undef dgl_sortprintw
      }

      for (i = 0; i < max_height; i++)
        {
          if (i + offset >= len)
            break;

	  if (i + offset == selected) attron(selected_attr);

	  is_nhext[i] = !strcmp (games[i + offset]->ttyrec_fn + strlen (games[i + offset]->ttyrec_fn) - 6, ".nhext");

	  if (is_nhext[i])
	    strcpy (gametype, "  NhExt");
	  else
	    snprintf (gametype, sizeof gametype, "%3dx%3d",
		games[i + offset]->ws_col, games[i + offset]->ws_row);

          mvprintw (top_banner_hei + 1 + i, 1, "%c) %-15s  %-5s  %s  %s %s  %ldm %lds",
                    selectorchars[i], games[i + offset]->name, myconfig[games[i + offset]->gamenum]->shortname, gametype,
                    games[i + offset]->date, games[i + offset]->time,
                    (time (&ctime) - games[i + offset]->idle_time) / 60,
                    (time (&ctime) - games[i + offset]->idle_time) % 60);

	  if (i + offset == selected) attroff(selected_attr);

        }

      btm = dgl_local_LINES-btm_banner_hei-top_banner_hei;
      if (btm > i) btm = i+1;
      if (len > 0) {
	  if (max_height+offset < len)
	      mvprintw ((btm+top_banner_hei), 1, "(%d-%d of %d)", offset + 1, offset + i, len);
	  else
	      mvprintw ((btm+top_banner_hei), 4, "(end)");
	  mvaddstr ((btm+2+top_banner_hei), 1, "Watch which game? ('?' for help) => ");
      } else {
	  mvprintw(top_banner_hei,4,"Sorry, no games available for viewing.");
	  mvaddstr((btm+2+top_banner_hei), 1, "Press 'q' to return, or '?' for help => ");
      }

      refresh ();

      switch ((menuchoice = dgl_getch ()))
        {
	case '*':
	    if (len > 0) {
		idx = random() % len;
		selected = idx;
		goto watchgame;
	    }
	    break;
	case '?':
	    (void) runmenuloop(dgl_find_menu("watchmenu_help"));
	    break;
       case '/':
           {
               int match = -1;
	       int firstmatch = -1;
	       int nmatches = 0;
               char findname[DGL_PLAYERNAMELEN+1];
	       if (len <= 0) break;
               findname[0] = '\0';
	       mvprintw ((btm+2+top_banner_hei), 1, "Watch which player? =>                 "); /* stupid... */
	       mvaddstr ((btm+2+top_banner_hei), 1, "Watch which player? => ");
               if ((mygetnstr(findname, DGL_PLAYERNAMELEN, 1) == OK) && (strlen(findname) > 1)) {
		   int mlen = strlen(findname);
                   for (i = 0; i < len; i++)
                       if (!strncasecmp(games[i]->name, findname, mlen)) {
			   if (firstmatch == -1) firstmatch = i;
                           match = i;
			   nmatches++;
                       }
		   if (nmatches > 1)
		       match = firstmatch;
                   if (match > -1) {
		       if (!strcmp(games[match]->ttyrec_fn + strlen (games[match]->ttyrec_fn) - 6, ".nhext")) break;
		       idx = match;
		       selected = idx;
		       goto watchgame;
                   }
               }
           }
           break;
        case '>':
	    if ((offset + max_height) >= len) {
		if (max_height < len) offset = (len - max_height);
		else offset = 0;
	    } else
            offset += max_height;
          break;

        case '<':
          if ((offset - max_height) < 0)
	      offset = 0;
          else
            offset -= max_height;
          break;

	case ERR:
	case 'q': case 'Q':
	    if (is_nhext) free(is_nhext);
	    free_populated_games(games, len);
          return;

	case '.':
	    if (sortmode < (NUM_SORTMODES-1)) sortmode++; else sortmode = SORTMODE_USERNAME;
	    break;

	case ',':
	    if (sortmode > SORTMODE_USERNAME) sortmode--; else sortmode = (NUM_SORTMODES-1);
	    break;

	case 12: case 18: /* ^L, ^R */
	  clear ();
	  break;

	case 13:
	case 10:
	case KEY_ENTER:
	    if (selected >= 0 && selected < len) {
		idx = selected;
		goto watchgame;
	    }
	    break;

        default:
	    if (strchr(selectorchars, menuchoice) && (len > 0)) {
		int sidx = strchr(selectorchars, menuchoice) - selectorchars;

		if ((sidx > max_height) || (sidx >= len)) {
		    selected = -1;
		    break;
		}

		if (is_nhext[sidx]) /* Cannot watch NhExt game */
		    break;

	      idx = sidx + offset;
	      if (require_enter) {
		  if (selected == idx) selected = -1;
		  else selected = idx;
		  break;
	      } else selected = idx;
watchgame:

              /* valid choice has been made */
              chosen_name = strdup (games[idx]->name);
              snprintf (ttyrecname, 130, "%s",
                        games[idx]->ttyrec_fn);

              clear ();
              refresh ();
              endwin ();

	      resizey = games[idx]->ws_row;
	      resizex = games[idx]->ws_col;
	      if (loggedin)
		  setproctitle("%s [watching %s]", me->username, chosen_name);
	      else
		  setproctitle("<Anonymous> [watching %s]", chosen_name);
              ttyplay_main (ttyrecname, 1, resizex, resizey);
	      if (loggedin)
		  setproctitle("%s", me->username);
	      else
		  setproctitle("<Anonymous>");
              initcurses ();
            }
        }

      if (selected >= 0 && selected < len)
	  selectedgame = strdup(games[selected]->name);
      games = populate_games (gameid, &len, NULL); /* FIXME: should be 'me' instead of 'NULL' */
      games = sort_games (games, len, sortmode);
      if (selectedgame) {
	  selected = -1;
	  for (i = 0; i < len; i++)
	      if (!strcmp(games[i]->name, selectedgame)) {
		  selected = i;
		  break;
	      }
	  free(selectedgame);
	  selectedgame = NULL;
      }
    }
  if (is_nhext) free(is_nhext);
  free_populated_games(games, len);
}

/* ************************************************************* */

/*
 * Check email address, returns 1 if valid, 0 otherwise.
 * Doesn't recognize addresses with parts in double-quotes.
 * Addresses with a colon in them are always rejected.
 */
int
check_email (char *s)
{
  char *atomchars = "!#$%&'*+-/=?^_`{|}~" "0123456789"
    "abcdefghijklmnopqrstuvwxyz" "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  int f;

  if (*s == '@')
    return 0;

  while (*s != '\0' && *s != '@')
    {
      if (strchr(atomchars, *s) == NULL)
        return 0;
      s++;
      if (*s == '.')
        s++;
    }

  if (*s == '\0')
    return 0;
  s++;

  f = 0;
  while (*s != '\0')
    {
      if (strchr(atomchars, *s) == NULL)
        return 0;
      s++;
      if (*s == '.')
        s++, f = 1;
    }

  return f;
}

void
change_email ()
{
  char buf[81];

  clear();

  for (;;)
  {
      drawbanner(&banner, 1,1);

    mvprintw(3, 1, "Your current email is: %s", me->email);
    mvaddstr(4, 1, "Please enter a new one (max 80 chars; blank line aborts)");
    mvaddstr(6, 1, "=> ");

    if (mygetnstr (buf, 80, 1) != OK)
	return;

    if (*buf == '\0')
      return;
    else if (!strcmp(me->email, buf))
    {
      clear();
      mvaddstr (8, 1, "That's the same one as before. Try again?");
      move(1,1);
    }
    else if (check_email (buf))
    {
      mvprintw (8, 1, "Changing email address to '%s'. Confirm (y/n): ", buf);
      if (dgl_getch() == 'y')
      {
	free(me->email);
	me->email = strdup(buf);
	writefile(0);
	return;
      }
      else
      {
	mvaddstr(9, 1, "No changes made. Press any key to continue...");
	dgl_getch();
	return;
      }
    }
    else
    {
      clear();
      mvaddstr (8, 1, "That doesn't look like an email address to me.");
      move(1,1);
    }
  }
}

int
changepw (int dowrite)
{
  char buf[DGL_PASSWDLEN+1];
  int error = 2;

  /* A precondition is that struct `me' exists because we can be not-yet-logged-in. */
  if (!me) {
      debug_write("no 'me' in changepw");
    graceful_exit (122);        /* Die. */
  }

  while (error)
    {
      char repeatbuf[DGL_PASSWDLEN+1];
      clear ();

      drawbanner (&banner, 1, 1);

      mvprintw (5, 1,
                "Please enter a%s password. Remember that this is sent over the net",
                loggedin ? " new" : "");
      mvaddstr (6, 1,
                "in plaintext, so make it something new and expect it to be relatively");
      mvaddstr (7, 1, "insecure.");
      mvprintw (8, 1,
                "%i character max. No ':' characters. Blank line to abort.", DGL_PASSWDLEN);
      mvaddstr (10, 1, "=> ");

      if (error == 1)
        {
          mvaddstr (15, 1, "Sorry, the passwords don't match. Try again.");
          move (10, 4);
        }

      refresh ();

      if (mygetnstr (buf, DGL_PASSWDLEN, 0) != OK)
	  return 0;

      if (*buf == '\0')
        return 0;

      if (strchr (buf, ':') != NULL) {
	  debug_write("cannot have ':' in passwd");
        graceful_exit (112);
      }

      mvaddstr (12, 1, "And again:");
      mvaddstr (13, 1, "=> ");

      if (mygetnstr (repeatbuf, DGL_PASSWDLEN, 0) != OK)
	  return 0;

      if (!strcmp (buf, repeatbuf))
        error = 0;
      else
        error = 1;
    }

  free(me->password);
  me->password = strdup (crypt (buf, buf));

  if (dowrite)
    writefile (0);

  return 1;
}

/* ************************************************************* */

void
wall_email(char *from, char *msg)
{
    int len, i;
    struct dg_game **games = NULL;
    char spool_fn[1024+1];
    FILE *user_spool = NULL;
    struct flock fl = { 0 };

    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    if (!from || !msg) return;

    if (strlen(from) < 1) {
	fprintf(stderr, "Error: wall: 'from' username is too short!\n");
	debug_write("wall: 'from' username too short");
	graceful_exit(121);
    }

    if (strlen(msg) >= DGL_MAILMSGLEN) {
	fprintf(stderr, "Error: wall: message too long!\n");
	debug_write("wall: message too long");
	graceful_exit(120);
    }

    games = populate_games(-1, &len, me);

    if (len == 0) {
	fprintf(stderr, "Error: wall: no one's logged in!\n");
	debug_write("wall: no people playing");
	graceful_exit(118);
    }

    for (i = 0; i < len; i++) {
	int game = games[i]->gamenum;
	int fnamelen;
	if (strlen(myconfig[game]->spool) < 1) continue;

	snprintf (spool_fn, 1024, "%s/%s", myconfig[game]->spool, games[i]->name);

	if ((user_spool = fopen (spool_fn, "a")) == NULL) continue;

	while (fcntl(fileno(user_spool), F_SETLK, &fl) == -1) {
	    if (errno != EAGAIN) continue;
	    sleep (1);
	}
	fprintf(user_spool, "%s:%s\n", from, msg);
	fclose(user_spool);
    }
    free_populated_games(games, len);
}

void
domailuser (char *username)
{
  unsigned int len, i;
  char *spool_fn, message[DGL_MAILMSGLEN+1];
  FILE *user_spool = NULL;
  time_t now;
  int mail_empty = 1;
  int game;
  struct flock fl = { 0 };

  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  assert (loggedin);

  game = 0; /*TODO: find_curr_player_game(username) */

  if (strlen(myconfig[game]->spool) < 1) return;

  len = strlen(myconfig[game]->spool) + strlen (username) + 1;
  spool_fn = malloc (len + 1);
  time (&now);
  snprintf (spool_fn, len + 1, "%s/%s", myconfig[game]->spool, username);

  /* print the enter your message line */
  clear ();
  drawbanner (&banner, 1, 1);
  mvprintw (5, 1,
            "Enter your message here. It is to be one line only and %i characters or less.",
	    DGL_MAILMSGLEN);
  mvaddstr (7, 1, "=> ");

  if (mygetnstr (message, DGL_MAILMSGLEN, 1) != OK)
      return;

  for (i = 0; i < strlen (message); i++)
    {
      if (message[i] != ' ' && message[i] != '\n' && message[i] != '\t')
        mail_empty = 0;
    }

  if (mail_empty)
    {
      mvaddstr (9, 1, "This scroll appears to be blank.");
      mvaddstr (10, 1, "(Aborting your message.)");
      mvaddstr (12, 1, "--More--");
      dgl_getch ();
      return;
    }

  if ((user_spool = fopen (spool_fn, "a")) == NULL)
    {
      mvaddstr (9, 1,
                "You fall into the water!  You sink like a rock.");
      mvprintw (10, 1,
                "(Couldn't open %s'%c spool file.  Aborting.)",
                username, (username[strlen (username) - 1] != 's') ? 's' : 0);
      mvaddstr (12, 1, "--More--");
      dgl_getch ();
      return;
    }

  mvaddstr (9, 1, "Sending your scroll...");
  refresh ();

  /* Getting a lock on the mailspool... */
  while (fcntl (fileno (user_spool), F_SETLK, &fl) == -1)
    {
      if (errno != EAGAIN)
        {
          mvaddstr (10, 1,
                    "(Received a weird error from fcntl.  Aborting.)");
	  mvaddstr (12, 1, "--More--");
          dgl_getch ();
          return;
        }
      sleep (1);
    }

  fprintf (user_spool, "%s:%s\n", me->username, message);

  /* 
   * Don't unlock the file ourselves, this way it will be done automatically
   * after all data has been written. (Using file locking with stdio is icky.)
   */

  fclose (user_spool);

  mvaddstr (9, 1, "Scroll delivered!         ");
  move(9, 19); /* Pedantry! */
  refresh ();
  sleep (2);

  return;
}


/* ************************************************************* */

void
freefile ()
{
#ifndef USE_SQLITE3
  int i;

  /* free existing mem, clear existing entries */
  for (i = 0; i < f_num; i++)
    {
      if (users[i] != me)
      {
	free (users[i]->password);
	free (users[i]->username);
	free (users[i]->email);
	free (users[i]->env);
	free (users[i]);
      }
    }

  if (users)
    free (users);

  users = NULL;
  f_num = 0;
#endif
}

/* ************************************************************* */

void
initcurses ()
{
  initscr ();
  cbreak ();
  noecho ();
  nonl ();
  intrflush (stdscr, FALSE);
  keypad (stdscr, TRUE);
}

/* ************************************************************* */

void
autologin (char* user, char *pass)
{
  struct dg_user *tmp;
  tmp = userexist(user, 0);
  if (tmp) {
      me = cpy_me(tmp);
      if (passwordgood(pass)) {
	  loggedin = 1;
	  setproctitle ("%s", me->username);
	  dgl_exec_cmdqueue(globalconfig.cmdqueue[DGLTIME_LOGIN], 0, me);
      }
  }
}

void
loginprompt (int from_ttyplay)
{
  char user_buf[DGL_PLAYERNAMELEN+1], pw_buf[DGL_PASSWDLEN+2];
  int error = 2;

  loggedin = 0;

  while (error)
    {
      clear ();

      drawbanner (&banner, 1, 1);

      if (from_ttyplay == 1)
	mvaddstr (4, 1, "This operation requires you to be logged in.");

      mvaddstr (5, 1,
                "Please enter your username. (blank entry aborts)");
      mvaddstr (7, 1, "=> ");

      if (error == 1)
        {
          mvaddstr (9, 1, "There was a problem with your last entry.");
          move (7, 4);
        }

      refresh ();

      if (mygetnstr (user_buf, DGL_PLAYERNAMELEN, 1) != OK)
	  return;

      if (*user_buf == '\0')
        return;

      error = 1;

      {
	  struct dg_user *tmpme;
	  if ((tmpme = userexist(user_buf, 0))) {
	      me = cpy_me(tmpme);
	      error = 0;
	  }
      }
    }

  clear ();

  drawbanner (&banner, 1, 1);

  mvaddstr (5, 1, "Please enter your password.");
  mvaddstr (7, 1, "=> ");

  refresh ();

  if (mygetnstr (pw_buf, DGL_PASSWDLEN, 0) != OK)
      return;

  if (passwordgood (pw_buf))
    {
      loggedin = 1;
      if (from_ttyplay)
	  setproctitle("%s [watching %s]", me->username, chosen_name);
      else
	  setproctitle("%s", me->username);
      dgl_exec_cmdqueue(globalconfig.cmdqueue[DGLTIME_LOGIN], 0, me);
    }
  else 
  {
    me = NULL;
    if (from_ttyplay == 1)
    {
      mvaddstr(9, 1, "Login failed. Returning to game.");
      refresh();
      sleep(2);
    }
  } 
}

/* ************************************************************* */

void
newuser ()
{
  char buf[1024], dirname[100];
  int error = 2;
  unsigned int i;

  loggedin = 0;

#ifndef USE_SQLITE3
  if (f_num >= globalconfig.max)
  {
      clear ();

      drawbanner (&banner, 1, 1);

      mvaddstr (5, 1, "Sorry, too many users have registered now.");
      mvaddstr (6, 1, "You might email the server administrator.");
      mvaddstr (7, 1, "Press return to return to the menu. ");
      dgl_getch ();

      return;
  }
#endif

  if (me)
    free (me);

  me = calloc (1, sizeof (struct dg_user));

  while (error)
    {
      clear ();

      sprintf(buf, "%i character max.", globalconfig.max_newnick_len);

      drawbanner (&banner, 1, 1);

      mvaddstr (5, 1, "Welcome new user. Please enter a username.");
      mvaddstr (6, 1,
                "Only characters and numbers are allowed, with no spaces.");
      mvaddstr (7, 1, buf);
      mvaddstr (9, 1, "=> ");

      if (error == 1)
        {
          mvaddstr (11, 1, "There was a problem with your last entry.");
          move (9, 4);
        }

      refresh ();

      if (mygetnstr (buf, globalconfig.max_newnick_len, 1) != OK)
	  buf[0] = 0;

      if (*buf == '\0') {
	  free(me);
	  me = NULL;
	  return;
      }

      if (!userexist(buf, 1)) {
	  error = 0;
      } else
	  error = 1;

      for (i = 0; i < strlen (buf); i++)
        {
          if (!isalnum((int)buf[i]))
            error = 1;
        }

      if (strlen (buf) < 2)
        error = 1;

      if (strlen (buf) == 0)
      {
	free(me);
	me = NULL;
        return;
      }
    }

  me->username = strdup (buf);

  /* password step */

  clear ();

  if (!changepw (0))                  /* Calling changepw instead to prompt twice. */
  {
    free(me->username);
    free(me);
    me = NULL;
    return;
  }

  /* email step */

  error = 2;
  while (error != 0)
    {
      clear ();

      drawbanner (&banner, 1, 1);

      mvaddstr (5, 1, "Please enter your email address.");
      mvaddstr (6, 1, "This is sent _nowhere_ but will be used if you ask"
        " the sysadmin for lost");
      mvaddstr (7, 1, "password help. Please use a correct one. It only"
        " benefits you.");
      mvaddstr (8, 1, "80 character max. No ':' characters. Blank line"
        " aborts.");
      mvaddstr (10, 1, "=> ");

      if (error == 1)
        {
          mvaddstr (12, 1, "There was a problem with your last entry.");
          move (10, 4);
        }

      refresh ();
      if (mygetnstr (buf, 80, 1) != OK)
	  buf[0] = 0;

      if (check_email (buf))
        error = 0;
      else
        error = 1;
 
      if (*buf == '\0')
      {
        free (me->username);
        free (me->password);
        free (me);
        me = NULL;
        return;
      }
    }

  me->email = strdup (buf);
  me->env = calloc (1, 1);

  loggedin = 1;

  setproctitle ("%s", me->username);

  dgl_exec_cmdqueue(globalconfig.cmdqueue[DGLTIME_REGISTER], 0, me);

  writefile (1);
}

/* ************************************************************* */

int
passwordgood (char *cpw)
{
  assert (me != NULL);

  if (!strncmp (crypt (cpw, cpw), me->password, DGL_PASSWDLEN))
    return 1;
  if (!strncmp (cpw, me->password, DGL_PASSWDLEN))
    return 1;

  return 0;
}

/* ************************************************************* */

int
readfile (int nolock)
{
#ifndef USE_SQLITE3
  FILE *fp = NULL, *fpl = NULL;
  char buf[1200];
  struct flock fl = { 0 };

  fl.l_type = F_RDLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  memset (buf, 1024, 0);

  /* read new stuff */

  if (!nolock)
    {
      fpl = fopen (globalconfig.lockfile, "r");
      if (!fpl) {
	  debug_write("cannot fopen lockfile");
        graceful_exit (106);
      }
      if (fcntl (fileno (fpl), F_SETLKW, &fl) == -1) {
	  debug_write("cannot fcntl lockfile");
        graceful_exit (114);
      }
    }

  fp = fopen (globalconfig.passwd, "r");
  if (!fp) {
      debug_write("cannot fopen passwd file");
    graceful_exit (106);
  }

  /* once per name in the file */
  while (fgets (buf, 1200, fp))
    {
      char *b = buf, *n = buf;

      users = realloc (users, sizeof (struct dg_user *) * (f_num + 1));
      users[f_num] = malloc (sizeof (struct dg_user));
      users[f_num]->username = (char *) calloc (DGL_PLAYERNAMELEN+2, sizeof (char));
      users[f_num]->email = (char *) calloc (82, sizeof (char));
      users[f_num]->password = (char *) calloc (DGL_PASSWDLEN+2, sizeof (char));
      users[f_num]->env = (char *) calloc (1026, sizeof (char));

      /* name field, must be valid */
      while (*b != ':')
        {
          if (!isalnum((int)*b))
            return 1;
          users[f_num]->username[(b - n)] = *b;
          b++;
          if ((b - n) >= DGL_PLAYERNAMELEN) {
	      debug_write("name field too long");
            graceful_exit (100);
	  }
        }

      /* advance to next field */
      n = b + 1;
      b = n;

      /* email field */
      while (*b != ':')
        {
          users[f_num]->email[(b - n)] = *b;
          b++;
          if ((b - n) > 80) {
	      debug_write("email field too long");
            graceful_exit (101);
	  }
        }

      /* advance to next field */
      n = b + 1;
      b = n;

      /* pw field */
      while (*b != ':')
        {
          users[f_num]->password[(b - n)] = *b;
          b++;
          if ((b - n) >= DGL_PASSWDLEN) {
	      debug_write("passwd field too long");
            graceful_exit (102);
	  }
        }

      /* advance to next field */
      n = b + 1;
      b = n;

      /* env field */
      while ((*b != '\n') && (*b != 0) && (*b != EOF))
        {
          users[f_num]->env[(b - n)] = *b;
          b++;
          if ((b - n) >= 1024) {
	      debug_write("env field too long");
            graceful_exit (102);
	  }
        }

      f_num++;
      /* prevent a buffer overrun here */
      if (f_num > globalconfig.max)
      {
	fprintf(stderr,"ERROR: number of users in database exceeds maximum. Exiting.\n");
	debug_write("too many users in database");
        graceful_exit (109);
      }
    }

  if (!nolock)
      fclose (fpl);
  fclose (fp);
#endif
  return 0;
}

/* ************************************************************* */

#ifndef USE_SQLITE3
struct dg_user *userexist_tmp_me = NULL;

struct dg_user *
userexist (char *cname, int isnew)
{
  int i;

  if (userexist_tmp_me) {
      free(userexist_tmp_me->username);
      free(userexist_tmp_me->email);
      free(userexist_tmp_me->env);
      free(userexist_tmp_me->password);
      free(userexist_tmp_me);
      userexist_tmp_me = NULL;
  }

  for (i = 0; i < f_num; i++)
    {
	if (!strncasecmp (cname, users[i]->username, (isnew ? globalconfig.max_newnick_len : DGL_PLAYERNAMELEN))) {
	    userexist_tmp_me = cpy_me(users[i]);
	    return userexist_tmp_me;
	}
    }

  return NULL;
}
#else

struct dg_user *userexist_tmp_me = NULL;

static int
userexist_callback(void *NotUsed, int argc, char **argv, char **colname)
{
    int i;
    NotUsed = NULL;

    userexist_tmp_me = malloc(sizeof(struct dg_user));

    for (i = 0; i < argc; i++) {
	if (!strcmp(colname[i], "username"))
	    userexist_tmp_me->username = strdup(argv[i]);
	else if (!strcmp(colname[i], "email"))
	    userexist_tmp_me->email = strdup(argv[i]);
	else if (!strcmp(colname[i], "env"))
	    userexist_tmp_me->env = strdup(argv[i]);
	else if (!strcmp(colname[i], "password"))
	    userexist_tmp_me->password = strdup(argv[i]);
	else if (!strcmp(colname[i], "flags"))
	    userexist_tmp_me->flags = atoi(argv[i]);
	else if (!strcmp(colname[i], "id"))
	    userexist_tmp_me->id = atoi(argv[i]);
    }
    return 0;
}

struct dg_user *
userexist (char *cname, int isnew)
{
    sqlite3 *db;
    char *errmsg = NULL;
    int ret, retry = 10;

    char *qbuf;

    char tmpbuf[DGL_PLAYERNAMELEN+2];
    strncpy(tmpbuf, cname, (isnew ? globalconfig.max_newnick_len : DGL_PLAYERNAMELEN));

    /* Check that the nick doesn't interfere with already registered nicks */
    if (isnew && (strlen(cname) >= globalconfig.max_newnick_len))
	strcat(tmpbuf, "%");

    qbuf = sqlite3_mprintf("select * from dglusers where username like '%q' limit 1", tmpbuf);

    ret = sqlite3_open(USE_SQLITE_DB, &db); /* FIXME: use globalconfig->passwd? */
    if (ret) {
	sqlite3_close(db);
	debug_write("sqlite3_open failed");
	graceful_exit(109);
    }

    if (userexist_tmp_me) {
	free(userexist_tmp_me->username);
	free(userexist_tmp_me->email);
	free(userexist_tmp_me->env);
	free(userexist_tmp_me->password);
	free(userexist_tmp_me);
	userexist_tmp_me = NULL;
    }

    sqlite3_busy_timeout(db, 10000);
    ret = sqlite3_exec(db, qbuf, userexist_callback, 0, &errmsg);

    sqlite3_free(qbuf);

    if (ret != SQLITE_OK) {
	sqlite3_close(db);
	debug_write("sqlite3_exec failed");
	graceful_exit(108);
    }
    sqlite3_close(db);

    return userexist_tmp_me;
}
#endif

/* ************************************************************* */

void
write_canned_rcfile (int game, char *target)
{
  FILE *canned, *newfile;
  char buf[1024], *rfn;
  size_t bytes, len;

  len = strlen(myconfig[game]->rcfile) + 2;
  rfn = malloc(len);
  snprintf (rfn, len, "/%s", myconfig[game]->rcfile);

  if (!(newfile = fopen (target, "w")))
    {
    bail:
      mvaddstr (13, 1,
                "You don't know how to write that! You write \"%s was here\" and the scroll disappears.");
      mvaddstr (14, 1,
                "(Sorry, but I couldn't open one of the config files. This is a bug.)");
      return;
    }

  if (!(canned = fopen (rfn, "r")))
    goto bail;

  free(rfn);

  while ((bytes = fread (buf, 1, 1024, canned)) > 0)
    {
      if (fwrite (buf, 1, bytes, newfile) != bytes)
        {
          if (ferror (newfile))
            {
              mvaddstr (13, 1, "Your hand slips while engraving.");
              mvaddstr (14, 1,
                        "(Encountered a problem writing the new file. This is a bug.)");
              fclose (canned);
              fclose (newfile);
              return;
            }
        }
    }

  fclose (canned);
  fclose (newfile);
  chmod (target, default_fmode);
}


void
editoptions (int game)
{
  FILE *rcfile;
  char *myargv[3];
  pid_t editor;

  rcfile = fopen (dgl_format_str(game, me, myconfig[game]->rc_fmt, NULL), "r");
  if (!rcfile)
      write_canned_rcfile (game, dgl_format_str(game, me, myconfig[game]->rc_fmt, NULL));

  /* use whatever editor_main to edit */

  myargv[0] = "";
  myargv[1] = dgl_format_str(game, me, myconfig[game]->rc_fmt, NULL);
  myargv[2] = 0;

  endwin ();

  editor = fork();

  if (editor == -1)
  {
    perror("fork");
    debug_write("edit fork failed");
    graceful_exit(114);
  }
  else if (editor == 0)
  {
    editor_main (2, myargv);
    exit(0);
  }
  else
    waitpid(editor, NULL, 0);

  refresh ();
  check_retard(1);
}

/* ************************************************************* */

#ifndef USE_SQLITE3
void
writefile (int requirenew)
{
  FILE *fp, *fpl;
  int i = 0;
  int my_done = 0;
  struct flock fl = { 0 };
  sigset_t oldmask, toblock;

  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  sigemptyset(&toblock);
  sigaddset(&toblock, SIGHUP);
  sigaddset(&toblock, SIGINT);
  sigaddset(&toblock, SIGQUIT);
  sigaddset(&toblock, SIGTERM);
  sigprocmask(SIG_BLOCK, &toblock, &oldmask);

  fpl = fopen (globalconfig.lockfile, "r+");
  if (!fpl)
    {
      sigprocmask(SIG_SETMASK, &oldmask, NULL);
      debug_write("writefile locking failed");
      graceful_exit (115);
    }
  if (fcntl (fileno (fpl), F_SETLK, &fl))
    {
      sigprocmask(SIG_SETMASK, &oldmask, NULL);
      debug_write("writefile fcntl failed");
      graceful_exit (107);
    }

  fl.l_type = F_UNLCK;

  freefile ();
  readfile (1);

  fp = fopen (globalconfig.passwd, "w");
  if (!fp)
    {
      sigprocmask(SIG_SETMASK, &oldmask, NULL);
      debug_write("passwd file fopen failed");
      graceful_exit (104);
    }

  for (i = 0; i < f_num; i++)
    {
      if (loggedin && !strncmp (me->username, users[i]->username, DGL_PLAYERNAMELEN))
        {
          if (requirenew)
            {
              /* this is if someone managed to register at the same time
               * as someone else. just die. */
	      fclose(fp);
	      fclose(fpl);
              sigprocmask(SIG_SETMASK, &oldmask, NULL);
	      debug_write("two users registering at the same time");
              graceful_exit (111);
            }
          fprintf (fp, "%s:%s:%s:%s\n", me->username, me->email, me->password,
                   me->env);
          my_done = 1;
        }
      else
        {
          fprintf (fp, "%s:%s:%s:%s\n", users[i]->username, users[i]->email,
                   users[i]->password, users[i]->env);
        }
    }
  if (loggedin && !my_done)
    {                           /* new entry */
      if (f_num < globalconfig.max)
        fprintf (fp, "%s:%s:%s:%s\n", me->username, me->email, me->password,
                 me->env);
      else /* Oops, someone else registered the last available slot first */
	{
          fclose(fp);
	  fclose(fpl);
          sigprocmask(SIG_SETMASK, &oldmask, NULL);
	  debug_write("too many users in passwd db already");
          graceful_exit (116);
	}
    }

  fclose (fp);
  fclose (fpl);

  sigprocmask(SIG_SETMASK, &oldmask, NULL);
}
#else
void
writefile (int requirenew)
{
    sqlite3 *db;
    char *errmsg = NULL;
    int ret, retry = 10;

    char *qbuf;

    if (requirenew) {
	qbuf = sqlite3_mprintf("insert into dglusers (username, email, env, password, flags) values ('%q', '%q', '%q', '%q', %li)", me->username, me->email, me->env, me->password, me->flags);
    } else {
	qbuf = sqlite3_mprintf("update dglusers set username='%q', email='%q', env='%q', password='%q', flags=%li where id=%i", me->username, me->email, me->env, me->password, me->flags, me->id);
    }

    ret = sqlite3_open(USE_SQLITE_DB, &db); /* FIXME: use globalconfig->passwd? */
    if (ret) {
	sqlite3_close(db);
	debug_write("writefile sqlite3_open failed");
	graceful_exit(107);
    }

    sqlite3_busy_timeout(db, 10000);
    ret = sqlite3_exec(db, qbuf, NULL, NULL, &errmsg);

    sqlite3_free(qbuf);

    if (ret != SQLITE_OK) {
	sqlite3_close(db);
	debug_write("writefile sqlite3_exec failed");
	graceful_exit(106);
    }
    sqlite3_close(db);
}
#endif

/* ************************************************************* */

/* ************************************************************* */

/* ************************************************************* */
/* ************************************************************* */
/* ************************************************************* */


int
purge_stale_locks (int game)
{
  DIR *pdir;
  struct dirent *dent;
  char* dir;
  size_t len;
  short firsttime = 1;

  dir = strdup(dgl_format_str(game, me, myconfig[game]->inprogressdir, NULL));

  if (!(pdir = opendir (dir))) {
      debug_write("purge_stale_locks dir open failed");
    graceful_exit (200);
  }

  free(dir);

  while ((dent = readdir (pdir)) != NULL)
    {
      FILE *ipfile;
      char *colon, *fn;
      char buf[16];
      pid_t pid;
      size_t len;
      int seconds = 0;

      if (!strcmp (dent->d_name, ".") || !strcmp (dent->d_name, ".."))
        continue;

      colon = strchr (dent->d_name, ':');
      /* should never happen */
      if (!colon) {
	  debug_write("purge_stale_locks !colon");
        graceful_exit (201);
      }
      if (colon - dent->d_name != strlen(me->username))
        continue;
      if (strncmp (dent->d_name, me->username, colon - dent->d_name))
        continue;

      len = strlen (dent->d_name) + strlen(dgl_format_str(game, me, myconfig[game]->inprogressdir, NULL)) + 1;
      fn = malloc (len);

      snprintf (fn, len, "%s%s", dgl_format_str(game, me, myconfig[game]->inprogressdir, NULL), dent->d_name);

      if (!(ipfile = fopen (fn, "r"))) {
	  debug_write("purge_stale_locks fopen inprogressdir fail");
        graceful_exit (202);
      }

      if (fgets (buf, 16, ipfile) == NULL) {
	  debug_write("purge_stale_locks fgets ipfile fail");
        graceful_exit (203);
      }

      fclose (ipfile);

      if (firsttime)
      {
	clear ();
	drawbanner (&banner, 1, 1);

#define HUP_WAIT 10 /* seconds before HUPPING */
	mvprintw (3, 1,
	    "There are some stale %s processes, will recover in %d  seconds.",
            myconfig[game]->game_name, HUP_WAIT);
	mvaddstr (4, 1,
	    "Press a key NOW if you don't want this to happen!");

	move (3, 51 + strlen(myconfig[game]->game_name)); /* pedantry */
	halfdelay(10);

	for (seconds = HUP_WAIT - 1; seconds >= 0; seconds--)
	{
	  if (dgl_getch() != ERR)
	  {
	    nocbreak(); /* leave half-delay */
	    cbreak();
	    return 0;
	  }
	  mvprintw (3, 50 + strlen(myconfig[game]->game_name), "%d%s", seconds, (seconds > 9) ? "" : " ");
	}

	nocbreak();
	cbreak();

	firsttime = 0;
      }

      clear ();
      refresh ();

      pid = atoi (buf);

      kill (pid, SIGHUP);

      errno = 0;

      /* Wait for it to stop running */
      seconds = 0;
      while (kill (pid, 0) == 0)
        {
          seconds++;
          sleep (1);
          if (seconds == 10)
            {
              mvprintw (3, 1,
                        "Couldn't terminate one of your stale %s processes gracefully.", myconfig[game]->game_name);
              mvaddstr (4, 1, "Force its termination? [yn] ");
              if (tolower (dgl_getch ()) == 'y')
                {
                  kill (pid, SIGTERM);
                  break;
                }
              else
                {
                  endwin ();
                  fprintf (stderr, "Sorry, no %s for you now, please "
                           "contact the admin.\n", myconfig[game]->game_name);
		  debug_write("could not terminate stale processes");
                  graceful_exit (1);
                }
            }
        }

      /* Don't remove the lock file until the process is dead. */
      unlink (fn);
      free (fn);
    }

  closedir (pdir);
  return 1;
}


int
runmenuloop(struct dg_menu *menu)
{
    struct dg_banner ban;
    struct dg_menuoption *tmpopt;
    int userchoice = 0;
    int doclear = 1;

    if (!menu) return 1;

    ban.lines = NULL;
    ban.len = 0;

    idle_alarm_set_enabled(1);
    loadbanner(menu->banner_fn, &ban);
    while (1) {
	if (doclear) {
	    doclear = 0;
	    clear();
	}
	drawbanner(&ban, 1, 0);
	if (menu->cursor_x >= 0 && menu->cursor_y >= 0)
	    mvprintw(menu->cursor_y, menu->cursor_x, "");
	refresh();
	userchoice = dgl_getch();
	if (userchoice == ERR) return 1;
	tmpopt = menu->options;
	while (tmpopt) {
	    if (strchr(tmpopt->keys, userchoice)) {
		dgl_exec_cmdqueue(tmpopt->cmdqueue, selected_game, me);
		doclear = 1;
		break;
	    } else {
		tmpopt = tmpopt->next;
	    }
	}

	if (return_from_submenu) {
	    return_from_submenu = 0;
	    return 0;
	}

	if (check_retard(0)) {
	    debug_write("retard");
	    graceful_exit(119);
	}
    }
}


int
authenticate ()
{
  int i, len, me_index;
  char user_buf[DGL_PLAYERNAMELEN+1], pw_buf[DGL_PASSWDLEN+1];
  struct dg_game **games = NULL;

  /* We use simple password authentication, rather than challenge/response. */
  printf ("\n");
  fflush(stdout);

  fgets (user_buf, sizeof(user_buf), stdin);
  len = strlen (user_buf);
  if (user_buf[len - 1] == '\n')
    user_buf[--len] = '\0';
  else
    {
	fprintf (stderr, "Username too long (max %i chars).\n", DGL_PLAYERNAMELEN);
      return 1;
    }

  fgets (pw_buf, sizeof(pw_buf), stdin);
  len = strlen (pw_buf);
  if (pw_buf[len - 1] == '\n')
    pw_buf[--len] = '\0';
  else
    {
	fprintf (stderr, "Password too long (max %i chars).\n", DGL_PASSWDLEN);
      return 1;
    }

  {
      struct dg_user *tmpme;
      if ((tmpme = userexist(user_buf, 0))) {
	  me = cpy_me(tmpme);
      if (passwordgood (pw_buf))
        {
	    games = populate_games (-1, &len, me);
	  for (i = 0; i < len; i++)
	    if (!strcmp (games[i]->name, user_buf))
	      {
		fprintf (stderr, "Game already in progress.\n");
		free_populated_games(games, len);
		return 1;
	      }
	  win.ws_row = win.ws_col = 0;
	  gen_inprogress_lock (0, getppid (), gen_nhext_filename ());
	  free_populated_games(games, len);
	  return 0;
	}
      }
  }

  sleep (2);
  fprintf (stderr, "Login failed.\n");
  free_populated_games(games, len);
  return 1;
}

int
main (int argc, char** argv)
{
  /* for chroot and program execution */
    char atrcfilename[81], *p, *auth = NULL;
  unsigned int len;
  int c, i;
  int nhext = 0, nhauth = 0;
  int userchoice;
  char *tmp;
  char *wall_email_str = NULL;
#ifdef USE_RLIMIT
  struct rlimit lim;
#endif

#ifndef HAVE_SETPROCTITLE
  /* save argc, argv */
  char** saved_argv;
  int saved_argc;

  saved_argc = argc;

  saved_argv = malloc(sizeof(char**) * (argc + 1));
  for (i = 0; i < argc; i++)
    saved_argv[i] = strdup(argv[i]);
  saved_argv[i] = '\0';
  
  compat_init_setproctitle(argc, argv);
  argv = saved_argv;
#endif

  p = getenv("USER");

  /* Linux telnetd allows importing the USER envvar via telnet,
   * while FreeBSD does not.  FreeBSD, on the other hand, does allow
   * the LOGNAME envvar.  Check USER first, then LOGNAME.
   */
  if (p == NULL) {
      p = getenv("LOGNAME");
  }

  if (p && *p != '\0')
    auth = strdup(p);
  /* else auth is still NULL */

  __progname = basename(strdup(argv[0]));

  while ((c = getopt(argc, argv, "qh:pf:aeW:")) != -1)
  {
    switch (c)
    {
      case 'q':
	silent = 1; break;

      case 'a':
	nhauth = 1; break;

      case 'e':
	nhext = 1; break;

      case 'f':
	if (config)
	{
	  if (!silent)
	    fprintf(stderr, "warning: using %s\n", argv[optind]);
	  free(config);
	}

	config = strdup(optarg);
	break;

    case 'W':
	wall_email_str = strdup(optarg);
	break;

      default:
	break; /*ignore */
    }
  }

  while (optind < argc)
  {
    size_t len = strlen(argv[optind]);
    memset(argv[optind++], 0, len);
  }
  setproctitle("<Anonymous>");

  srand(time(0));

  create_config();

  /* signal handlers */
  signal (SIGHUP, catch_sighup);
  signal(SIGWINCH, sigwinch_func);

  (void) tcgetattr (0, &tt);
  if (-1 == ioctl (0, TIOCGWINSZ, (char *) &win) || win.ws_row < 4 ||
		  win.ws_col < 4) /* Rudimentary validity check */
    {
      win.ws_row = 24;
      win.ws_col = 80;
      win.ws_xpixel = win.ws_col * 8;
      win.ws_ypixel = win.ws_row * 8;
    }

  /* get master tty just before chroot (lives in /dev) */
  if (!nhext && !nhauth)
    ttyrec_getpty ();

#ifdef USE_RLIMIT
#ifdef USE_RLIMIT_CORE
  /* enable and set core dump size */
  if (!getrlimit(RLIMIT_CORE, &lim)) {
      lim.rlim_cur = USE_RLIMIT_CORE;
      setrlimit(RLIMIT_CORE, &lim);
  }
#endif
#ifdef USE_RLIMIT_AS
  /* set maximum memory usage */
  if (!getrlimit(RLIMIT_AS, &lim)) {
      lim.rlim_cur = USE_RLIMIT_AS;
      setrlimit(RLIMIT_AS, &lim);
  }
#endif
#endif

  if (geteuid () != globalconfig.shed_uid)
    {
      /* chroot */
      if (chroot (globalconfig.chroot))
	{
	  perror ("cannot change root directory");
	  graceful_exit (1);
	}

      if (chdir ("/"))
	{
	  perror ("cannot chdir to root directory");
	  graceful_exit (1);
	}

      /* shed privs. this is done immediately after chroot. */
      if (setgroups (1, &globalconfig.shed_gid) == -1)
	{
	  perror ("setgroups");
	  graceful_exit (1);
	}

      if (setgid (globalconfig.shed_gid) == -1)
	{
	  perror ("setgid");
	  graceful_exit (1);
	}

      if (setuid (globalconfig.shed_uid) == -1)
	{
	  perror ("setuid");
	  graceful_exit (1);
	}
    }

  if (wall_email_str) {
      char *emailfrom = wall_email_str;
      char *emailmsg = strchr(wall_email_str, ':');
      if (!emailmsg) {
	  debug_write("wall: no mail msg");
	  graceful_exit(117);
      }
      *emailmsg = '\0';
      emailmsg++;
      if (emailmsg)
	  wall_email(emailfrom, emailmsg);
      graceful_exit(0);
  }


  loadbanner(globalconfig.banner, &banner);

  dgl_exec_cmdqueue(globalconfig.cmdqueue[DGLTIME_DGLSTART], 0, NULL);

  if (nhext)
    {
      char *myargv[3];

      myargv[0] = myconfig[0]->game_path;
      myargv[1] = "--proxy";
      myargv[2] = 0;

      execvp (myconfig[0]->game_path, myargv);
      perror (myconfig[0]->game_path);
      graceful_exit (1);
    }

  /* simple login routine, uses ncurses */
  if (readfile (0)) {
      debug_write("log in fail");
    graceful_exit (110);
  }

  if (nhauth) {
      debug_write("nhauth");
    graceful_exit (authenticate ());
  }

  if (auth)
  {
    char *user, *pass, *p;

    p = strchr(auth, ':');

    if (p)
    {
      pass = p + 1;

      if (*pass != '\0')
      {
        *p = '\0';
        user = auth;
        autologin(user, pass);
      }
    }
  }

  initcurses ();
  term_resize_check();

  while (1) {
      if (runmenuloop(dgl_find_menu(loggedin ? "mainmenu_user" : "mainmenu_anon")))
	  break;
  }

  /* NOW we can safely kill this */
  freefile ();

  if (me)
    free (me);

  graceful_exit (1);

  return 1;
}
