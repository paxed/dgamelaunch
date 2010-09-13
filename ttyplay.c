/*
 * Copyright (c) 2000 Satoru Takabayashi <satoru@namazu.org>
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

#include "config.h"

#define _GNU_SOURCE /* need sighandler_t */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef HAVE_KQUEUE
#include <sys/event.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <curses.h>
#include <signal.h>
#include <errno.h>

#include "dgamelaunch.h"
#include "ttyplay.h"
#include "ttyrec.h"
#include "io.h"
#include "stripgfx.h"

#ifdef __MACH__
typedef void (*sighandler_t)(int);
#endif

int stripped = NO_GRAPHICS;
static int got_sigwinch = 0;

static int term_resizex = -1;
static int term_resizey = -1;

void
ttyplay_sigwinch_func(int sig)
{
    signal(SIGWINCH, ttyplay_sigwinch_func);
    got_sigwinch = 1;
}

struct timeval
timeval_diff (struct timeval tv1, struct timeval tv2)
{
  struct timeval diff;

  diff.tv_sec = tv2.tv_sec - tv1.tv_sec;
  diff.tv_usec = tv2.tv_usec - tv1.tv_usec;
  if (diff.tv_usec < 0)
    {
      diff.tv_sec--;
      diff.tv_usec += 1000000;
    }

  return diff;
}

struct timeval
timeval_div (struct timeval tv1, double n)
{
  double x = ((double) tv1.tv_sec + (double) tv1.tv_usec / 1000000.0) / n;
  struct timeval div;

  div.tv_sec = (int) x;
  div.tv_usec = (x - (int) x) * 1000000;

  return div;
}

double
ttywait (struct timeval prev, struct timeval cur, double speed)
{
  struct timeval diff = timeval_diff (prev, cur);

  assert (speed != 0);
  diff = timeval_div (diff, speed);

  select (1, NULL, NULL, NULL, &diff); /* skip if a user hits any key */
  
  return speed;
}

double
ttynowait (struct timeval prev, struct timeval cur, double speed)
{
  return 0;                     /* Speed isn't important. */
}

int
kbhit(void)
{
    int i = 0;
    nodelay(stdscr, TRUE);
    timeout(0);
    i = wgetch(stdscr);
    nodelay(stdscr, FALSE);

    if (i == -1)
        i = 0;
    else
        ungetch(i);
    return (i);
}

int
ttyplay_keyboard_action(int c)
{
    struct termios t;
    switch (c)
    {
    case ERR:
    case 'q':
        return READ_QUIT;
    case 'r':
        if (term_resizex > 0 && term_resizey > 0) {
            printf ("\033[8;%d;%dt", term_resizey, term_resizex);
            return READ_RESTART;
        }
        break;
    case 's':
        switch (stripped)
        {
	case NO_GRAPHICS: populate_gfx_array ((stripped = DEC_GRAPHICS)); break;
	case DEC_GRAPHICS: populate_gfx_array ((stripped = IBM_GRAPHICS)); break;
	case IBM_GRAPHICS: populate_gfx_array ((stripped = NO_GRAPHICS)); break;
        }
        return READ_RESTART;

    case 'm':
        tcgetattr (0, &t);
        if (!loggedin)
        {
            initcurses();
            loginprompt(1);
        }
        if (loggedin)
        {
            initcurses ();
            domailuser (chosen_name);
        }
        endwin ();
        tcsetattr (0, TCSANOW, &t);
        return READ_RESTART;
    case '?':
        tcgetattr (0, &t);
        initcurses();
        (void) runmenuloop(dgl_find_menu("watchmenu_help"));
        endwin ();
        tcsetattr (0, TCSANOW, &t);
        return READ_RESTART;
    }
    return (READ_DATA);
}

int
ttyread (FILE * fp, Header * h, char **buf, int pread)
{
  long offset;
  int kb = kbhit();

  if (kb == ERR) return READ_QUIT;
  else if (kb) {
      const int c = dgl_getch();
      const int action = ttyplay_keyboard_action(c);
      if (action != READ_DATA)
	  return (action);
  }

  /* do this BEFORE header read, hlen bug */
  offset = ftell (fp);

  if (read_header (fp, h) == 0)
    {
      return READ_EOF;
    }

  /* length should never be longer than one BUFSIZ */
  if (h->len > BUFSIZ)
    {
      fprintf (stderr, "h->len too big (%ld) limit %ld\n",
		      (long)h->len, (long)BUFSIZ);
      return READ_QUIT;
    }

  *buf = malloc (h->len + 1);
  if (*buf == NULL)
    {
      perror ("malloc");
      return READ_QUIT;
    }

  if (fread (*buf, 1, h->len, fp) != h->len)
    {
      fseek (fp, offset, SEEK_SET);
      return READ_EOF;
    }
  (*buf)[h->len] = 0;
  return READ_DATA;
}

