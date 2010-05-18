/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@misiek.eu.org>
 * - added Native Language Support
 */

/* 2000-12-27 Satoru Takabayashi <satoru@namazu.org>
 * - modify `script' to create `ttyrec'.
 */

/*
 * script
 */

#include "dgamelaunch.h"
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <termios.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#ifndef NOSTREAMS
# include <stropts.h>
#endif
#include <stdlib.h>

#include "ttyrec.h"
#include "io.h"

#ifndef XCASE
# define XCASE 0
#endif

int slave;
pid_t dgl_parent;
pid_t child, subchild;
pid_t input_child;
char* ipfile = NULL;

volatile int wait_for_menu = 0;

FILE *fscript;
int master;

struct termios tt;
struct winsize win;

void
ttyrec_id(int game, char *username, char *ttyrec_filename)
{
    int i;
    time_t tstamp;
    Header h;
    char *buf = (char *)malloc(1024);
    char tmpbuf[256];
    if (!buf) return;

    tstamp = time(NULL);

#define dCLRSCR "\033[2J"
#define dCRLF   "\r\n"
    snprintf(buf, 1024,
            dCLRSCR "\033[1;1H" dCRLF
            "Player: %s" dCRLF
            "Game: %s" dCRLF
            "Server: %s" dCRLF
            "Filename: %s" dCRLF
            "Time: (%lu) %s" dCRLF
            dCLRSCR,
            username,
            myconfig[game]->game_name,
            globalconfig.server_id,
            ttyrec_filename,
            tstamp, ctime(&tstamp)
            );
#undef dCLRSCR
#undef dCRLF
    h.len = strlen(buf);
    gettimeofday (&h.tv, NULL);

    (void) write_header(fscript, &h);
    (void) fwrite(buf, 1, h.len, fscript);

    free(buf);
}

int
ttyrec_main (int game, char *username, char *ttyrec_path, char* ttyrec_filename)
{
  char dirname[100];

  /* Note our PID to let children kill the main dgl process for idling */
  dgl_parent = getpid();
  child = subchild = input_child = 0;

  if (!ttyrec_path) {
      child = fork();
      if (child < 0) {
	  perror ("fork");
	  fail ();
      }
      if (child == 0) {
	  execvp (myconfig[game]->game_path, myconfig[game]->bin_args);
      } else {
	  int status;
	  (void) wait(&status);
      }
      return 0;
  }

  if (ttyrec_path[strlen(ttyrec_path)-1] == '/')
      snprintf (dirname, 100, "%s%s", ttyrec_path, ttyrec_filename);
  else
      snprintf (dirname, 100, "%s/%s", ttyrec_path, ttyrec_filename);

  atexit(&remove_ipfile);
  if ((fscript = fopen (dirname, "w")) == NULL)
    {
      perror (dirname);
      fail ();
    }
  setbuf (fscript, NULL);

  fixtty ();

  (void) signal (SIGCHLD, finish);
  child = fork ();
  if (child < 0)
    {
      perror ("fork");
      fail ();
    }
  if (child == 0)
    {
      subchild = child = fork ();
      if (child < 0)
        {
          perror ("fork");
          fail ();
        }
      if (child)
        {
          close (slave);
          ipfile = gen_inprogress_lock (game, child, ttyrec_filename);
	  ttyrec_id(game, username, ttyrec_filename);
          dooutput (myconfig[game]->max_idle_time);
        }
      else
	  doshell (game, username);
    }

  (void) fclose (fscript);

  wait_for_menu = 1;
  input_child = fork();
  if (input_child < 0)
  {
      perror ("fork2");
      fail ();
  }
  if (!input_child)
      doinput ();
  else
  {
      while (wait_for_menu)
	  sleep(1);
  }

  remove_ipfile();
  child = 0;

  return 0;
}

void
doinput ()
{
  register int cc;
  char ibuf[BUFSIZ];

  while ((cc = read (0, ibuf, BUFSIZ)) > 0)
    (void) write (master, ibuf, cc);
  done ();
}

void
finish (int sig)
{
  int status;
  register int pid;
  register int die = 0;

  (void)sig; /* unused */

  while ((pid = wait3 (&status, WNOHANG, 0)) > 0)
  {
    if (pid == child)
      die = 1;
  }

  if (die)
  {
      if (input_child)
      {
	  // Need to kill the child that's writing input to pty.
	  kill(input_child, SIGTERM);
	  while ((pid = wait3(&status, WNOHANG, 0)) > 0);
      }
      else
	  done ();
  }
  wait_for_menu = 0;
}

