/* IMPORTANT defines */

#ifndef __DGAMELAUNCH_H
#define __DGAMELAUNCH_H

#include <time.h>

/* Default - should work everywhere */
#define USE_OPENPTY
#define NOSTREAMS

struct dg_user
{
  char *username;
  char *email;
  char *env;
  char *password;
  int flags;
};

struct dg_banner
{
  char **lines;
  unsigned int len;
};

struct dg_game
{
  char *ttyrec_fn;
  char *name;
  char *date;
  char *time;
  time_t idle_time;
};

#define SHED_UID 5              /* the uid to shed privs to */
#define SHED_GID 60             /* the gid to shed privs to */
#define MAXUSERS 64000          /* solves some preallocation issues. */

#define LOC_CHROOT		"/var/lib/dgamelaunch/"
#define LOC_NETHACK		"/bin/nethack"
#define LOC_DGLROOT		"/dgldir/"
#define LOC_DGLDIR		LOC_DGLROOT "rcfiles/"
#define LOC_TTYRECDIR		LOC_DGLROOT "ttyrec/"
#define LOC_INPROGRESSDIR	LOC_DGLROOT "inprogress/"
#define LOC_SPOOLDIR		"/var/mail"
#define LOC_CANNED		"/dgl-default-rcfile"
#define LOC_BANNER		"/dgl-banner"

/* dgamelaunch.c function prototypes */
extern void ttyrec_getmaster (void);
extern void gen_ttyrec_filename (void);
extern void gen_inprogress_lock (void);
extern void catch_sighup (int signum);
extern void loadbanner (struct dg_banner *ban);
extern void drawbanner (unsigned int start_line, unsigned int howmany);
extern struct dg_game **populate_games (int *l);
extern void inprogressmenu (void);
extern void changepw (void);
extern void domailuser (char *username);
extern void drawmenu (void);
extern void freefile (void);
extern void initncurses (void);
extern struct dg_user *deep_copy (struct dg_user *src);
extern void loginprompt (void);
extern void newuser (void);
extern int passwordgood (char *cpw);
extern int readfile (int nolock);
extern int userexist (char *cname);
extern void write_canned_rcfile (char *target);
extern void editoptions (void);
extern void writefile (int requirenew);

/* strlcpy.c */
extern size_t strlcpy(char *dst, const char *src, size_t siz);

#endif
