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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <curses.h>
#include "ttyrec.h"
#include "io.h"
#include "stripgfx.h"

extern void domailuser (char *);
extern void initncurses (void);
extern char *chosen_name;
extern int loggedin;

int ttyplay_main (char *ttyfile, int mode, int rstripgfx);

off_t seek_offset_clrscr;
int bstripgfx;
char *ttyfile_local;

typedef double (*WaitFunc) (struct timeval prev,
                            struct timeval cur, double speed);
typedef int (*ReadFunc) (FILE * fp, Header * h, char **buf, int pread);
typedef void (*WriteFunc) (char *buf, int len);
typedef void (*ProcessFunc) (FILE * fp, double speed,
                             ReadFunc read_func, WaitFunc wait_func);

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
  fd_set readfs;

  assert (speed != 0);
  diff = timeval_div (diff, speed);

  FD_SET (STDIN_FILENO, &readfs);
  select (1, &readfs, NULL, NULL, &diff); /* skip if a user hits any key */
  if (FD_ISSET (0, &readfs))
    {                           /* a user hits a character? */
      char c;
      read (STDIN_FILENO, &c, 1); /* drain the character */
      switch (c)
        {
        case '+':
        case 'f':
          speed *= 2;
          break;
        case '-':
        case 's':
          speed /= 2;
          break;
        case '1':
          speed = 1.0;
          break;
        }
    }
  return speed;
}

double
ttynowait (struct timeval prev, struct timeval cur, double speed)
{
  return 0;                     /* Speed isn't important. */
}

int
ttyread (FILE * fp, Header * h, char **buf, int pread)
{
  long offset;

  /* do this BEFORE header read, hlen bug */
  offset = ftell (fp);

  if (read_header (fp, h) == 0)
    {
      return 0;
    }

  /* length should never be longer than one BUFSIZ */
  if (h->len > BUFSIZ)
    {
      perror ("hlen");
      exit (-21);
    }

  *buf = malloc (h->len);
  if (*buf == NULL)
    {
      perror ("malloc");
      exit (-22);
    }

  if (fread (*buf, 1, h->len, fp) != h->len)
    {
      fseek (fp, offset, SEEK_SET);
      return 0;
    }
  return 1;
}

int
ttypread (FILE * fp, Header * h, char **buf, int pread)
{
  int counter = 0;
  fd_set readfs;
  struct timeval zerotime;

  zerotime.tv_sec = 0;
  zerotime.tv_usec = 0;

  /*
   * Read persistently just like tail -f.
   */
  while (ttyread (fp, h, buf, 1) == 0)
    {
      struct timeval w = { 0, 100000 };
      select (0, NULL, NULL, NULL, &w);
      clearerr (fp);
      if (counter++ > (20 * 60 * 10))
        {
          endwin ();
          printf ("Exiting due to 20 minutes of inactivity.\n");
          exit (-23);
          return 0;
        }


      /* look for keypresses here. as good a place as any */
      FD_SET (STDIN_FILENO, &readfs);
      select (1, &readfs, NULL, NULL, &zerotime);
      if (FD_ISSET (0, &readfs))
        {                       /* a user hits a character? */
          char c;
          read (STDIN_FILENO, &c, 1); /* drain the character */
          switch (c)
            {
            case 'q':
              return 0;
              break;
            case 'm':
              if (loggedin)
                {
                  initncurses ();
                  domailuser (chosen_name);
                  endwin ();
                  ttyplay_main (ttyfile_local, 1, 0);
                  return 0;
                }
              break;
            }
        }
    }
  return 1;
}

void
ttywrite (char *buf, int len)
{
  int i;

  if (bstripgfx)
    {
      for (i = 0; i < len; i++)
        {
          buf[i] = strip_gfx (buf[i]);
        }
    }

  fwrite (buf, 1, len, stdout);
}

void
ttynowrite (char *buf, int len)
{
  /* do nothing */
}

