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

#include <stdlib.h>
#include <curses.h>

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

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
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

#include "y.tab.h"
extern FILE* yyin;
extern int yyparse ();

extern int editor_main (int argc, char **argv);

/* global variables */

char rcfilename[80];

int f_num = 0;
struct dg_user **users = NULL;
struct dg_user *me = NULL;
struct dg_banner banner;

#if !(defined(__linux__) || defined(BSD))
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
#endif /* !linux && !bsd */

/* ************************************************************* */
/* for ttyrec */

void
ttyrec_getpty ()
{
#ifdef USE_OPENPTY
  if (openpty (&master, &slave, NULL, NULL, NULL) == -1)
    graceful_exit (62);
#else
  if ((master = open ("/dev/ptmx", O_RDWR)) < 0)
    graceful_exit (62);
  grantpt (master);
  unlockpt (master);
  if ((slave = open ((const char *) ptsname (master), O_RDWR)) < 0)
    {
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
gen_inprogress_lock (pid_t pid, char* ttyrec_filename)
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

  len = strlen(myconfig->dglroot) + strlen(me->username) + strlen(ttyrec_filename) + 13;
  lockfile = calloc(len, sizeof(char));
  
  snprintf (lockfile, len, "%sinprogress/%s:%s", myconfig->dglroot,
            me->username, ttyrec_filename);

  fd = open (lockfile, O_WRONLY | O_CREAT, 0644);
  if (fcntl (fd, F_SETLKW, &fl) == -1)
    graceful_exit (68);

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
  graceful_exit (2);
}

/* ************************************************************* */

void
loadbanner (struct dg_banner *ban)
{
  FILE *bannerfile;
  char buf[80];

  memset (buf, 0, 80);

  bannerfile = fopen (myconfig->banner, "r");

  if (!bannerfile)
    {
      size_t len;
      ban->len = 2;
      ban->lines = malloc (sizeof (char *));
      ban->lines[0] =
        strdup ("### dgamelaunch " VERSION
                " - network console game launcher");
      len = strlen(myconfig->banner) + ARRAY_SIZE("### NOTE: administrator has not installed a  file");
      ban->lines[1] = malloc(len);
      snprintf(ban->lines[1], len, "### NOTE: administrator has not installed a %s file", myconfig->banner);
      return;
    }

  ban->len = 0;

  while (fgets (buf, 80, bannerfile) != NULL)
    {
      char *loc, *b = buf;
      char bufnew[80];
      
      memset (bufnew, 0, 80);

      ban->len++;
      ban->lines = realloc (ban->lines, sizeof (char *) * ban->len);

      if (strstr(b, "$VERSION"))
      {
	int i = 0; 
	while ((loc = strstr (b, "$VERSION")) != NULL)
	{
          for (; i < 80; i++)
            {
              if (loc != b)
                bufnew[i] = *(b++);
              else
                {
                  strlcat (bufnew, VERSION, 80);
                  b += 8;       /* skip the whole $VERSION string */
                  i += ARRAY_SIZE (VERSION) - 1;
		  break;
                }

              if (strlen (b) == 0)
                break;
	    }
	}
        
	if (*b)
	  strlcat(bufnew, b, 80);
	
	ban->lines[ban->len - 1] = strdup (bufnew);
      }
      else
        ban->lines[ban->len - 1] = strdup (buf);

      memset (buf, 0, 80);

      if (ban->len == 11)       /* menu itself needs 13 lines, 24 - 11 */
         break;
  }

  fclose (bannerfile);
}

void
drawbanner (unsigned int start_line, unsigned int howmany)
{
  static short loaded_banner = 0;
  unsigned int i;

  if (!loaded_banner)
    {
      loadbanner (&banner);
      loaded_banner = 1;
    }

  if (howmany > banner.len || howmany == 0)
    howmany = banner.len;

  for (i = 0; i < howmany; i++)
    mvaddstr (start_line + i, 1, banner.lines[i]);
}

void
inprogressmenu ()
{
  int i, menuchoice, len = 20, offset = 0, doresizewin = 0;
  time_t ctime;
  struct dg_game **games;
  char ttyrecname[130], *replacestr = NULL;
  sigset_t oldmask, toblock;

  games = populate_games (&len);

  while (1)
    {
      if (len == 0)
        offset = 0;
      else
        {
	  while (offset >= len && offset >= 14)
	    offset -= 14;
	}

      clear ();
      drawbanner (1, 1);
      mvprintw (3, 1,
                "During playback, hit 'q' to return here, 'm' to send mail (requires login),");
      mvaddstr (4, 1,
                "'s' to toggle graphic-set stripping for DEC, IBM, and none.");
      mvaddstr (5, 1, "The following games are in progress: (use uppercase to try to change size)");

      /* clean old games and list good ones */
      i = 0;

      for (i = 0; i < 14; i++)
        {
          if (i + offset >= len)
            break;

          mvprintw (7 + i, 1, "%c) %-15s (%3dx%3d) %s %s (%ldm %lds idle)",
                    i + 97, games[i + offset]->name,
		    games[i + offset]->ws_col, games[i + offset]->ws_row,
                    games[i + offset]->date, games[i + offset]->time,
                    (time (&ctime) - games[i + offset]->idle_time) / 60,
                    (time (&ctime) - games[i + offset]->idle_time) % 60);
        }

      if (len > 0)
        mvprintw (21, 1, "(%d-%d of %d)", offset + 1, offset + i, len);
      mvaddstr (23, 1,
                "Watch which game? (any key refreshes, 'q' quits, '>'/'<' for more/less) => ");
      refresh ();

      switch ((menuchoice = getch ()))
        {
        case '>':
          if ((offset + 14) >= len)
            break;
          else
            offset += 14;
          break;

        case '<':
          if ((offset - 14) < 0)
            break;
          else
            offset -= 14;
          break;

	case 'q': case 'Q':
          return;

        default:
	  doresizewin = 0;
	  if (isupper (menuchoice))
	    {
	      doresizewin = 1;
	      menuchoice = tolower (menuchoice);
	    }
          if ((menuchoice - 'a') >= 0 && (menuchoice - 'a') < i)
            {
              /* valid choice has been made */
              snprintf (ttyrecname, 130, "%sttyrec/%s", myconfig->dglroot,
                        games[menuchoice - 97 + offset]->ttyrec_fn);
              chosen_name = strdup (games[menuchoice - 97 + offset]->name);

              /* reuse thie char* */
              replacestr = strchr (ttyrecname, ':');

              if (!replacestr)
                graceful_exit (145);

              replacestr[0] = '/';

              clear ();
              refresh ();
              endwin ();
	      if (doresizewin)
	        {
		  /*
		   * Let curses deal with the resize later. Perhaps this is
		   * not the best way.
		   */
		  sigemptyset (&toblock);
		  sigaddset (&toblock, SIGWINCH);
		  sigprocmask (SIG_BLOCK, &toblock, &oldmask);
		  printf ("\033[8;%d;%dt",
		    games[menuchoice - 97 + offset]->ws_row,
		    games[menuchoice - 97 + offset]->ws_col);
		  fflush (stdout);
		}
              ttyplay_main (ttyrecname, 1);
              initcurses ();
	      if (doresizewin)
	        sigprocmask (SIG_SETMASK, &oldmask, NULL);
            }
        }

      games = populate_games (&len);
    }
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
    drawbanner(1,1);

    mvprintw(3, 1, "Your current email is: %s", me->email);
    mvaddstr(4, 1, "Please enter a new one (max 80 chars; blank line aborts)");
    mvaddstr(6, 1, "=> ");

    mygetnstr (buf, 80, 1);

    if (buf && *buf == '\0')
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
      if (getch() == 'y')
      {
	free(me->email);
	me->email = strdup(buf);
	writefile(0);
	return;
      }
      else
      {
	mvaddstr(9, 1, "No changes made. Press any key to continue...");
	getch();
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
  char buf[21];
  int error = 2;

  /* A precondition is that struct `me' exists because we can be not-yet-logged-in. */
  if (!me)
    graceful_exit (122);        /* Die. */

  while (error)
    {
      char repeatbuf[21];
      clear ();

      drawbanner (1, 1);

      mvprintw (5, 1,
                "Please enter a%s password. Remember that this is sent over the net",
                loggedin ? " new" : "");
      mvaddstr (6, 1,
                "in plaintext, so make it something new and expect it to be relatively");
      mvaddstr (7, 1, "insecure.");
      mvaddstr (8, 1,
                "20 character max. No ':' characters. Blank line to abort.");
      mvaddstr (10, 1, "=> ");

      if (error == 1)
        {
          mvaddstr (15, 1, "Sorry, the passwords don't match. Try again.");
          move (10, 4);
        }

      refresh ();

      mygetnstr (buf, 20, 0);

      if (buf && *buf == '\0')
        return 0;

      if (strchr (buf, ':') != NULL)
        graceful_exit (112);

      mvaddstr (12, 1, "And again:");
      mvaddstr (13, 1, "=> ");

      mygetnstr (repeatbuf, 20, 0);

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
domailuser (char *username)
{
  unsigned int len, i;
  char *spool_fn, message[81];
  FILE *user_spool = NULL;
  time_t now;
  int mail_empty = 1;
  struct flock fl = { 0 };

  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  assert (loggedin);

  len = strlen(myconfig->spool) + strlen (username) + 1;
  spool_fn = malloc (len + 1);
  time (&now);
  snprintf (spool_fn, len + 1, "%s/%s", myconfig->spool, username);

  /* print the enter your message line */
  clear ();
  drawbanner (1, 1);
  mvaddstr (5, 1,
            "Enter your message here. It is to be one line only and 80 characters or less.");
  mvaddstr (7, 1, "=> ");

  mygetnstr (message, 80, 1);

  for (i = 0; i < strlen (message); i++)
    {
      if (message[i] != ' ' && message[i] != '\n' && message[i] != '\t')
        mail_empty = 0;
    }

  if (mail_empty)
    {
      mvaddstr (9, 1, "This scroll appears to be blank.--More--");
      mvaddstr (10, 1, "(Aborting your message.)");
      getch ();
      return;
    }

  if ((user_spool = fopen (spool_fn, "a")) == NULL)
    {
      mvaddstr (9, 1,
                "You fall into the water! You sink like a rock.--More--");
      mvprintw (10, 1,
                "(I couldn't open %s'%c spool file for some reason, so I'm giving up.)",
                username, (username[strlen (username) - 1] != 's') ? 's' : 0);
      getch ();
      return;
    }

  mvaddstr (9, 1, "Getting a lock on the mailspool...");
  refresh ();

  while (fcntl (fileno (user_spool), F_SETLK, &fl) == -1)
    {
      if (errno != EAGAIN)
        {
          mvaddstr (10, 1,
                    "Received a weird error from fcntl, so I'm giving up.");
          getch ();
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

  mvaddstr (9, 1, "Message sent successfully         ");
  move(9, 26); /* Pedantry! */
  refresh ();
  sleep (2);

  return;
}

void
drawmenu ()
{
  static int flood = 0;

  clear ();

  drawbanner (1, 0);

  if (loggedin)
    {
      mvprintw (banner.len + 2, 1, "Logged in as: %s", me->username);
      mvaddstr (banner.len + 4, 1, "c) Change password");
      mvaddstr (banner.len + 5, 1, "e) Change email address");
      mvaddstr (banner.len + 6, 1, "o) Edit option file");
      mvaddstr (banner.len + 7, 1, "w) Watch games in progress");
      mvaddstr (banner.len + 8, 1, "p) Play nethack!");
      mvaddstr (banner.len + 9, 1, "q) Quit");
      mvaddstr (banner.len + 11, 1, "=> ");
    }
  else
    {
      mvaddstr (banner.len + 2, 1, "Not logged in.");
      mvaddstr (banner.len + 4, 1, "l) Login");
      mvaddstr (banner.len + 5, 1, "r) Register new user");
      mvaddstr (banner.len + 6, 1, "w) Watch games in progress");
      mvaddstr (banner.len + 7, 1, "q) Quit");
      mvaddstr (banner.len + 9, 1, "=> ");
    }

  refresh ();

  /* for retarded clients */
  flood++;
  if (flood >= 20)
  {
    endwin();
    graceful_exit (119);
  }
}

/* ************************************************************* */

void
freefile ()
{
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
loginprompt (int from_ttyplay)
{
  char user_buf[22], pw_buf[22];
  int error = 2, me_index = -1;

  loggedin = 0;

  while (error)
    {
      clear ();

      drawbanner (1, 1);

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

      mygetnstr (user_buf, 20, 1);

      if (user_buf && *user_buf == '\0')
        return;

      error = 1;

      if ((me_index = userexist (user_buf)) != -1)
        {
          me = users[me_index];
          error = 0;
        }
    }

  clear ();

  drawbanner (1, 1);

  mvaddstr (5, 1, "Please enter your password.");
  mvaddstr (7, 1, "=> ");

  refresh ();

  mygetnstr (pw_buf, 20, 0);

  if (passwordgood (pw_buf))
    {
      loggedin = 1;
      snprintf (rcfilename, 80, "%srcfiles/%s.nethackrc", myconfig->dglroot, me->username);
    }
  else if (from_ttyplay == 1)
  {
    mvaddstr(9, 1, "Login failed. Returning to game.");
    refresh();
    sleep(2);
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

  if (f_num >= myconfig->max)
  {
      clear ();

      drawbanner (1, 1);

      mvaddstr (5, 1, "Sorry, too many users have registered now.");
      mvaddstr (6, 1, "You might email the server administrator.");
      mvaddstr (7, 1, "Press return to return to the menu. ");
      getch ();

      return;
  }

  if (me)
    free (me);

  me = calloc (1, sizeof (struct dg_user));

  while (error)
    {
      clear ();

      drawbanner (1, 1);

      mvaddstr (5, 1, "Welcome new user. Please enter a username.");
      mvaddstr (6, 1,
                "Only characters and numbers are allowed, with no spaces.");
      mvaddstr (7, 1, "20 character max.");
      mvaddstr (9, 1, "=> ");

      if (error == 1)
        {
          mvaddstr (11, 1, "There was a problem with your last entry.");
          move (9, 4);
        }

      refresh ();

      mygetnstr (buf, 20, 1);
      if (userexist (buf) == -1)
        error = 0;
      else
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

      drawbanner (1, 1);

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
      mygetnstr (buf, 80, 1);
 
      if (check_email (buf))
        error = 0;
      else
        error = 1;
 
      if (buf && *buf == '\0')
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

  snprintf (rcfilename, 80, "%srcfiles/%s.nethackrc", myconfig->dglroot, me->username);

  if (access (rcfilename, R_OK) == -1)
    write_canned_rcfile (rcfilename);

  /* create their ttyrec dir */
  snprintf (dirname, 100, "%sttyrec/%s", myconfig->dglroot, me->username);

  if (access (dirname, F_OK) != 0)
    mkdir (dirname, 0755);
  writefile (1);
}

/* ************************************************************* */

int
passwordgood (char *cpw)
{
  if (me == NULL)
    return 1;

  if (!strncmp (crypt (cpw, cpw), me->password, 13))
    return 1;
  if (!strncmp (cpw, me->password, 20))
    return 1;

  return 0;
}

/* ************************************************************* */

int
readfile (int nolock)
{
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
      fpl = fopen (myconfig->lockfile, "r");
      if (!fpl)
        graceful_exit (106);
      if (fcntl (fileno (fpl), F_SETLKW, &fl) == -1)
        graceful_exit (114);
    }

  fp = fopen (myconfig->passwd, "r");
  if (!fp)
    graceful_exit (106);

  /* once per name in the file */
  while (fgets (buf, 1200, fp))
    {
      char *b = buf, *n = buf;

      users = realloc (users, sizeof (struct dg_user *) * (f_num + 1));
      users[f_num] = malloc (sizeof (struct dg_user));
      users[f_num]->username = (char *) calloc (22, sizeof (char));
      users[f_num]->email = (char *) calloc (82, sizeof (char));
      users[f_num]->password = (char *) calloc (22, sizeof (char));
      users[f_num]->env = (char *) calloc (1026, sizeof (char));

      /* name field, must be valid */
      while (*b != ':')
        {
          if (!isalnum((int)*b))
            return 1;
          users[f_num]->username[(b - n)] = *b;
          b++;
          if ((b - n) >= 21)
            graceful_exit (100);
        }

      /* advance to next field */
      n = b + 1;
      b = n;

      /* email field */
      while (*b != ':')
        {
          users[f_num]->email[(b - n)] = *b;
          b++;
          if ((b - n) > 80)
            graceful_exit (101);
        }

      /* advance to next field */
      n = b + 1;
      b = n;

      /* pw field */
      while (*b != ':')
        {
          users[f_num]->password[(b - n)] = *b;
          b++;
          if ((b - n) >= 20)
            graceful_exit (102);
        }

      /* advance to next field */
      n = b + 1;
      b = n;

      /* env field */
      while ((*b != '\n') && (*b != 0) && (*b != EOF))
        {
          users[f_num]->env[(b - n)] = *b;
          b++;
          if ((b - n) >= 1024)
            graceful_exit (102);
        }

      f_num++;
      /* prevent a buffer overrun here */
      if (f_num > myconfig->max)
      {
	fprintf(stderr,"ERROR: number of users in database exceeds maximum. Exiting.\n");
        graceful_exit (109);
      }
    }

  if (!nolock)
      fclose (fpl);
  fclose (fp);
  return 0;
}

/* ************************************************************* */

int
userexist (char *cname)
{
  int i;

  for (i = 0; i < f_num; i++)
    {
      if (!strncasecmp (cname, users[i]->username, 20))
        return i;
    }

  return -1;
}

/* ************************************************************* */

void
write_canned_rcfile (char *target)
{
  FILE *canned, *newfile;
  char buf[1024], *rfn;
  size_t bytes, len;

  len = strlen(myconfig->rcfile) + 2;
  rfn = malloc(len);
  snprintf (rfn, len, "/%s", myconfig->rcfile);

  if (!(newfile = fopen (target, "w")))
    {
    bail:
      mvaddstr (13, 1,
                "You don't know how to write that! You write \"%s was here\" and the scroll disappears.");
      mvaddstr (14, 1,
                "(Sorry, but I couldn't open one of the nethackrc files. This is a bug.)");
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
}


void
editoptions ()
{
  FILE *rcfile;
  char *myargv[3];
  pid_t editor;

  rcfile = fopen (rcfilename, "r");
  if (!rcfile)                  /* should not really happen except for old users */
    write_canned_rcfile (rcfilename);

  /* use whatever editor_main to edit */

  myargv[0] = "";
  myargv[1] = rcfilename;
  myargv[2] = 0;

  endwin ();

  editor = fork();

  if (editor == -1)
  {
    perror("fork");
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
}

/* ************************************************************* */

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

  fpl = fopen (myconfig->lockfile, "r+");
  if (!fpl)
    {
      sigprocmask(SIG_SETMASK, &oldmask, NULL);
      graceful_exit (115);
    }
  if (fcntl (fileno (fpl), F_SETLK, &fl))
    {
      sigprocmask(SIG_SETMASK, &oldmask, NULL);
      graceful_exit (107);
    }

  fl.l_type = F_UNLCK;
  
  freefile ();
  readfile (1);

  fp = fopen (myconfig->passwd, "w");
  if (!fp)
    {
      sigprocmask(SIG_SETMASK, &oldmask, NULL);
      graceful_exit (104);
    }

  for (i = 0; i < f_num; i++)
    {
      if (loggedin && !strncmp (me->username, users[i]->username, 20))
        {
          if (requirenew)
            {
              /* this is if someone managed to register at the same time
               * as someone else. just die. */
	      fclose(fp);
	      fclose(fpl);
              sigprocmask(SIG_SETMASK, &oldmask, NULL);
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
      if (f_num < myconfig->max)
        fprintf (fp, "%s:%s:%s:%s\n", me->username, me->email, me->password,
                 me->env);
      else /* Oops, someone else registered the last available slot first */
	{
          fclose(fp);
	  fclose(fpl);
          sigprocmask(SIG_SETMASK, &oldmask, NULL);
          graceful_exit (116);
	}
    }

  fclose (fp);
  fclose (fpl);

  sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

/* ************************************************************* */

/* ************************************************************* */

/* ************************************************************* */
/* ************************************************************* */
/* ************************************************************* */

/*
 * Backup the savefile, if configured.
 * Returns non-zero if successful, otherwise an error message has been
 * given already.
 */
int
backup_savefile (void)
{
  char buf[1024];
  char *f, *p, *end;
  int ispercent = 0, n;
  int in, out;

  f = myconfig->savefilefmt;

  if (*f == '\0')
    return 1;
  if (me == NULL)
    graceful_exit (147);

  p = buf;
  end = buf + sizeof(buf) - 10; /* make sure we can add .bak */
  while (*f)
    {
      if (ispercent)
        {
          switch (*f)
  	  {
  	    case 'u':
  	      snprintf (p, end + 1 - p, "%d", myconfig->shed_uid);
  	      while (*p != '\0')
  	        p++;
  	      break;
  	    case 'n':
  	      snprintf (p, end + 1 - p, "%s", me->username);
  	      while (*p != '\0')
  	        p++;
  	      break;
  	    default:
  	      *p = *f;
  	      if (p < end)
  	        p++;
  	  }
	  ispercent = 0;
	}
      else
        {
	  if (*f == '%')
	    ispercent = 1;
	  else
	    {
	      *p = *f;
	      if (p < end)
	        p++;
	    }
	}
      f++;
    }
  *p = '\0';

  /*fprintf(stderr, "***\n[SAVEFILE=%s]\n***\n", buf);
  sleep(3);*/
  in = open (buf, O_RDONLY);
  if (in == -1)
    {
      if (errno == ENOENT)
        return 1; /* Nothing to back up */
      else
        {
          fprintf (stderr, "Cannot open savefile '%s'\n", buf);
	  perror ("for input");
          return 0;
        }
    }
  strcpy (p, ".bak");
  out = open (buf, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (out == -1)
    {
      close (in);
      fprintf (stderr, "Cannot open backup savefile '%s'\n", buf);
      perror ("for output");
      return 0;
    }

  while ((n = read (in, buf, sizeof(buf))) > 0)
    {
      n = write (out, buf, n);
      if (n < 0)
        break;
    }
  close (out);
  close (in);
  if (n < 0)
    perror ("I/O error while backing up savefile");
  return n >= 0;
}

/* TODO: Some of the messages here (sorry no nethack for you!) are nethack specific
 * as may be some code... don't think so though. Globalize it. */ 
int
purge_stale_locks (void)
{
  DIR *pdir;
  struct dirent *dent;
  char* dir;
  size_t len;
  short firsttime = 1;

  len = strlen(myconfig->dglroot) + ARRAY_SIZE("inprogress/") + 1;
  dir = malloc(len);
  snprintf(dir, len, "%sinprogress/", myconfig->dglroot);
  
  if (!(pdir = opendir (dir)))
    graceful_exit (200);

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
      if (!colon)
        graceful_exit (201);

      if (colon - dent->d_name != strlen(me->username))
        continue;
      if (strncmp (dent->d_name, me->username, colon - dent->d_name))
        continue;

      len = strlen (dent->d_name) + strlen(myconfig->dglroot) + 12;
      fn = malloc (len);

      snprintf (fn, len, "%sinprogress/%s", myconfig->dglroot, dent->d_name);

      if (!(ipfile = fopen (fn, "r")))
        graceful_exit (202);

      if (fgets (buf, 16, ipfile) == NULL)
        graceful_exit (203);

      fclose (ipfile);

      if (firsttime)
      {
	clear ();
	drawbanner (1, 1);

#define HUP_WAIT 10 /* seconds before HUPPING */
	mvprintw (3, 1,
	    "There are some stale Nethack processes, will recover in %d  seconds.", HUP_WAIT);
	mvaddstr (4, 1,
	    "Press a key NOW if you don't want this to happen!");

	move (3, 58); /* pedantry */
	halfdelay(10);
	
	for (seconds = HUP_WAIT - 1; seconds >= 0; seconds--)
	{
	  if (getch() != ERR)
	  {
	    nocbreak(); /* leave half-delay */
	    cbreak();
	    return 0;
	  }
	  mvprintw (3, 57, "%d%s", seconds, (seconds > 9) ? "" : " ");
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
              mvaddstr (3, 1,
                        "Couldn't terminate one of your stale Nethack processes gracefully.");
              mvaddstr (4, 1, "Force its termination? [yn] ");
              if (tolower (getch ()) == 'y')
                {
                  kill (pid, SIGTERM);
                  break;
                }
              else
                {
                  endwin ();
                  fprintf (stderr, "Sorry, no nethack for you now, please "
                           "contact the admin.\n");
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

void
menuloop (void)
{
  int userchoice = 0;
  while ((userchoice != 'p') | (!loggedin))
    {
      drawmenu ();
      userchoice = getch ();
      switch (tolower (userchoice))
        {
        case 'c':
          if (loggedin)
            changepw (1);
          break;
	case 'e':
	  if (loggedin)
	    change_email();
	  break;
        case 'w':
          inprogressmenu ();
          break;
        case 'o':
          if (loggedin)
            editoptions ();
          break;
        case 'q':
          endwin ();
          graceful_exit(0);
          /* break; */
        case 'r':
          if (!loggedin)        /*not visible to loggedin */
            newuser ();
          break;
        case 'l':
          if (!loggedin)        /* not visible to loggedin */
            loginprompt (0);
          break;
        }
    }
}

int
main (int argc, char** argv)
{
  /* for chroot and program execution */
  char atrcfilename[81], *spool;
  unsigned int len;
  int c;

  while ((c = getopt(argc, argv, "qh:pp:f:")) != -1)
  {
    switch (c)
    {
      case 'q':
	silent = 1; break;

      case 'f':
	if (config)
	{
	  if (!silent)
	    fprintf(stderr, "warning: using %s\n", argv[optind]);
	  free(config);
	}
	  
	config = strdup(optarg);
	break;
	
      default:
	break; /*ignore */
    }
  }
  
  create_config();

  /* signal handlers */
  signal (SIGHUP, catch_sighup);

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
  ttyrec_getpty ();

  /* chroot */
  if (chroot (myconfig->chroot))
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
  if (setgroups (1, &myconfig->shed_gid) == -1)
    {
      perror ("setgroups");
      graceful_exit (1);
    }

  if (setgid (myconfig->shed_gid) == -1)
    {
      perror ("setgid");
      graceful_exit (1);
    }

  if (setuid (myconfig->shed_uid) == -1)
    {
      perror ("setuid");
      graceful_exit (1);
    }

  /* simple login routine, uses ncurses */
  if (readfile (0))
    graceful_exit (110);

  initcurses ();
  menuloop();

  assert (loggedin);

  while (!purge_stale_locks())
    menuloop();

  endwin ();
  signal(SIGWINCH, SIG_DFL);

  if (!backup_savefile ())
    graceful_exit (5);

  /* environment */
  snprintf (atrcfilename, 81, "@%s", rcfilename);

  len = strlen(myconfig->spool) + strlen (me->username) + 1;
  spool = malloc (len + 1);
  snprintf (spool, len + 1, "%s/%s", myconfig->spool, me->username);

  mysetenv ("NETHACKOPTIONS", atrcfilename, 1);
  mysetenv ("MAIL", spool, 1);
  mysetenv ("SIMPLEMAIL", "1", 1);

  /* don't let the mail file grow */
  if (access (spool, F_OK) == 0)
    unlink (spool);

  free (spool);

  /* launch program */
  ttyrec_main (me->username, gen_ttyrec_filename());

  /* NOW we can safely kill this */
  freefile ();

  if (me)
    free (me);

  graceful_exit (1);
  return 1;
}
