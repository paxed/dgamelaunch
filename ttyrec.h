#ifndef __TTYREC_H__
#define __TTYREC_H__

#include <sys/time.h>
#include <sys/types.h>

typedef struct header
{
  struct timeval tv;
  size_t len;
}
Header;

extern void done (void);
extern void fail (void);
extern void fixtty (void);
extern void getslave (void);
extern void doinput (void);
extern void dooutput (void);
extern void doshell (char *);
extern void finish (int);

extern int ttyrec_main(char *username, char *ttyrec_filename);

extern pid_t child; /* nethack process */
extern int master, slave;
extern struct termios tt;
extern struct winsize win;

#endif
