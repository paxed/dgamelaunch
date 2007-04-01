/* Functions common to both dgamelaunch itself and dgl-wall. */

#include "dgamelaunch.h"
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

extern FILE* yyin;
extern int yyparse ();

/* Data structures */
struct dg_config **myconfig = NULL;
struct dg_config defconfig = {
    /* chroot = */ /*"/var/lib/dgamelaunch/",*/
  /* game_path = */ "/bin/nethack",
  /* game_name = */ "NetHack",
  /* shortname = */ "NH",
  /* chdir = */ NULL,
  /* dglroot = *//*  "/dgldir/",*/
  /* lockfile = */ /*"/dgl-lock",*/
  /* passwd = */ /*"/dgl-login",*/
  /* banner = */ /*"/dgl-banner",*/
  /* rcfile = */ NULL, /*"/dgl-default-rcfile",*/
  /* spool = */ "/var/mail/",
  /* shed_user = */ /*"games",*/
  /* shed_group = */ /*"games",*/
  /* shed_uid = *//* 5,*/
  /* shed_gid = */ /*60,*/ /* games:games in Debian */
  /* max = */ /*64000,*/
  /* savefilefmt = */ "", /* don't do this by default */
  /* inprogressdir = */ "inprogress/",
  /* num_args = */ 0,
  /* bin_args = */ NULL,
  /* rc_fmt = */ "%rrcfiles/%n.nethackrc" /* [dglroot]rcfiles/[username].nethackrc */
};

char* config = NULL;
int silent = 0;
/*int set_max = 0;*/ /* XXX */
int loggedin = 0;
char *chosen_name;
int num_games = 0;

struct dg_globalconfig globalconfig;

/*
 * replace following codes with variables:
 * %u == shed_uid (number)
 * %n == user name (string)
 * %r == chroot (string)
 */
char *
dgl_format_str(int game, struct dg_user *me, char *str)
{
    static char buf[1024];
    char *f, *p, *end;
    int ispercent = 0;

    if (!str) return NULL;

    f = str;
    p = buf;
    end = buf + sizeof(buf) - 10;

    while (*f) {
	if (ispercent) {
	    switch (*f) {
  	    case 'u':
		snprintf (p, end + 1 - p, "%d", globalconfig.shed_uid);
		while (*p != '\0')
		    p++;
		break;
  	    case 'n':
		snprintf (p, end + 1 - p, "%s", me->username);
		while (*p != '\0')
		    p++;
		break;
	    case 'r':
		snprintf (p, end + 1 - p, "%s", globalconfig.dglroot);
		while (*p != '\0')
		    p++;
		break;
  	    default:
		*p = *f;
		if (p < end)
		    p++;
	    }
	    ispercent = 0;
	} else {
	    if (*f == '%')
		ispercent = 1;
	    else {
		*p = *f;
		if (p < end)
		    p++;
	    }
	}
	f++;
    }
    *p = '\0';

    return buf;
}


