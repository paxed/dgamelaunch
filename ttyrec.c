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
#include <fcntl.h>

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
#include <stdint.h>

#include "ttyrec.h"
#include "io.h"

#ifndef XCASE
# define XCASE 0
#endif
static void query_encoding(int game, char *username);

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

char last_ttyrec[512] = { '\0' };

int ancient_encoding = 0;

void
ttyrec_id(int game, char *username, char *ttyrec_filename)
{
    int i;
    time_t tstamp;
    Header h;
    char *buf = (char *)malloc(1024);
    char tmpbuf[256];
    char *server_id = banner_var_value("$SERVERID");
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
	    (server_id ? server_id : "Unknown"),
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
  ancient_encoding = myconfig[game]->encoding;
  if (ancient_encoding == -1)
      query_encoding(game, username);

  snprintf(last_ttyrec, 512, "%s", dirname);

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

static unsigned short charset_vt100[128] =
{
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x2192, 0x2190, 0x2191, 0x2193, 0x002f,
    0x2588, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x00a0,
#if 0
    0x25c6, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0x00b0, 0x00b1,
    0x2591, 0x240b, 0x2518, 0x2510, 0x250c, 0x2514, 0x253c, 0xf800,
    0xf801, 0x2500, 0xf803, 0xf804, 0x251c, 0x2524, 0x2534, 0x252c,
    0x2502, 0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3, 0x00b7, 0x007f,
#endif
    0x2666, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0x00b0, 0x00b1,
    0x2424, 0x240b, 0x2518, 0x2510, 0x250c, 0x2514, 0x253c, 0x23ba,
    0x23bb, 0x2500, 0x23bc, 0x23bd, 0x251c, 0x2524, 0x2534, 0x252c,
    0x2502, 0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3, 0x00b7, 0x0020,
};

static unsigned short charset_cp437[256] =
{
    // Real IBM charset has no control codes, but they are needed by
    // terminals.
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f,
#if 0
    0x0000, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
    0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c,
    0x25b6, 0x25c0, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8,
    0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc,
#endif
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x2302,
    0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
    0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
    0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
    0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
    0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
    0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
    0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
    0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
    0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
    0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
    0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
    0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
    0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0,
};

// there must be at least 4 bytes free, NOT CHECKED!
static int wctoutf8(char *d, uint32_t s)
{
    if (s < 0x80)
    {
        d[0] = s;
        return 1;
    }
    if (s < 0x800)
    {
        d[0] = ( s >>  6)         | 0xc0;
        d[1] = ( s        & 0x3f) | 0x80;
        return 2;
    }
    if (s < 0x10000)
    {
        d[0] = ( s >> 12)         | 0xe0;
        d[1] = ((s >>  6) & 0x3f) | 0x80;
        d[2] = ( s        & 0x3f) | 0x80;
        return 3;
    }
    if (s < 0x110000)
    {
        d[0] = ( s >> 18)         | 0xf0;
        d[1] = ((s >> 12) & 0x3f) | 0x80;
        d[2] = ((s >>  6) & 0x3f) | 0x80;
        d[3] = ( s        & 0x3f) | 0x80;
        return 4;
    }
    // Invalid char marker (U+FFFD).
    d[0] = 0xef;
    d[1] = 0xbf;
    d[2] = 0xbd;
    return 3;
}

void
dooutput (int max_idle_time)
{
  int cc, i, len;
  time_t tvec, time ();
  char obuf[BUFSIZ], ubuf[BUFSIZ*4+2], *ctime (), *out;
  int galt = 0; // vt100 G switch

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

      switch (ancient_encoding)
      {
      case 0:	// UTF-8
      default:
          h.len = cc;
          gettimeofday (&h.tv, NULL);
          (void) write (1, obuf, cc);
          (void) write_header (fscript, &h);
          (void) fwrite (obuf, 1, cc, fscript);
          break;
      case 1:	// IBM
          out = ubuf;
          // Old Crawl emits useless toggles of vt100 mode, even though it
          // never uses them in this mode.  And they break stuff...
          for (i = 0; i < cc; i++)
          {
              if (galt == 2) // ignore "ESC ( 0", "ESC ( B" and anything such
              {
                  galt = 0;
                  continue;
              }
              else if (galt == 1)
                  if (obuf[i] == '(')
                  {
                      galt = 2;
                      continue;
                  }
                  else
                  {
                      galt = 0; // false alarm, emit ESC and continue
                      *out ++ = 27;
                  }
              else if (obuf[i] == 27)
                  {
                      galt = 1;
                      continue;
                  }
              else if (obuf[i] == 14 || obuf[i] == 15)
                  continue;
              out += wctoutf8(out, charset_cp437[(unsigned char)obuf[i]]);
          }
          h.len = len = out - ubuf;
          gettimeofday(&h.tv, NULL);
          write(1, ubuf, len);
          write_header(fscript, &h);
          fwrite(ubuf, 1, len, fscript);
          break;
      case 2:	// DEC
          out = ubuf;
          for (i = 0; i < cc; i++)
          {
              if (obuf[i] == 14)
                  galt = 1;
              else if (obuf[i] == 15)
                  galt = 0;
              else if (obuf[i] & 0x80) // strictly 7-bit
                  out += wctoutf8(out, 0xFFFD); // or we could assume some other charset
              else if (galt)
                  out += wctoutf8(out, charset_vt100[(int)obuf[i]]);
              else
                  *out++ = obuf[i];
          }
          h.len = len = out - ubuf;
          gettimeofday(&h.tv, NULL);
          write(1, ubuf, len);
          write_header(fscript, &h);
          fwrite(ubuf, 1, len, fscript);
          break;
      }
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

int encoding_by_name(const char *enc)
{
    if (!strcasecmp(enc, "UTF-8") || !strcasecmp(enc, "UNICODE"))
        return 0;
    if (!strcasecmp(enc, "IBM") || !strcasecmp(enc, "CP437"))
        return 1;
    if (!strcasecmp(enc, "DEC"))
        return 2;
    if (!strcasecmp(enc, "ASCII"))
        return 1; // what to do with invalid chars?
    return -1;
}

static void call_print_charset(char *exe, char **args)
{
    char **args2, **a;
    int nargs = 0;
    for (args2 = args; *args2; args2++)
        nargs++;
    args2 = calloc(nargs + 1, sizeof(char*));
    for (a = args2; *args; args++)
        *a++ = *args;

    *a++ = "--print-charset";
    *a = 0;
    execvp(exe, args2);
}

static void query_encoding(int game, char *username)
{
    int son;
    int p[2];
    int null;
    struct timeval  tv;
    fd_set          se;
    char buf[128];

    if (pipe(p))
        perror("pipe");
    switch(son = fork())
    {
    case -1:
        perror("fork");
        fail();
    case 0:
        null = open("/dev/null", O_RDONLY);
        if (null != -1)
            dup2(null, 0), close(null);
        else
        {
            fprintf(stderr, "Error: can't open /dev/null\n");
            // non-fatal, but if the child opens some other file, it might
            // get confused
            close(0);
        }
        dup2(p[1], 1);
        close(p[1]);
        close(p[0]);
        call_print_charset(myconfig[game]->game_path, myconfig[game]->bin_args);
        exit(1);
    }
    close(p[1]);

    FD_ZERO(&se);
    FD_SET(p[0],&se);
    tv.tv_sec = 60; // FIXME: a huge delay, in case there's a db rebuild
    tv.tv_usec = 0;
    if (select(p[0]+1,&se,NULL,NULL,&tv) != 1)
    {
        fprintf(stderr, "Error: can't obtain charset info.\nPress any key...\n");
        read(0, buf, 1);
        close(p[0]); // SIGPIPE
        kill(son, SIGTERM); // and SIGTERM for a good measure
        waitpid(son, 0, 0);
        ancient_encoding = 0;
        return;
    }

    // Sending _one_ short message over a pipe is atomic on all systems I know.
    // Don't assume this on about any other file descriptor.
    read(p[0], buf, sizeof(buf)-1);
    buf[sizeof(buf)-1] = 0;
    close(p[0]);
    kill(son, SIGTERM);
    waitpid(son, 0, 0);
    if (strchr(buf, '\n'))
        *strchr(buf, '\n') = 0;
    ancient_encoding = encoding_by_name(buf);
    if (ancient_encoding == -1)
    {
        fprintf(stderr, "Error: unknown encoding \"%s\"\nPress any key...\n", buf);
        read(0, buf, 1);
    }
}