int
ttypread (FILE * fp, Header * h, char **buf, int pread)
{
  int n;
#ifdef HAVE_KQUEUE
  struct kevent evt[2];
  static int kq = -1;
#endif
  struct timeval w = { 0, 100000 };
  struct timeval origw = { 0, 100000 };
  int counter = 0;
  fd_set readfs;
  int doread = 0;
  int action = READ_DATA;

#ifdef HAVE_KQUEUE
  if (kq == -1)
    kq = kqueue ();
  if (kq == -1)
    {
      printf ("kqueue() failed.\n");
      return READ_QUIT;
    }
#endif

  /*
   * Read persistently just like tail -f.
   */
  while ((action = ttyread (fp, h, buf, 1)) == READ_EOF)
    {
      idle_alarm_reset();
      fflush(stdout);
      clearerr (fp);
#ifdef HAVE_KQUEUE
      n = -1;
      if (kq != -2)
      {
	EV_SET (&evt[0], STDIN_FILENO, EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);
	EV_SET (&evt[1], fileno (fp), EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, NULL);
	n = kevent (kq, evt, 2, evt, 1, NULL);
	doread = (n >= 1 && evt[0].ident == STDIN_FILENO &&
	  evt[0].filter == EVFILT_READ) ||
	  (n >= 2 && evt[1].ident == STDIN_FILENO &&
	   evt[1].filter == EVFILT_READ);
	if (n == -1)
	  {
	    /*
	     * Perhaps kevent(2) doesn't work on this fstype,
	     * use select(2) instead. Never use kevent again, assuming all
	     * active ttyrecs are on the same fstype.
	     */
	    close(kq);
	    kq = -2;
	  }
      }
      if (n == -1)
#endif
      {
	if (counter++ > (20 * 60 * 10))
	  {
	    /*
	     * The reason for this timeout is that the select() method uses
	     * some CPU in waiting. The kqueue() method does not do that, so it
	     * does not need the timeout.
	     */
	    endwin ();
	    printf ("Exiting due to 20 minutes of inactivity.\n");
	    return READ_QUIT;
	  }
	FD_ZERO (&readfs);
	FD_SET (STDIN_FILENO, &readfs);
	n = select (1, &readfs, NULL, NULL, &w);
	w = origw;
	doread = n >= 1 && FD_ISSET (0, &readfs);
      }
      if (n == -1)
	{
	    if ((errno == EINTR) && got_sigwinch) {
		got_sigwinch = 0;
		return READ_RESTART;
	    } else {
		printf("select()/kevent() failed.\n");
		return READ_QUIT;
	    }
	}
      if (doread)
        {                       /* user hits a character? */
	  const int c = dgl_getch();
	  action = ttyplay_keyboard_action(c);
	  if (action != READ_DATA)
	      return action;

        }
    }
  return (action);
}

void
ttywrite (char *buf, int len)
{
  int i;

  for (i = 0; i < len; i++)
  {
    if (stripped != NO_GRAPHICS)
      buf[i] = strip_gfx (buf[i]);
  }

  fwrite (buf, 1, len, stdout);
}

void
ttynowrite (char *buf, int len)
{
  /* do nothing */
}

int
ttyplay (FILE * fp, double speed, ReadFunc read_func,
         WriteFunc write_func, WaitFunc wait_func, off_t offset)
{
  int first_time = 1;
  int r = READ_EOF;
  struct timeval prev;

  /* for dtype's attempt to get the last clrscr and playback from there */
  if (offset != -1)
    {
      fseek (fp, offset, SEEK_SET);
    }

  while (1)
    {
      char *buf;
      Header h;

      r = read_func (fp, &h, &buf, 0);
      if (r != READ_DATA)
        {
          break;
        }

      if (!first_time)
        {
          speed = wait_func (prev, h.tv, speed);
        }
      first_time = 0;

      write_func (buf, h.len);
      prev = h.tv;
      free (buf);
    }
  return r;
}