struct dg_game **
populate_games (int xgame, int *l)
{
  int fd, len, n, is_nhext, pid;
  DIR *pdir;
  struct dirent *pdirent;
  struct stat pstat;
  char fullname[130], ttyrecname[130], pidws[80];
  char *replacestr, *dir, *p;
  struct dg_game **games = NULL;
  struct flock fl = { 0 };
  size_t slen;

  int game;

  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  len = 0;

  for (game = ((xgame < 0) ? 0 : xgame); game <= ((xgame < 0) ? num_games : xgame); game++) {

   slen = strlen(globalconfig.dglroot) + strlen(myconfig[game]->inprogressdir) + 1;
   dir = malloc(slen);
   snprintf(dir, slen, "%s%s", globalconfig.dglroot, myconfig[game]->inprogressdir);

   if (!(pdir = opendir (dir)))
    graceful_exit (140);

   while ((pdirent = readdir (pdir)))
    {
      if (!strcmp (pdirent->d_name, ".") || !strcmp (pdirent->d_name, ".."))
        continue;

      is_nhext = !strcmp (pdirent->d_name + strlen (pdirent->d_name) - 6, ".nhext");

      snprintf (fullname, 130, "%s%s%s", globalconfig.dglroot, myconfig[game]->inprogressdir, pdirent->d_name);

      fd = 0;
      /* O_RDWR here should be O_RDONLY, but we need to test for
       * an exclusive lock */
      fd = open (fullname, O_RDWR);
      if (fd >= 0 && (is_nhext || fcntl (fd, F_SETLK, &fl) == -1))
        {

          /* stat to check idle status */
	  if (!is_nhext)
	    {
	      snprintf (ttyrecname, 130, "%sttyrec/%s", globalconfig.dglroot, pdirent->d_name);
	      replacestr = strchr (ttyrecname, ':');
	      if (!replacestr)
		graceful_exit (145);
	      replacestr[0] = '/';
	    }
          if (is_nhext || !stat (ttyrecname, &pstat))
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

	      games[len]->gamenum = game;

	      n = read(fd, pidws, sizeof(pidws) - 1);
	      if (n > 0)
	        {
		  pidws[n] = '\0';
		  p = pidws;
		}
	      else
		p = "";
	      pid = atoi(p);
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
	      if (is_nhext)
	        {
		  if (kill (pid, 0) != 0)
		    {
		      /* Dead game */
		      free (games[len]->ttyrec_fn);
		      free (games[len]->name);
		      free (games[len]->date);
		      free (games[len]->time);
		      free (games[len]);
		      unlink (fullname);
		    }
		  else
		    len++;
		}
	      else
	        {
		  if (games[len]->ws_row < 4 || games[len]->ws_col < 4)
		  {
		    games[len]->ws_row = 24;
		    games[len]->ws_col = 80;
		  }
		  len++;
		}
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
  }
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
  int tmp;

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
      /*      fprintf(stderr, "DEFCONFIG: %s\n", DEFCONFIG);*/
    config = DEFCONFIG;
    if ((config_file = fopen(DEFCONFIG, "r")) != NULL)
    {
      yyin = config_file;
      /*      fprintf(stderr, "PARSING\n");*/
      yyparse();
      /*      fprintf(stderr, "PARSED\n");*/
      fclose(config_file);
    }
#else
    /*      fprintf(stderr, "NO DEFCONFIG\n");*/
      myconfig = calloc(DIFF_GAMES, sizeof(myconfig[0]));
    for (tmp = 0; tmp < DIFF_GAMES; tmp++)
	myconfig[tmp] = NULL;
    myconfig[0] = &defconfig;
    return;
#endif
  }

  if (!myconfig) /* a parse error occurred */
  {
      /*      fprintf(stderr, "PARSE ERROR\n");*/
      myconfig = calloc(DIFF_GAMES, sizeof(myconfig[0]));
    for (tmp = 0; tmp < DIFF_GAMES; tmp++)
	myconfig[tmp] = NULL;
    myconfig[0] = &defconfig;
    return;
  }
  /* Fill the rest with defaults */

  for (tmp = 0; tmp < DIFF_GAMES; tmp++) {

      if (!myconfig[tmp]->game_path) myconfig[tmp]->game_path = defconfig.game_path;
      if (!myconfig[tmp]->game_name) myconfig[tmp]->game_name = defconfig.game_name;
      if (!myconfig[tmp]->shortname) myconfig[tmp]->shortname = defconfig.shortname;
      if (!myconfig[tmp]->rcfile) myconfig[tmp]->rcfile = defconfig.rcfile;
      if (!myconfig[tmp]->spool) myconfig[tmp]->spool = defconfig.spool;
      if (!myconfig[tmp]->savefilefmt) myconfig[tmp]->savefilefmt = defconfig.savefilefmt;
      if (!myconfig[tmp]->inprogressdir) myconfig[tmp]->inprogressdir = defconfig.inprogressdir;

  }

  if (!globalconfig.chroot) globalconfig.chroot = "/var/lib/dgamelaunch/";

  if (globalconfig.max == 0) globalconfig.max = 64000;
  if (globalconfig.max_newnick_len == 0) globalconfig.max_newnick_len = 20;
  if (!globalconfig.dglroot) globalconfig.dglroot = "/dgldir/";
  if (!globalconfig.banner)  globalconfig.banner = "/dgl-banner";

  if (!globalconfig.passwd) globalconfig.passwd = "/dgl-login";
  if (!globalconfig.lockfile) globalconfig.lockfile = "/dgl-lock";
  if (!globalconfig.shed_user && globalconfig.shed_uid == (uid_t)-1)
	  {
	      struct passwd *pw;
	      if ((pw = getpwnam("games")))
		  globalconfig.shed_uid = pw->pw_uid;
	      else
		  globalconfig.shed_uid = 5; /* games uid in debian */
	  }

  if (!globalconfig.shed_group && globalconfig.shed_gid == (gid_t)-1)
	  {
	      struct group *gr;
	      if ((gr = getgrnam("games")))
		  globalconfig.shed_gid = gr->gr_gid;
	      else
		  globalconfig.shed_gid = 60; /* games gid in debian */
	  }

}
