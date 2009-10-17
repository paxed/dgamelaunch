/* Functions common to both dgamelaunch itself and dgl-wall. */

#include "dgamelaunch.h"
#include "ttyrec.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <curses.h>

extern FILE* yyin;
extern int yyparse ();

/* Data structures */
struct dg_config **myconfig = NULL;
struct dg_config defconfig = {
    /* chroot = */ /*"/var/lib/dgamelaunch/",*/
  /* game_path = */ "/bin/nethack",
  /* game_name = */ "NetHack",
  /* shortname = */ "NH",
  /* chdir = */ /*NULL,*/
  /* mkdir = */ /*NULL,*/
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
  /* savefilefmt = */ /*"",*/ /* don't do this by default */
  /* inprogressdir = */ "%rinprogress/",
  NULL,
  /* num_args = */ 0,
  /* bin_args = */ NULL,
  /* rc_fmt = */ "%rrcfiles/%n.nethackrc", /* [dglroot]rcfiles/[username].nethackrc */
  /* cmdqueue = */ NULL
};

char* config = NULL;
int silent = 0;
int loggedin = 0;
char *chosen_name;
int num_games = 0;

int selected_game = 0;
int return_from_submenu = 0;

mode_t default_fmode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;

struct dg_globalconfig globalconfig;


int
check_retard(int reset)
{
    static int retardation = 0;  /* counter for retarded clients & flooding */
    if (reset) retardation = 0;
    else retardation++;
    return ((retardation > 20) ? 1 : 0);
}


struct dg_menu *
dgl_find_menu(char *menuname)
{
    struct dg_menulist *tmp = globalconfig.menulist;

    while (tmp) {
	if (!strcmp(tmp->menuname, menuname)) return tmp->menu;
	tmp = tmp->next;
    }
    return NULL;
}

/*
 * replace following codes with variables:
 * %u == shed_uid (number)
 * %n == user name (string; gotten from 'me', or from 'plrname' if 'me' is null)
 * %r == chroot (string)  (aka "dglroot" config var)
 * %g == game name
 */
char *
dgl_format_str(int game, struct dg_user *me, char *str, char *plrname)
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
		if (me) snprintf (p, end + 1 - p, "%s", me->username);
		else if (plrname) snprintf(p, end + 1 - p, "%s", plrname);
		else return NULL;
		while (*p != '\0')
		    p++;
		break;
  	    case 'g':
		if (game >= 0 && game < num_games && myconfig[game]) snprintf (p, end + 1 - p, "%s", myconfig[game]->game_name);
		else return NULL;
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

