/* nethacklaunch.c
 *
 * (c)2001-3 M. Drew Streib <dtype@dtype.org>
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
 * See this in program action at http://dtype.org/nethack/
 *
 * This is a little wrapper for nethack (and soon other programs) that
 * will allow them to be run from a telnetd session, chroot, shed privs,
 * make a simple login, then play the game.
 *
 * By default, this thing is also statically compiled, and can thus be
 * run inside of a chroot jail itself if necessary.
 *
 * Yes, I know it is all global variables. Deal with it. The program
 * is very small.
 */

/* a request from the author: please leave some remnance of
 * 'based on dgamelaunch version xxx' in any derivative works, or
 * even keep the line the same altogether. I'm probably happy 
 * to make any changes you need. */

#define VERLINES 7							/* number of lines in this vanity text */
#define VER1 "## dgamelaunch - network console game launcher\n"
#define VER2 "## version 1.3.10\n"
#define VER3 "## \n"
#define VER4 "## (c)2001-3 M. Drew Streib. This program's source is released under the GPL.\n"
#define VER5 "## Send mail to <dtype@dtype.org> for details or a copy of the source code.\n"
#define VER6 "## ** Games on this server are recorded for in-progress viewing and playback!\n"
#define VER7 "## Server info is at http://alt.org/nethack/"

/* ************************************************************* */
/* ************************************************************* */
/* ************************************************************* */

/* program stuff */

#define _XOPEN_SOURCE						/* grantpt, etc. */
#define _BSD_SOURCE							/* setenv */

#include "dgamelaunch.h"
#include <stdlib.h>
#include <curses.h>
#include <crypt.h>
#include <sys/types.h>
#include <sys/file.h>						/* for flock() */
#include <sys/ioctl.h>					/* ttyrec */
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>

extern int vi_main (int argc, char **argv);
extern int ttyplay_main (char *ttyfile, int mode, int rstripgfx);
extern int ttyrec_main (char *);
extern int master;
extern int slave;
extern struct termios tt;
extern struct winsize win;

/* local functions */
static void writefile (int);
static void write_canned_rcfile (char *);
static int userexist (char *);
static int passwordgood (char *, char *);

/* global variables */

int caught_sighup = 0;
int pid_game = 0;
int loggedin = 0;
char rcfilename[80];
char ttyrec_filename[100];
char *chosen_name;

/* preallocate this mem. bad, but ohwell. is only for pointers */
/* makes a max number of users compiled in */
int f_num = 0;
struct dg_user **users = NULL;
struct dg_user *me = NULL;

/* ************************************************************* */
/* for ttyrec */

void
ttyrec_getmaster ()
{
	(void) tcgetattr (0, &tt);
	(void) ioctl (0, TIOCGWINSZ, (char *) &win);
	if ((master = open ("/dev/ptmx", O_RDWR)) < 0)
		{
			exit (62);
		}
}

/* ************************************************************* */

