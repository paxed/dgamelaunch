/* Functions common to both dgamelaunch itself and dgl-wall. */

#include "dgamelaunch.h"
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

extern FILE* yyin;
extern int yyparse ();

/* Data structures */
struct dg_config *myconfig = NULL;
struct dg_config defconfig = {
  /* chroot = */ "/var/lib/dgamelaunch/",
  /* nethack = */ "/bin/nethack",
  /* dglroot = */  "/dgldir/",
  /* lockfile = */ "/dgl-lock",
  /* passwd = */ "/dgl-login",
  /* banner = */ "/dgl-banner",
  /* rcfile = */ "/dgl-default-rcfile",
  /* spool = */ "/var/mail/",
  /* shed_user = */ "games",
  /* shed_group = */ "games",
  /* shed_uid = */ 5,
  /* shed_gid = */ 60, /* games:games in Debian */
  /* max = */ 64000 
};

char* config = NULL;
int silent = 0;
int set_max = 0; /* XXX */
int loggedin = 0;
char *chosen_name;

struct dg_game **
populate_games (int *l)
{
  int fd, len, n;
  DIR *pdir;
  struct dirent *pdirent;
  struct stat pstat;
  char fullname[130], ttyrecname[130], pidws[80];
  char *replacestr, *dir, *p;
  struct dg_game **games = NULL;
  struct flock fl = { 0 };
  size_t slen;

  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  len = 0;
  
  slen = strlen(myconfig->dglroot) + ARRAY_SIZE("inprogress/") + 1;
  dir = malloc(slen);
  snprintf(dir, slen, "%sinprogress/", myconfig->dglroot);

  if (!(pdir = opendir (dir)))
    graceful_exit (140);

  while ((pdirent = readdir (pdir)))
    {
      if (!strcmp (pdirent->d_name, ".") || !strcmp (pdirent->d_name, ".."))
        continue;

      snprintf (fullname, 130, "%sinprogress/%s", myconfig->dglroot, pdirent->d_name);

      fd = 0;
      /* O_RDWR here should be O_RDONLY, but we need to test for
       * an exclusive lock */
      fd = open (fullname, O_RDWR);
      if ((fd > 0) && fcntl (fd, F_SETLK, &fl) == -1)
        {

          /* stat to check idle status */
          snprintf (ttyrecname, 130, "%sttyrec/%s", myconfig->dglroot, pdirent->d_name);
          replacestr = strchr (ttyrecname, ':');
          if (!replacestr)
            graceful_exit (145);
          replacestr[0] = '/';
          if (!stat (ttyrecname, &pstat))
            {
              /* now it's a valid game for sure */
              games = realloc (games, sizeof (struct dg_game) * (len + 1));
              games[len] = malloc (sizeof (struct dg_game));
              games[len]->ttyrec_fn = strdup (pdirent->d_name);

              if (!(replacestr = strchr (pdirent->d_name, ':')))
                graceful_exit (146);
              else
                *replacestr = '\0';

              games[len]->name = malloc (strlen (pdirent->d_name) + 1);
              strlcpy (games[len]->name, pdirent->d_name,
                       strlen (pdirent->d_name) + 1);

              games[len]->date = malloc (11);
              strlcpy (games[len]->date, replacestr + 1, 11);

              games[len]->time = malloc (9);
              strlcpy (games[len]->time, replacestr + 12, 9);

              games[len]->idle_time = pstat.st_mtime;

	      n = read(fd, pidws, sizeof(pidws) - 1);
	      if (n > 0)
	        {
		  pidws[n] = '\0';
		  p = pidws;
		}
	      else
		p = "";
	      while (*p != '\0' && *p != '\n')
	        p++;
	      if (*p != '\0')
	        p++;
	      games[len]->ws_row = atoi(p);
	      while (*p != '\0' && *p != '\n')
	        p++;
	      if (*p != '\0')
	        p++;
	      games[len]->ws_col = atoi(p);
	      if (games[len]->ws_row < 4 || games[len]->ws_col < 4)
	        {
		  games[len]->ws_row = 24;
		  games[len]->ws_col = 80;
	        }

              len++;
            }
        }
      else
        {
          /* clean dead ones */
          unlink (fullname);
        }
      close (fd);

      fl.l_type = F_WRLCK;
    }

  closedir (pdir);
  *l = len;
  return games;
}

  void
graceful_exit (int status)
{
  /*FILE *fp;
     if (status != 1) 
     { 
     fp = fopen ("/crash.log", "a");
     char buf[100];
     sprintf (buf, "graceful_exit called with status %d", status);
     fputs (buf, fp);
     } 
     This doesn't work. Ever.
   */
  exit (status);
}

void
create_config ()
{
  FILE *config_file = NULL;

  if (config)
  {
    if ((config_file = fopen(config, "r")) != NULL)
    {
      yyin = config_file;
      yyparse();
      fclose(config_file);
      free (config);
    }
    else
    {
      fprintf(stderr, "ERROR: can't find or open %s for reading\n", config);
      graceful_exit(104);
      return;
    }
  }
  else
  {
#ifdef DEFCONFIG
    config = DEFCONFIG;
    if ((config_file = fopen(DEFCONFIG, "r")) != NULL)
    {
      yyin = config_file;
      yyparse();
      fclose(config_file);
    }
#else
    myconfig = &defconfig;
    return;
#endif
  }

  if (!myconfig) /* a parse error occurred */
  {
    myconfig = &defconfig;
    return;
  }
  /* Fill the rest with defaults */
  if (!myconfig->shed_user && myconfig->shed_uid == -1)
  {
    struct passwd *pw;
    if ((pw = getpwnam(defconfig.shed_user)))
      myconfig->shed_uid = pw->pw_uid;
    else
      myconfig->shed_uid = defconfig.shed_uid;
  }

  if (!myconfig->shed_group && myconfig->shed_gid == -1)
  {
    struct group *gr;
    if ((gr = getgrnam(defconfig.shed_group)))
      myconfig->shed_gid = gr->gr_gid;
    else
      myconfig->shed_gid = defconfig.shed_gid;
  }

  if (myconfig->max == 0 && !set_max) myconfig->max = defconfig.max;
  if (!myconfig->banner) myconfig->banner = defconfig.banner;
  if (!myconfig->chroot) myconfig->chroot = defconfig.chroot;
  if (!myconfig->nethack) myconfig->nethack = defconfig.nethack;
  if (!myconfig->dglroot) myconfig->dglroot = defconfig.dglroot;
  if (!myconfig->rcfile) myconfig->rcfile = defconfig.rcfile;
  if (!myconfig->spool) myconfig->spool = defconfig.spool;
  if (!myconfig->passwd) myconfig->passwd = defconfig.passwd;
  if (!myconfig->lockfile) myconfig->lockfile = defconfig.lockfile;
}