int
dgl_exec_cmdqueue(struct dg_cmdpart *queue, int game, struct dg_user *me)
{
    int i;
    struct dg_cmdpart *tmp = queue;
    char *p1;
    char *p2;

    if (!queue) return 1;

    p1 = (char *)malloc(1024);
    p2 = (char *)malloc(1024);

    if (!p1 || !p2) return 1;

    return_from_submenu = 0;

    while (tmp && !return_from_submenu) {
	if (tmp->param1) strcpy(p1, dgl_format_str(game, me, tmp->param1, NULL));
	if (tmp->param2) strcpy(p2, dgl_format_str(game, me, tmp->param2, NULL));

	switch (tmp->cmd) {
	default: break;
	case DGLCMD_MKDIR:
	    if (p1 && (access(p1, F_OK) != 0)) mkdir(p1, 0755);
	    break;
	case DGLCMD_UNLINK:
	    if (p1 && (access(p1, F_OK) != 0)) unlink(p1);
	    break;
	case DGLCMD_CHDIR:
	    if (p1) chdir(p1);
	    break;
	case DGLCMD_IF_NX_CP:
	    if (p1 && p2) {
		FILE *tmpfile;
		tmpfile = fopen(p2, "r");
		if (tmpfile) break;
	    }
	    /* else fall through to cp */
	case DGLCMD_CP:
	    if (p1 && p2) {
		FILE *cannedf, *newfile;
		char buf[1024];
		size_t bytes;
		/* FIXME: use nethack-themed error messages here, as per write_canned_rcfile() */
		if (!(cannedf = fopen (p1, "r"))) break;
		if (!(newfile = fopen (p2, "w"))) break;
		while ((bytes = fread (buf, 1, 1024, cannedf)) > 0) {
		    if (fwrite (buf, 1, bytes, newfile) != bytes) {
			if (ferror (newfile)) {
			    fclose (cannedf);
			    fclose (newfile);
			    break;
			}
		    }
		}
		fclose (cannedf);
		fclose (newfile);
		chmod (p2, default_fmode);
	    }
	    break;
	case DGLCMD_EXEC:
	    if (p1 && p2) {
		pid_t child;
		char *myargv[3];

		myargv[0] = p1;
		myargv[1] = p2;
		myargv[2] = 0;

		endwin();
		child = fork();
		if (child == -1) {
		    perror("fork");
		    debug_write("exec-command fork failed");
		    graceful_exit(114);
		} else if (child == 0) {
		    execvp(p1, myargv);
		    exit(0);
		} else
		    waitpid(child, NULL, 0);
		refresh();
		check_retard(1);
	    }
	    break;
	case DGLCMD_SETENV:
	    if (p1 && p2) mysetenv(p1, p2, 1);
	    break;
	case DGLCMD_CHPASSWD:
	    if (loggedin) changepw(1);
	    break;
	case DGLCMD_CHMAIL:
	    if (loggedin) change_email();
	    break;
	case DGLCMD_WATCH_MENU:
	    inprogressmenu(game);
	    break;
	case DGLCMD_LOGIN:
	    if (!loggedin) loginprompt(0);
	    if (loggedin) runmenuloop(dgl_find_menu("mainmenu_user"));
	    break;
	case DGLCMD_REGISTER:
	    if (!loggedin && globalconfig.allow_registration) newuser();
	    break;
	case DGLCMD_QUIT:
	    debug_write("command: quit");
	    graceful_exit(0);
	    /* break; */
	case DGLCMD_SUBMENU:
	    if (p1)
		runmenuloop(dgl_find_menu(p1));
	    break;
	case DGLCMD_RETURN:
	    return_from_submenu = 1;
	    break;
	case DGLCMD_EDITOPTIONS:
	    if (loggedin && p1) {
		int i;
		for (i = 0; i < num_games; i++) {
		    if ((!strcmp(myconfig[i]->game_name, p1) || !strcmp(myconfig[i]->shortname, p1)) && myconfig[i]->rcfile) {
			editoptions(i);
			check_retard(1);
			break;
		    }
		}
	    }
	    break;
	case DGLCMD_PLAYGAME:
	    if (loggedin && me && p1) {
		int userchoice, i;
		char *tmpstr;
		for (userchoice = 0; userchoice < num_games; userchoice++) {
		    if (!strcmp(myconfig[userchoice]->game_name, p1) || !strcmp(myconfig[userchoice]->shortname, p1)) {
			if (purge_stale_locks(userchoice)) {
			    if (myconfig[userchoice]->rcfile) {
				if (access (dgl_format_str(userchoice, me, myconfig[userchoice]->rc_fmt, NULL), R_OK) == -1)
				    write_canned_rcfile (userchoice, dgl_format_str(userchoice, me, myconfig[userchoice]->rc_fmt, NULL));
			    }

			    setproctitle("%s [playing %s]", me->username, myconfig[userchoice]->shortname);

			    endwin ();
			    signal(SIGWINCH, SIG_DFL);

			    /* first run the generic "do these when a game is started" commands */
			    dgl_exec_cmdqueue(globalconfig.cmdqueue[DGLTIME_GAMESTART], userchoice, me);
			    /* then run the game-specific commands */
			    dgl_exec_cmdqueue(myconfig[userchoice]->cmdqueue, userchoice, me);

			    /* fix the variables in the arguments */
			    for (i = 0; i < myconfig[userchoice]->num_args; i++) {
				tmpstr = strdup(dgl_format_str(userchoice, me, myconfig[userchoice]->bin_args[i], NULL));
				free(myconfig[userchoice]->bin_args[i]);
				myconfig[userchoice]->bin_args[i] = tmpstr;
			    }

			    /* launch program */
			    ttyrec_main (userchoice, me->username,
					 dgl_format_str(userchoice, me, myconfig[userchoice]->ttyrecdir, NULL),
					 gen_ttyrec_filename());

			    /* lastly, run the generic "do these when a game is left" commands */
			    dgl_exec_cmdqueue(globalconfig.cmdqueue[DGLTIME_GAMEEND], userchoice, me);

			    setproctitle ("%s", me->username);
			    initcurses ();
			    check_retard(1); /* reset retard counter */
			}
			break;
		    }
		}
	    }
	    break;
	}
	tmp = tmp->next;
    }

    free(p1);
    free(p2);

    return 0;
}



static int
sort_game_username(const void *g1, const void *g2)
{
    const struct dg_game *game1 = *(const struct dg_game **)g1;
    const struct dg_game *game2 = *(const struct dg_game **)g2;
    return strcasecmp(game1->name, game2->name);
}

static int
sort_game_idletime(const void *g1, const void *g2)
{
    const struct dg_game *game1 = *(const struct dg_game **)g1;
    const struct dg_game *game2 = *(const struct dg_game **)g2;
    if (game2->idle_time != game1->idle_time)
	return difftime(game2->idle_time, game1->idle_time);
    else
	return strcasecmp(game1->name, game2->name);
}