static off_t
find_last_string_in_file(FILE * fp, const char *seq)
{
    char buf[512];
    struct stat mystat;
    off_t offset = 0L;
    const long readsz = sizeof(buf);
    int bytes_read = 0;
    const int seqlen = strlen(seq);
    const char *reset_pos = seq + seqlen - 1;
    const char *match_pos = reset_pos;

    fstat(fileno (fp), &mystat);
    offset = mystat.st_size - readsz;
    if (offset < 0)
        offset = 0;
    while (1)
    {
        const char *search_pos = 0;

        fseeko(fp, offset, SEEK_SET);
        bytes_read = fread(buf, 1, readsz, fp);

        if (bytes_read <= 0)
            break;

        search_pos = buf + bytes_read - 1;
        while (search_pos >= buf)
        {
            int matched = *search_pos == *match_pos;
            if (!matched && match_pos != reset_pos)
            {
                match_pos = reset_pos;
                matched = *search_pos == *match_pos;
            }
            if (matched)
            {
                if (match_pos == seq)
                    return offset + (search_pos - buf);
                --match_pos;
            }
            --search_pos;
        }

        // If we've reached the start of the file, exit.
        if (!offset)
            break;

        offset -= readsz;
        if (offset < 0)
            offset = 0;
    }

    return 0;
}

static off_t
find_seek_offset_clrscr (FILE * fp)
{
  off_t raw_seek_offset = 0;
  off_t raw_seek_offset2 = 0;
  off_t seek_offset_clrscr;

  raw_seek_offset = find_last_string_in_file(fp, "\033[2J");
  raw_seek_offset2 = find_last_string_in_file(fp, "\033[H\033[J");
  if (raw_seek_offset2>raw_seek_offset) raw_seek_offset=raw_seek_offset2;

  seek_offset_clrscr = 0;
  /* now find last filepos that is less than seek offset */
  fseek (fp, 0, SEEK_SET);
  while (1)
    {
      char *buf;
      Header h;
      long offset;

      if (ttyread (fp, &h, &buf, 0) != READ_DATA)
        {
          break;
        }

      free (buf);

      offset = ftell(fp);
      if (offset < raw_seek_offset)
          seek_offset_clrscr = ftell (fp);
      else
          break;
    }

  return seek_offset_clrscr;
}

#if 0 /* not used anymore */
void
ttyskipall (FILE * fp)
{
  /*
   * Skip all records.
   */
  ttyplay (fp, 0, ttyread, ttynowrite, ttynowait, 0);
}
#endif

void
ttyplayback (FILE * fp, double speed, ReadFunc read_func, WaitFunc wait_func)
{
  ttyplay (fp, speed, ttyread, ttywrite, wait_func, 0);
}

void
ttypeek (FILE * fp, double speed)
{
  int r;
  do
  {
    setvbuf (fp, NULL, _IOFBF, 0);
    r = ttyplay(fp, 0, ttyread, ttywrite, ttynowait, find_seek_offset_clrscr(fp));
    if (r == READ_EOF) {
	clearerr (fp);
        setvbuf (fp, NULL, _IONBF, 0);
        fflush (stdout);
        r = ttyplay (fp, speed, ttypread, ttywrite, ttynowait, -1);
    }
  } while (r == READ_RESTART);
}


int
ttyplay_main (char *ttyfile, int mode, int resizex, int resizey)
{
  double speed = 1.0;
  ReadFunc read_func = ttyread;
  WaitFunc wait_func = ttywait;
  FILE *input = stdin;
  struct termios old, new;
  sighandler_t old_sigwinch;

  populate_gfx_array (stripped);

  input = efopen (ttyfile, "r");

  tcgetattr (0, &old);          /* Get current terminal state */
  new = old;                    /* Make a copy */
  new.c_lflag &= ~(ICANON | ECHO | ECHONL); /* unbuffered, no echo */
  new.c_cc[VMIN] = 1;
  new.c_cc[VTIME] = 0;
  tcsetattr (0, TCSANOW, &new); /* Make it current */
  raw();

  if (resizex > 0 && resizey > 0) {
      term_resizex = resizex;
      term_resizey = resizey;
  }

  got_sigwinch = 0;
  old_sigwinch = signal(SIGWINCH, ttyplay_sigwinch_func);

  if (mode == 1)
    ttypeek (input, speed);
  else
    ttyplayback (input, speed, read_func, wait_func);

  tcsetattr (0, TCSANOW, &old); /* Return terminal state */
  fclose (input);

  if (old_sigwinch != SIG_ERR)
      signal(SIGWINCH, old_sigwinch);

  term_resizex = term_resizey = -1;

  printf("\033[2J"); /* clear screen afterwards */

  return 0;
}
