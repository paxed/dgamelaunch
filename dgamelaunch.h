/* IMPORTANT defines */

#ifndef __DGAMELAUNCH_H
#define __DGAMELAUNCH_H

#include <sys/param.h>
#include <sys/types.h>
#include <time.h>

/* Default - should work everywhere */
#if defined(__linux__) || defined(BSD)
# define USE_OPENPTY
# define NOSTREAMS
#endif

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

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
  int ws_row, ws_col; /* Window size */
};

struct dg_config
{
  char* chroot;
  char* nethack;
  char* dglroot;
  char* lockfile;
  char* passwd;
  char* banner;
  char* rcfile;
  char* spool;
  char* shed_user;
  char* shed_group;
  uid_t shed_uid;
  gid_t shed_gid;
  unsigned long max;
  char* savefilefmt;
};

/* Global variables */
extern char* config; /* file path */
extern struct dg_config *myconfig;
extern char *chosen_name;
extern int loggedin;
extern int silent;
extern int set_max;

/* dgamelaunch.c */
extern void create_config(void);
extern void ttyrec_getmaster(void);
extern char *gen_ttyrec_filename(void);
extern char *gen_inprogress_lock(pid_t pid, char *ttyrec_filename);
extern void catch_sighup(int signum);
extern void loadbanner(struct dg_banner *ban);
extern void drawbanner(unsigned int start_line, unsigned int howmany);
extern struct dg_game **populate_games(int *l);
extern void inprogressmenu(void);
extern void change_email(void);
extern int changepw(int dowrite);
extern void domailuser(char *username);
extern void drawmenu(void);
extern void freefile(void);
extern void initcurses(void);
extern void loginprompt(int from_ttyplay);
extern void newuser(void);
extern int passwordgood(char *cpw);
extern int readfile(int nolock);
extern int userexist(char *cname);
extern void write_canned_rcfile(char *target);
extern void editoptions(void);
extern void writefile(int requirenew);
extern void graceful_exit(int status);
extern int purge_stale_locks(void);
extern void menuloop(void);
extern void ttyrec_getpty(void);
#if !defined(BSD) && !defined(__linux__)
extern int mysetenv (const char* name, const char* value, int overwrite);
#else
# define mysetenv setenv
#endif

/* strlcpy.c */
extern size_t strlcpy (char *dst, const char *src, size_t siz);
extern size_t strlcat (char *dst, const char *src, size_t siz);

/* mygetnstr.c */
extern int mygetnstr(char *buf, int maxlen, int doecho);

#endif