void
ttyplay (FILE * fp, double speed, ReadFunc read_func,
         WriteFunc write_func, WaitFunc wait_func, off_t offset)
{
  int first_time = 1;
  struct timeval prev;

  setbuf (stdout, NULL);
  setbuf (fp, NULL);

  /* for dtype's attempt to get the last clrscr and playback from there */
  if (offset)
    {
      lseek (fileno (fp), offset, SEEK_SET);
    }

  while (1)
    {
      char *buf;
      Header h;

      if (read_func (fp, &h, &buf, 0) == 0)
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
}

void
set_seek_offset_clrscr (FILE * fp)
{
  off_t raw_seek_offset = 0;
  char *buf;
  struct stat mystat;
  int state = 0;
  int i;
  int bytesread;

  lseek (fileno (fp), 0, SEEK_SET);
  fstat (fileno (fp), &mystat);
  buf = malloc (mystat.st_size);
  bytesread = read (fileno (fp), buf, mystat.st_size);

  /* one byte at at time sucks, but is a simple hack for the temp
   * being to avoid looking for wraparounds */
  for (i = 0; i < bytesread; i++)
    {
      if (buf[i] == 0x1b)
        {
          state = 1;
        }
      else if ((buf[i] == 0x5b) && (state == 1))
        {
          state = 2;
        }
      else if ((buf[i] == 0x32) && (state == 2))
        {
          state = 3;
        }
      else if ((buf[i] == 0x4a) && ((state == 2) || (state == 3)))
        {
          state = 4;
        }
      else
        {
          state = 0;
        }

      if (state == 4)
        {
          raw_seek_offset = i - 2;
        }
    }

  free (buf);

  /* now find last filepos that is less than seek offset */
  lseek (fileno (fp), 0, SEEK_SET);
  while (1)
    {
      char *buf;
      Header h;

      if (ttyread (fp, &h, &buf, 0) == 0)
        {
          break;
        }

      if (lseek (fileno (fp), 0, SEEK_CUR) < raw_seek_offset)
        {
          seek_offset_clrscr = lseek (fileno (fp), 0, SEEK_CUR);
        }

      free (buf);
    }

}

void
ttyskipall (FILE * fp)
{
  /*
   * Skip all records.
   */
  ttyplay (fp, 0, ttyread, ttynowrite, ttynowait, 0);
}

void
ttyplayback (FILE * fp, double speed, ReadFunc read_func, WaitFunc wait_func)
{
  ttyplay (fp, speed, ttyread, ttywrite, wait_func, 0);
}

void
ttypeek (FILE * fp, double speed, ReadFunc read_func, WaitFunc wait_func)
{
  ttyskipall (fp);
  set_seek_offset_clrscr (fp);
  if (seek_offset_clrscr)
    {
      ttyplay (fp, 0, ttyread, ttywrite, ttynowait, seek_offset_clrscr);
    }
  ttyplay (fp, speed, ttypread, ttywrite, ttynowait, 0);
}


int
ttyplay_main (char *ttyfile, int mode, int rstripgfx)
{
  double speed = 1.0;
  ReadFunc read_func = ttyread;
  WaitFunc wait_func = ttywait;
  ProcessFunc process = ttyplayback;
  FILE *input = stdin;
  struct termios old, new;

  ttyfile_local = ttyfile;

  /* strip graphics mode flag */
  bstripgfx = rstripgfx;
  if (bstripgfx)
    populate_gfx_array (DEC_GRAPHICS);

  seek_offset_clrscr = 0;

  if (mode == 1)
    process = ttypeek;

  input = efopen (ttyfile, "r");

  tcgetattr (0, &old);          /* Get current terminal state */
  new = old;                    /* Make a copy */
  new.c_lflag &= ~(ICANON | ECHO | ECHONL); /* unbuffered, no echo */
  tcsetattr (0, TCSANOW, &new); /* Make it current */

  process (input, speed, read_func, wait_func);
  tcsetattr (0, TCSANOW, &old); /* Return terminal state */

  return 0;
}