void
gen_ttyrec_filename ()
{
	time_t rawtime;
	struct tm *ptm;

	/* append time to filename */
	time (&rawtime);
	ptm = gmtime (&rawtime);
	snprintf (ttyrec_filename, 100, "%04i-%02i-%02i.%02i:%02i:%02i.ttyrec",
						ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday,
						ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
}

/* ************************************************************* */

void
gen_inprogress_lock ()
{
	char lockfile[130];
	int fd;

	snprintf (lockfile, 130, "%s%s:%s", LOC_INPROGRESSDIR,
						me->username, ttyrec_filename);

	fd = open (lockfile, O_WRONLY | O_CREAT, 0644);
	if (flock (fd, LOCK_EX))
		exit (68);
}

/* ************************************************************* */

void
catch_sighup ()
{
	caught_sighup = 1;
	if (pid_game)
		{
			sleep (10);
			kill (pid_game, SIGHUP);
			sleep (5);
		}
	exit (2);
}

/* ************************************************************* */

void
inprogressmenu ()
{
	int i;
	DIR *pdir;
	struct dirent *pdirent;
	int fd;
	struct stat pstat;
	char fullname[130];
	char ttyrecname[130];
	char *replacestr;
	char *games[15];
	char m_name[26], m_date[11], m_time[9];
	int m_namelen;
	time_t ctime;
	int menuchoice;


	do
		{
			clear ();
			mvaddstr (1, 1, VER1);
			mvaddstr (3, 1,
								"(Press 'q' during game playback to return to this menu.)");
			mvaddstr (4, 1,
								"(Use capital letter of selection to strip DEC graphics, VERY experimental!)");
			mvaddstr (5, 1, "The following games are in progress:");

			/* clean old games and list good ones */
			i = 0;
			pdir = opendir (LOC_INPROGRESSDIR);
			if (!pdir)
				exit (140);

			while ((pdirent = readdir (pdir)) && (i <= 14))
				{
					snprintf (fullname, 130, "%s%s", LOC_INPROGRESSDIR,
										pdirent->d_name);

					fd = 0;
					fd = open (fullname, O_RDONLY);
					if ((fd > 0) && flock (fd, LOCK_EX | LOCK_NB))
						{

							/* stat to check idle status */
							snprintf (ttyrecname, 130, "%s%s", LOC_TTYRECDIR,
												pdirent->d_name);
							replacestr = strstr (ttyrecname, ":");
							if (!replacestr)
								exit (145);
							replacestr[0] = '/';
							if (!stat (ttyrecname, &pstat))
								{
									games[i] = pdirent->d_name;

									memset (m_name, 0, 26);
									memset (m_date, 0, 11);
									memset (m_time, 0, 9);
									m_namelen =
										replacestr - ttyrecname - strlen (LOC_TTYRECDIR);
									strncpy (m_name, pdirent->d_name, m_namelen);
									strncpy (m_date, replacestr + 1, 10);
									strncpy (m_time, replacestr + 12, 8);

									mvprintw (7 + i, 1, "%c) %-15s %s %s (%ldm %lds idle)",
														i + 97, m_name, m_date, m_time,
														(time (&ctime) - pstat.st_mtime) / 60,
														(time (&ctime) - pstat.st_mtime) % 60);
									i++;
								}
						}
					else
						{
							unlink (fullname);
						}
					flock (fd, LOCK_UN | LOCK_NB);
					close (fd);
				}

			mvaddstr (23, 1, "Watch which game? (r to refresh, q to quit) => ");
			refresh ();

			menuchoice = tolower (getch ());

			if ((menuchoice - 97) >= 0 && (menuchoice - 97) < i)
				{
					/* valid choice has been made */
					snprintf (ttyrecname, 130, "%s%s", LOC_TTYRECDIR,
										games[menuchoice - 97]);
					chosen_name = strdup (games[menuchoice - 97]);
					if (!(replacestr = strstr (chosen_name, ":")))
						exit (145);
					else
						*replacestr = '\0';
					replacestr = strstr (ttyrecname, ":");
					if (!replacestr)
						exit (145);
					replacestr[0] = '/';

					clear ();
					refresh ();
					endwin ();
					ttyplay_main (ttyrecname, 1, 0);
				}

			closedir (pdir);
		}
	while (menuchoice != 'q');
}

/* ************************************************************* */

void
changepw ()
{
	char buf[21];
	int error = 2;

	if (!loggedin)
		return;

	while (error)
		{
			char repeatbuf[21];
			clear ();

			mvaddstr (1, 1, VER1);

			mvaddstr (5, 1,
								"Please enter a new password. Remember that this is sent over the net");
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
			getnstr (buf, 20);

			if (buf && *buf == '\0')
				return;

			if (strstr (buf, ":") != NULL)
				exit (112);

			mvaddstr (12, 1, "And again:");
			mvaddstr (13, 1, "=> ");

			getnstr (repeatbuf, 20);

			if (!strcmp (buf, repeatbuf))
				error = 0;
			else
				error = 1;
		}

	me->password = strdup (crypt (buf, buf));
	writefile (0);
}

/* ************************************************************* */

void
domailuser (char *username)
{
	unsigned int len, i;
	char *spool_fn, message[80];
	FILE *user_spool = NULL;
	time_t now;
	int mail_empty = 1;
	struct flock fl = { F_WRLCK, SEEK_SET, 0, 0, getpid () };

	assert (loggedin);

	len =
		(sizeof (LOC_SPOOLDIR) / sizeof (LOC_SPOOLDIR[0])) + strlen (username) +
		1;
	spool_fn = malloc (len + 1);
	time (&now);
	snprintf (spool_fn, len, "%s/%s", LOC_SPOOLDIR, username);

	/* print the enter your message line */
	clear ();
	mvaddstr (1, 1, VER1);
	mvaddstr (5, 1,
						"Enter your message here. It is to be one line only and 80 characters or less.");
	mvaddstr (7, 1, "=> ");

	getnstr (message, 80);

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

	while (fcntl (fileno (user_spool), F_SETLK, &fl) == -1);

	fprintf (user_spool, "%s:%s\n", me->username, message);

	fl.l_type = F_UNLCK;

	if (fcntl (fileno (user_spool), F_UNLCK, &fl) == -1)
		mvaddstr (10, 1, "Couldn't unlock the file! Oh well.");

	fclose (user_spool);

	return;
}

void
mailuser ()
{
	char buf[20], *user = NULL;
	int error = 2;

	clear ();
	mvaddstr (1, 1, VER1);
	mvaddstr (5, 1,
						"To whom would you like to send a message? (Blank entry returns)");
	mvaddstr (7, 1, "=> ");

	while (error)
		{
			if (error == 1)
				mvaddstr (9, 1, "There is no such user. Try again.  ");
			else if (error == 3)
				mvaddstr (9, 1, "That user is not playing right now.");

			mvaddstr (7, 1, "=>                     ");
			move (7, 4);
			getnstr (buf, 20);

			if (buf && *buf == '\0')
				return;

			if (userexist (buf) != -1)
				{
					DIR *ip = opendir (LOC_INPROGRESSDIR);
					struct dirent *dent;

					error = 3;						/* unless set by loop below, assume not playing */

					while ((dent = readdir (ip)) != NULL)
						{
							char *trname = strdup (dent->d_name);
							int len =
								(sizeof (LOC_INPROGRESSDIR) / sizeof (LOC_INPROGRESSDIR[0])) +
								strlen (dent->d_name);
							char *fullpath = malloc (len + 1);
							int fd;

							snprintf (fullpath, len, "%s%s", LOC_INPROGRESSDIR,
												dent->d_name);

							fd = open (fullpath, O_RDONLY);
							user = strdup (strtok (trname, ":"));

							/* if it's locked, it's in session */
							if (!strcmp (buf, user))
								{
									/* could lock? unlock and forget about it */
									if (flock (fd, LOCK_EX | LOCK_NB) == 0)
										{
											flock (fd, LOCK_UN | LOCK_NB);
											continue;
										}
									else					/* no lock, good deal */
										{
											error = 0;
											break;
										}
								}
							close (fd);

							free (user);
							free (trname);
						}

					closedir (ip);
				}
			else
				error = 1;
		}

	if (error == 0)
		domailuser (user);
}

void
drawmenu ()
{
	static int flood = 0;

	clear ();

	mvaddstr (1, 1, VER1);
	mvaddstr (2, 1, VER2);
	mvaddstr (3, 1, VER3);
	mvaddstr (4, 1, VER4);
	mvaddstr (5, 1, VER5);
	mvaddstr (6, 1, VER6);
	mvaddstr (7, 1, VER7);

	if (loggedin)
		{
			mvprintw (VERLINES + 2, 1, "Logged in as: %s", me->username);
			mvaddstr (VERLINES + 4, 1, "c) Change password");
			mvaddstr (VERLINES + 5, 1, "o) Edit option file (requires vi use)");
			mvaddstr (VERLINES + 6, 1, "w) Watch games in progress");
			mvaddstr (VERLINES + 7, 1, "m) Contact a current player");
			mvaddstr (VERLINES + 8, 1, "p) Play nethack!");
			mvaddstr (VERLINES + 9, 1, "q) Quit");
			mvaddstr (VERLINES + 11, 1, "=> ");
		}
	else
		{
			mvaddstr (VERLINES + 2, 1, "Not logged in.");
			mvaddstr (VERLINES + 4, 1, "l) Login");
			mvaddstr (VERLINES + 5, 1, "r) Register new user");
			mvaddstr (VERLINES + 6, 1, "w) Watch games in progress");
			mvaddstr (VERLINES + 7, 1, "q) Quit");
			mvaddstr (VERLINES + 9, 1, "=> ");
		}

	refresh ();

	/* for retarded clients */
	flood++;
	if (flood >= 20)
		exit (119);
}

/* ************************************************************* */

void
freefile ()
{
	int i;

	/* free existing mem, clear existing entries */
	for (i = 0; i < f_num; i++)
		{
			free (users[i]->password);
			free (users[i]->username);
			free (users[i]->email);
			free (users[i]->env);
			free (users[i]);
		}

	if (users)
		free (users);

	users = NULL;
	f_num = 0;
}

/* ************************************************************* */

void
initncurses ()
{
	initscr ();
	cbreak ();
	echo ();
	nonl ();
	intrflush (stdscr, FALSE);
	keypad (stdscr, TRUE);
}

/* ************************************************************* */

struct dg_user *
deep_copy (struct dg_user *src)
{
	struct dg_user *dest = malloc (sizeof (struct dg_user));

	dest->username = strdup (src->username);
	dest->email = strdup (src->email);
	dest->env = strdup (src->env);
	dest->password = strdup (src->password);
	dest->flags = src->flags;

	return dest;
}

void
login ()
{
	char user_buf[22], pw_buf[22];
	int error = 2, me_index = -1;

	loggedin = 0;

	while (error)
		{
			clear ();

			mvaddstr (1, 1, VER1);

			mvaddstr (5, 1,
								"Please enter your username. (blank entry returns to main menu)");
			mvaddstr (7, 1, "=> ");

			if (error == 1)
				{
					mvaddstr (9, 1, "There was a problem with your last entry.");
					move (7, 4);
				}

			refresh ();

			getnstr (user_buf, 20);

			if (user_buf && *user_buf == '\0')
				return;

			error = 1;

			if ((me_index = userexist (user_buf)) != -1)
				{
					me = deep_copy (users[me_index]);
					error = 0;
				}
		}

	clear ();

	mvaddstr (1, 1, VER1);

	mvaddstr (5, 1, "Please enter your password.");
	mvaddstr (7, 1, "=> ");

	refresh ();

	noecho ();
	getnstr (pw_buf, 20);
	echo ();

	if (passwordgood (user_buf, pw_buf))
		{
			loggedin = 1;
			snprintf (rcfilename, 80, "%s%s.nethackrc", LOC_DGLDIR, me->username);
		}
}

/* ************************************************************* */

void
newuser ()
{
	char buf[1024];
	int error = 2;
	unsigned int i;

	loggedin = 0;

	if (me)
		free (me);

	me = malloc (sizeof (struct dg_user));

	while (error)
		{
			clear ();

			mvaddstr (1, 1, VER1);

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

			getnstr (buf, 20);
			if (userexist (buf) == -1)
				error = 0;
			else
				error = 1;

			for (i = 0; i < strlen (buf); i++)
				{
					if (!
							(((buf[i] >= 'a') && (buf[i] <= 'z'))
							 || ((buf[i] >= 'A') && (buf[i] <= 'Z')) || ((buf[i] >= '0')
																													 && (buf[i] <=
																															 '9'))))
						error = 1;
				}

			if (strlen (buf) < 2)
				error = 1;

			if (strlen (buf) == 0)
				return;
		}

	me->username = strdup (buf);

	/* password step */

	clear ();

	mvaddstr (1, 1, VER1);

	mvaddstr (5, 1, "Please enter a password.");
	mvaddstr (6, 1,
						"This is only trivially encoded at the server. Please use something");
	mvaddstr (7, 1, "new and expect it to be relatively insecure.");
	mvaddstr (8, 1, "20 character max. No ':' characters.");
	mvaddstr (10, 1, "=> ");

	refresh ();
	getnstr (buf, 20);

	/* we warned em */
	if (strstr (buf, ":") != NULL)
		exit (112);

	me->password = strdup (crypt (buf, buf));

	/* email step */

	clear ();

	mvaddstr (1, 1, VER1);

	mvaddstr (5, 1, "Please enter your email address.");
	mvaddstr (6, 1,
						"This is sent _nowhere_ but will be used if you ask the sysadmin for lost");
	mvaddstr (7, 1,
						"password help. Please use a correct one. It only benefits you.");
	mvaddstr (8, 1, "80 character max. No ':' characters.");
	mvaddstr (10, 1, "=> ");

	refresh ();
	getnstr (buf, 80);

	if (strstr (buf, ":") != NULL)
		exit (113);

	me->email = strdup (buf);
	me->env = calloc (1, 1);

	loggedin = 1;

	snprintf (rcfilename, 80, "%s%s.nethackrc", LOC_DGLDIR, me->username);
	write_canned_rcfile (rcfilename);

	writefile (1);
}

/* ************************************************************* */

int
passwordgood (char *cname, char *cpw)
{
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

	memset (buf, 1024, 0);

	/* read new stuff */

	if (!nolock)
		{
			fpl = fopen ("/dgl-lock", "r");
			if (!fpl)
				exit (106);
			if (flock (fileno (fpl), LOCK_SH))
				exit (114);
		}

	fp = fopen ("/dgl-login", "r");
	if (!fp)
		exit (106);

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
					if (!(((*b >= 'a') && (*b <= 'z')) || ((*b >= 'A') && (*b <= 'Z'))
								|| ((*b >= '0') && (*b <= '9'))))
						return 1;
					users[f_num]->username[(b - n)] = *b;
					b++;
					if ((b - n) >= 21)
						exit (100);
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
						exit (101);
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
						exit (102);
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
						exit (102);
				}

			f_num++;
			/* prevent a buffer overrun here */
			if (f_num >= MAXUSERS)
				exit (109);
		}

	if (!nolock)
		{
			flock (fileno (fpl), LOCK_UN);
			fclose (fpl);
		}
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
	char buf[1024];
	size_t bytes;

	if (!(newfile = fopen (target, "w")))
		{
		bail:
			mvaddstr (13, 1,
								"You don't know how to write that! You write \"%s\" was here and the scroll disappears.");
			mvaddstr (14, 1,
								"(Sorry, but I couldn't open one of the nethackrc files. This is a bug.)");
			return;
		}

	if (!(canned = fopen (LOC_CANNED, "r")))
		goto bail;

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

	rcfile = fopen (rcfilename, "r");
	printf (" read");
	if (!rcfile)									/* should not really happen except for old users */
		write_canned_rcfile (rcfilename);

	/* use virus to edit */

	myargv[0] = "";
	myargv[1] = rcfilename;
	myargv[2] = 0;

	endwin ();
	vi_main (2, myargv);
	refresh ();
}

