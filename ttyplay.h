#ifndef INCLUDED_ttyplay_h
#define INCLUDED_ttyplay_h

#include <stdio.h>
#include "ttyrec.h"

int ttyplay_main (char *ttyfile, int mode, int rstripgfx);

typedef double (*WaitFunc) (struct timeval prev,
                            struct timeval cur, double speed);
typedef int (*ReadFunc) (FILE * fp, Header * h, char **buf, int pread);
typedef void (*WriteFunc) (char *buf, int len);
typedef void (*ProcessFunc) (FILE * fp, double speed,
                             ReadFunc read_func, WaitFunc wait_func);

#endif /* !INCLUDED_ttyrec_h */