void
game_idle_kill(int signal)
{
    kill(child, SIGHUP);
    kill(dgl_parent, SIGHUP);
}

void
dooutput (int max_idle_time)
{
  int cc;
  time_t tvec, time ();
  char obuf[BUFSIZ], *ctime ();

  setbuf (stdout, NULL);
  (void) close (0);
  tvec = time ((time_t *) NULL);
  /* Set up SIGALRM handler to kill idle games */
  signal(SIGALRM, game_idle_kill);
  for (;;)
    {
      Header h;

      cc = read (master, obuf, BUFSIZ);
      if (cc <= 0)
        break;

      if (max_idle_time)
          alarm(max_idle_time);

      h.len = cc;
      gettimeofday (&h.tv, NULL);
      (void) write (1, obuf, cc);
      (void) write_header (fscript, &h);
      (void) fwrite (obuf, 1, cc, fscript);
    }
  done ();
}

void
doshell (int game, char *username)
{
  getslave ();
  (void) close (master);
  (void) fclose (fscript);
  (void) dup2 (slave, 0);
  (void) dup2 (slave, 1);
  (void) dup2 (slave, 2);
  (void) close (slave);

  /*
  if (myconfig[game]->mkdir)
      (void) mkdir(myconfig[game]->mkdir, 0755);

  if (myconfig[game]->chdir)
      (void) chdir(myconfig[game]->chdir);
  */

  execvp (myconfig[game]->game_path, myconfig[game]->bin_args);

  fail ();
}

void
fixtty ()
{
  struct termios rtt;

  rtt = tt;
  rtt.c_iflag = 0;
  rtt.c_lflag &= ~(ISIG | ICANON | XCASE | ECHO | ECHOE | ECHOK | ECHONL);
  rtt.c_oflag = OPOST;
  rtt.c_cc[VINTR] = _POSIX_VDISABLE;
  rtt.c_cc[VQUIT] = _POSIX_VDISABLE;
  rtt.c_cc[VERASE] = _POSIX_VDISABLE;
  rtt.c_cc[VKILL] = _POSIX_VDISABLE;
  rtt.c_cc[VEOF] = 1;
  rtt.c_cc[VEOL] = 0;
  rtt.c_cc[VMIN] = 1;
  rtt.c_cc[VTIME] = 0;
  (void) tcsetattr (0, TCSAFLUSH, &rtt);
}

void
fail ()
{

  (void) kill (0, SIGTERM);
  done ();
}

void
done ()
{
  time_t tvec, time ();
  char *ctime ();

  if (subchild)
    {
      tvec = time ((time_t *) NULL);
      (void) fclose (fscript);
      (void) close (master);
    }
  else
    {
      (void) tcsetattr (0, TCSAFLUSH, &tt);
    }

  remove_ipfile();
  graceful_exit (0);
}

void
getslave ()
{
  (void) setsid ();
  /* grantpt( master);
     unlockpt(master);
     if ((slave = open((const char *)ptsname(master), O_RDWR)) < 0) {
     perror((const char *)ptsname(master));
     fail();
     perror("open(fd, O_RDWR)");
     fail();
     } */
#ifndef NOSTREAMS
  if (isastream (slave))
    {
      if (ioctl (slave, I_PUSH, "ptem") < 0)
        {
          perror ("ioctl(fd, I_PUSH, ptem)");
          fail ();
        }
      if (ioctl (slave, I_PUSH, "ldterm") < 0)
        {
          perror ("ioctl(fd, I_PUSH, ldterm)");
          fail ();
        }
#ifndef _HPUX_SOURCE
      if (ioctl (slave, I_PUSH, "ttcompat") < 0)
        {
          perror ("ioctl(fd, I_PUSH, ttcompat)");
          fail ();
        }
#endif
    }
#endif /* !NOSTREAMS */
  (void) ioctl (0, TIOCGWINSZ, (char *) &win);
}

void
remove_ipfile (void)
{
    if (ipfile != NULL) {
	unlink (ipfile);
	free(ipfile);
	ipfile = NULL;
    }
    signal(SIGALRM, SIG_IGN);
}