struct dg_game **
sort_games (struct dg_game **games, int len, dg_sortmode sortmode)
{
    switch (sortmode) {
    case SORTMODE_USERNAME: qsort(games, len, sizeof(struct dg_game *), sort_game_username); break;
    case SORTMODE_IDLETIME: qsort(games, len, sizeof(struct dg_game *), sort_game_idletime); break;
    default: ;
    }
    return games;
}

#ifdef USE_DEBUGFILE
void
debug_write(char *str)
{
    FILE *fp;
    fp = fopen("/dgldebug.log", "a");
    if (!fp) return;
    fprintf(fp, "%s\n", str);
    fclose(fp);

}
#endif /* USE_DEBUGFILE */

struct dg_game **
populate_games (int xgame, int *l, struct dg_user *me)
{
  int fd, len, n, is_nhext, pid;
  DIR *pdir;
  struct dirent *pdirent;
  struct stat pstat;
  char fullname[130], ttyrecname[130], pidws[80], playername[30];
  char *replacestr, *dir, *p;
  struct dg_game **games = NULL;
  struct flock fl = { 0 };

  int game;

  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  len = 0;

  for (game = ((xgame < 0) ? 0 : xgame); game < ((xgame <= 0) ? num_games : (xgame+1)); game++) {

      dir = strdup(dgl_format_str(game, me, myconfig[game]->inprogressdir, NULL));
      if (!dir) continue;

      if (!(pdir = opendir (dir))) {
	  debug_write("cannot open inprogress-dir");
	  graceful_exit (140);
      }

   while ((pdirent = readdir (pdir)))
    {
	char *inprog = NULL;
      if (!strcmp (pdirent->d_name, ".") || !strcmp (pdirent->d_name, ".."))
        continue;

      is_nhext = !strcmp (pdirent->d_name + strlen (pdirent->d_name) - 6, ".nhext");

      inprog = dgl_format_str(game, me, myconfig[game]->inprogressdir, NULL);

      if (!inprog) continue;

      snprintf (fullname, 130, "%s%s", inprog, pdirent->d_name);

      fd = 0;
      /* O_RDWR here should be O_RDONLY, but we need to test for
       * an exclusive lock */
      fd = open (fullname, O_RDWR);
      if (fd >= 0 && (is_nhext || fcntl (fd, F_SETLK, &fl) == -1))
        {

          /* stat to check idle status */
	  if (!is_nhext)
	    {
		char *ttrecdir = NULL;
		strncpy(playername, pdirent->d_name, 29);
		playername[29] = '\0';
		if ((replacestr = strchr(playername, ':')))
		    *replacestr = '\0';

              replacestr = strchr(pdirent->d_name, ':');
              if (!replacestr) {
		  debug_write("inprogress-filename does not have ':'");
		  graceful_exit(145);
	      }
              replacestr++;

	      ttrecdir = dgl_format_str(game, me, myconfig[game]->ttyrecdir, playername);
	      if (!ttrecdir) continue;
              snprintf (ttyrecname, 130, "%s%s", ttrecdir, replacestr);
	    }
          if (is_nhext || !stat (ttyrecname, &pstat))
            {
              /* now it's a valid game for sure */
              games = realloc (games, sizeof (struct dg_game) * (len + 1));
              games[len] = malloc (sizeof (struct dg_game));
              games[len]->ttyrec_fn = strdup (ttyrecname);

              if (!(replacestr = strchr (pdirent->d_name, ':'))) {
		  debug_write("inprogress-filename does not have ':', pt. 2");
		  graceful_exit (146);
              } else
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
  endwin();
  exit (status);
}

void
create_config ()
{
  FILE *config_file = NULL;
  int tmp;

  if (!globalconfig.allow_registration) globalconfig.allow_registration = 1;
  globalconfig.menulist = NULL;
  globalconfig.server_id = NULL;

  globalconfig.shed_uid = (uid_t)-1;
  globalconfig.shed_gid = (gid_t)-1;

  globalconfig.sortmode = SORTMODE_NONE;

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
      debug_write("cannot read config file");
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
    } else {
	fprintf(stderr, "ERROR: can't find or open %s for reading\n", config);
	debug_write("cannot read default config file");
	graceful_exit(105);
	return;
    }
#else
    num_games = 0;
    myconfig = calloc(1, sizeof(myconfig[0]));
    myconfig[0] = &defconfig;
    return;
#endif
  }

  if (!myconfig) /* a parse error occurred */
  {
      fprintf(stderr, "ERROR: configuration parsing failed\n");
      debug_write("config file parsing failed");
      graceful_exit(113);
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