/* ************************************************************* */

void
writefile (int requirenew)
{
	FILE *fp, *fpl;
	int i = 0;
	int my_done = 0;

	fpl = fopen ("/dgl-lock", "r");
	if (!fpl)
		exit (115);
	if (flock (fileno (fpl), LOCK_EX))
		exit (107);

	freefile ();
	readfile (1);

	fp = fopen ("/dgl-login", "w");
	if (!fp)
		exit (104);

	for (i = 0; i < f_num; i++)
		{
			if (loggedin && !strncasecmp (me->username, users[i]->username, 20))
				{
					if (requirenew)
						{
							/* this is if someone managed to register at the same time
							 * as someone else. just die. */
							exit (111);
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
		{														/* new entry */
			fprintf (fp, "%s:%s:%s:%s\n", me->username, me->email, me->password,
							 me->env);
		}

	flock (fileno (fpl), LOCK_UN);
	fclose (fp);
	fclose (fpl);
}

/* ************************************************************* */
/* ************************************************************* */
/* ************************************************************* */

int
main (void)
{
	/* for chroot and program execution */
	uid_t newuid = SHED_UID;
	gid_t newgid = SHED_GID;
	char atrcfilename[81], *spool;
	unsigned int len;

	int userchoice = 0;

	/* signal handlers */
	signal (SIGHUP, catch_sighup);

	/* get master tty just before chroot (lives in /dev) */
	ttyrec_getmaster ();
	grantpt (master);
	unlockpt (master);
	if ((slave = open ((const char *) ptsname (master), O_RDWR)) < 0)
		{
			exit (65);
		}


	/* chroot */
	if (chroot (LOC_CHROOT))
		{
			perror ("cannot change root directory");
			exit (1);
		}

	if (chdir ("/"))
		{
			perror ("cannot chdir to root directory");
			exit (1);
		}

	/* shed privs. this is done immediately after chroot. */
	setgid (newgid);
	setuid (newuid);

	/* simple login routine, uses ncurses */
	if (readfile (0))
		exit (110);

	initncurses ();
	while ((userchoice != 'p') | (!loggedin))
		{
			drawmenu ();
			userchoice = getch ();
			switch (tolower (userchoice))
				{
				case 'c':
					changepw ();
					break;
				case 'w':
					inprogressmenu ();
					break;
				case 'o':
					if (loggedin)
						editoptions ();
					break;
				case 'm':
					if (loggedin)
						mailuser ();
					break;
				case 'q':
					endwin ();
					exit (1);
					/* break; */
				case 'r':
					if (!loggedin)				/*not visible to loggedin */
						newuser ();
					break;
				case 'l':
					if (!loggedin)				/* not visible to loggedin */
						login ();
					break;
				}
		}

	assert (loggedin);

	endwin ();

	/* environment */
	snprintf (atrcfilename, 81, "@%s", rcfilename);

	len =
		(sizeof (LOC_SPOOLDIR) / sizeof (LOC_SPOOLDIR[0])) +
		strlen (me->username) + 1;
	spool = malloc (len + 1);
	snprintf (spool, len, "%s/%s", LOC_SPOOLDIR, me->username);

	setenv ("NETHACKOPTIONS", atrcfilename, 1);
	setenv ("MAIL", spool, 1);
	setenv ("SIMPLEMAIL", "1", 1);

	/* don't let the mail file grow */
	if (access (spool, F_OK) == 0)
		unlink (spool);

	free (spool);

	/* lock */
	gen_ttyrec_filename ();
	gen_inprogress_lock ();

	/* launch program */
	ttyrec_main (me->username);

	/* NOW we can safely kill this */
	freefile ();

	if (me)
		free (me);

	exit (1);
	return 1;
}
