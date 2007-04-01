%{

#define YYSTACK_USE_ALLOCA 0

#include <sys/types.h>

#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dgamelaunch.h"

extern int yylex(void);
extern void yyerror(const char*);
extern char *yytext;
extern unsigned int line, col;

extern int num_games;
int ncnf = 0;

static const char* lookup_token (int t);

%}

%union {
	char* s;
	int kt;
	unsigned long i;
}

%token TYPE_SUSER TYPE_SGROUP TYPE_SGID TYPE_SUID TYPE_MAX TYPE_MAXNICKLEN
%token TYPE_PATH_CHDIR TYPE_PATH_MKDIR TYPE_GAME_SHORT_NAME
%token TYPE_PATH_GAME TYPE_NAME_GAME TYPE_PATH_DGLDIR TYPE_PATH_SPOOL
%token TYPE_PATH_BANNER TYPE_PATH_CANNED TYPE_PATH_CHROOT TYPE_GAMENUM
%token TYPE_PATH_PASSWD TYPE_PATH_LOCKFILE TYPE_PATH_SAVEFILEFMT
%token TYPE_MALSTRING TYPE_PATH_INPROGRESS TYPE_GAME_ARGS TYPE_RC_FMT
%token <s> TYPE_VALUE
%token <i> TYPE_NUMBER
%type  <kt> KeyType

%%

Configuration: KeyPairs
	| { if (!silent) fprintf(stderr, "%s: no settings, proceeding with defaults\n", config); }
	;

KeyPairs: KeyPairs KeyPair
	| KeyPair
	;

KeyPair: KeyType '=' TYPE_VALUE {
  struct group* gr;
  struct passwd* usr;

  if (ncnf < 0 || ncnf >= DIFF_GAMES) {
      fprintf(stderr, "%s:%d: illegal game number, bailing out\n",
        config, line);
      exit(1);
  }

  if (!myconfig)
  {
    int tmp;
    myconfig = calloc(DIFF_GAMES, sizeof(myconfig[0]));
    for (tmp = 0; tmp < DIFF_GAMES; tmp++) {
	myconfig[tmp] = calloc(1, sizeof(struct dg_config));
    }
    globalconfig.shed_uid = (uid_t)-1;
    globalconfig.shed_gid = (gid_t)-1;
  }

  switch ($1)
  {
    case TYPE_SGROUP:
      if (globalconfig.shed_gid != (gid_t)-1)
        break;

      globalconfig.shed_group = strdup($3);
      if ((gr = getgrnam($3)) != NULL)
      {
	globalconfig.shed_gid = gr->gr_gid;
	if (!silent)
	  fprintf(stderr, "%s:%d: suggest replacing 'shed_group = \"%s\"' line with 'shed_gid = %d'\n",
	  config, line, $3, gr->gr_gid);
      }
      else
      {
        if (!silent)
          fprintf(stderr, "%s:%d: no such group '%s'\n", config, line, $3);
      }

      break;
    case TYPE_SUSER:
      if (globalconfig.shed_uid != (uid_t)-1)
        break;

      if (!strcmp($3, "root"))
      {
        fprintf(stderr, "%s:%d: I refuse to run as root! Aborting.\n", config, line);
	graceful_exit(1);
      }
      globalconfig.shed_user = strdup($3);
      if ((usr = getpwnam($3)) != NULL)
      {
        if (usr->pw_uid != 0)
	{
          globalconfig.shed_uid = usr->pw_uid;
	  if (!silent)
	    fprintf(stderr, "%s:%d: suggest replacing 'shed_user = \"%s\"' line with 'shed_uid = %d'\n",
	      config, line, $3, usr->pw_uid);
	}
	else
	{
	  fprintf(stderr, "%s:%d: I refuse to run as %s (uid 0!) Aborting.\n", config, line, $3);
	  graceful_exit(1);
	}
      }
      else
      {
        if (!silent)
          fprintf(stderr, "%s:%d: no such user '%s'\n", config, line, $3);
      }
      break;

    case TYPE_PATH_CHROOT:
      if (globalconfig.chroot) free(globalconfig.chroot);
      globalconfig.chroot = strdup ($3);
      break;

    case TYPE_PATH_GAME:
      if (myconfig[ncnf]->game_path) free(myconfig[ncnf]->game_path);
      myconfig[ncnf]->game_path = strdup ($3);
      break;

    case TYPE_NAME_GAME:
      if (myconfig[ncnf]->game_name) free (myconfig[ncnf]->game_name);
      myconfig[ncnf]->game_name = strdup($3);
      break;

    case TYPE_GAME_SHORT_NAME:
      if (myconfig[ncnf]->shortname) free (myconfig[ncnf]->shortname);
      myconfig[ncnf]->shortname = strdup($3);
      break;

    case TYPE_PATH_CHDIR:
      if (myconfig[ncnf]->chdir) free(myconfig[ncnf]->chdir);
      myconfig[ncnf]->chdir = strdup ($3);
      break;

    case TYPE_PATH_MKDIR:
      if (myconfig[ncnf]->mkdir) free(myconfig[ncnf]->mkdir);
      myconfig[ncnf]->mkdir = strdup ($3);
      break;

    case TYPE_PATH_DGLDIR:
      if (globalconfig.dglroot) free(globalconfig.dglroot);
      globalconfig.dglroot = strdup($3);
      break;

    case TYPE_PATH_BANNER:
      if (globalconfig.banner) free(globalconfig.banner);
      globalconfig.banner = strdup($3);
      break;

    case TYPE_PATH_CANNED:
      if (myconfig[ncnf]->rcfile) free(myconfig[ncnf]->rcfile);
      myconfig[ncnf]->rcfile = strdup($3);
      break;

    case TYPE_PATH_SPOOL:
      if (myconfig[ncnf]->spool) free (myconfig[ncnf]->spool);
      myconfig[ncnf]->spool = strdup($3);
      break;

    case TYPE_PATH_LOCKFILE:
      if (globalconfig.lockfile) free (globalconfig.lockfile);
      globalconfig.lockfile = strdup($3);
      break;

    case TYPE_PATH_PASSWD:
      if (globalconfig.passwd) free(globalconfig.passwd);
      globalconfig.passwd = strdup($3);
      break;

    case TYPE_PATH_SAVEFILEFMT:
      if (myconfig[ncnf]->savefilefmt) free(myconfig[ncnf]->savefilefmt);
      myconfig[ncnf]->savefilefmt = strdup($3);
      break;

  case TYPE_RC_FMT:
      if (myconfig[ncnf]->rc_fmt) free(myconfig[ncnf]->rc_fmt);
      myconfig[ncnf]->rc_fmt = strdup($3);
      break;

    case TYPE_PATH_INPROGRESS:
      if (myconfig[ncnf]->inprogressdir) free(myconfig[ncnf]->inprogressdir);
      myconfig[ncnf]->inprogressdir = strdup($3);
      break;

  case TYPE_GAME_ARGS:
      {
	  char **tmpargs;
	  if (myconfig[ncnf]->bin_args) {
	      myconfig[ncnf]->num_args++;
	      tmpargs = calloc((myconfig[ncnf]->num_args+1), sizeof(char *));
	      memcpy(tmpargs, myconfig[ncnf]->bin_args, (myconfig[ncnf]->num_args * sizeof(char *)));
	      free(myconfig[ncnf]->bin_args);
	      myconfig[ncnf]->bin_args = tmpargs;
	  } else {
	      myconfig[ncnf]->num_args = 1;
	      myconfig[ncnf]->bin_args = calloc(2, sizeof(char *));
	  }
	  myconfig[ncnf]->bin_args[(myconfig[ncnf]->num_args)-1] = strdup($3);
	  myconfig[ncnf]->bin_args[(myconfig[ncnf]->num_args)] = 0;
      }
      break;

    default:
      fprintf(stderr, "%s:%d: token %s does not take a string, bailing out\n",
        config, line, lookup_token($1));
      exit(1);

  }

  free($3);
}
	| KeyType '=' TYPE_MALSTRING {}
	| KeyType '=' TYPE_NUMBER {

  if (ncnf < 0 || ncnf >= DIFF_GAMES) {
      fprintf(stderr, "%s:%d: illegal game number, bailing out\n",
        config, line);
      exit(1);
  }

  if (!myconfig)
  {
    int tmp;
    myconfig = calloc(DIFF_GAMES, sizeof(myconfig[0]));
    for (tmp = 0; tmp < DIFF_GAMES; tmp++) {
	myconfig[tmp] = calloc(1, sizeof(struct dg_config));
    }
    globalconfig.shed_uid = (uid_t)-1;
    globalconfig.shed_gid = (gid_t)-1;
  }

  switch ($1)
  {
    case TYPE_SUID:
      if (!silent && globalconfig.shed_uid != (uid_t)-1 && globalconfig.shed_uid != $3)
        fprintf(stderr, "%s:%d: 'shed_uid = %lu' entry overrides old setting %d\n",
	  config, line, $3, globalconfig.shed_uid);

      /* Naive user protection - do not allow running as user root */
      if ($3 == 0)
      {
        fprintf(stderr, "%s:%d: I refuse to run as uid 0 (root)! Aborting.\n", config, line);
        graceful_exit(1);
      }
     
      globalconfig.shed_uid = $3;
      break;

    case TYPE_SGID:
      if (!silent && globalconfig.shed_gid != (gid_t)-1 && globalconfig.shed_gid != $3)
        fprintf(stderr, "%s:%d: 'shed_gid = %lu' entry overrides old setting %d\n",
	  config, line, $3, globalconfig.shed_gid);
      
      globalconfig.shed_gid = $3;
      break;

  case TYPE_GAMENUM:
      if (($3 < 0) || ($3 >= DIFF_GAMES)) {
	  fprintf(stderr, "%s:%d: illegal game number, bailing out\n",
		  config, line);
	  exit(1);
      }
      ncnf = $3;
      if (ncnf > num_games)
	  num_games = ncnf;
      break;

    case TYPE_MAX:
      globalconfig.max = $3;
      break;

  case TYPE_MAXNICKLEN:
      globalconfig.max_newnick_len = $3;
      break;

    default:
      fprintf(stderr, "%s:%d: token %s does not take a number, bailing out\n",
        config, line, lookup_token($1)); 
      exit(1);
  }
};

KeyType : TYPE_SUSER	{ $$ = TYPE_SUSER; }
	| TYPE_SGROUP	{ $$ = TYPE_SGROUP; }
	| TYPE_SUID	{ $$ = TYPE_SUID; }
	| TYPE_SGID	{ $$ = TYPE_SGID; }
	| TYPE_MAX	{ $$ = TYPE_MAX; }
	| TYPE_MAXNICKLEN	{ $$ = TYPE_MAXNICKLEN; }
	| TYPE_GAMENUM	{ $$ = TYPE_GAMENUM; }
	| TYPE_PATH_CHROOT	{ $$ = TYPE_PATH_CHROOT; }
	| TYPE_PATH_GAME	{ $$ = TYPE_PATH_GAME; }
        | TYPE_NAME_GAME        { $$ = TYPE_NAME_GAME; }
	| TYPE_GAME_SHORT_NAME	{ $$ = TYPE_GAME_SHORT_NAME; }
	| TYPE_PATH_CHDIR	{ $$ = TYPE_PATH_CHDIR; }
	| TYPE_PATH_MKDIR	{ $$ = TYPE_PATH_MKDIR; }
	| TYPE_PATH_DGLDIR	{ $$ = TYPE_PATH_DGLDIR; }
	| TYPE_PATH_SPOOL	{ $$ = TYPE_PATH_SPOOL; }
	| TYPE_PATH_BANNER	{ $$ = TYPE_PATH_BANNER; }
	| TYPE_PATH_CANNED	{ $$ = TYPE_PATH_CANNED; }
	| TYPE_PATH_PASSWD	{ $$ = TYPE_PATH_PASSWD; }
	| TYPE_PATH_LOCKFILE	{ $$ = TYPE_PATH_LOCKFILE; }
	| TYPE_PATH_SAVEFILEFMT	{ $$ = TYPE_PATH_SAVEFILEFMT; }
	| TYPE_PATH_INPROGRESS	{ $$ = TYPE_PATH_INPROGRESS; }
	| TYPE_GAME_ARGS	{ $$ = TYPE_GAME_ARGS; }
	| TYPE_RC_FMT		{ $$ = TYPE_RC_FMT; }
	;

%%

const char* lookup_token (int t)
{
  switch (t)
  {
    case TYPE_SUSER: return "shed_user";
    case TYPE_SGROUP: return "shed_group";
    case TYPE_SUID: return "shed_uid";
    case TYPE_SGID: return "shed_gid";
    case TYPE_MAX: return "maxusers";
    case TYPE_MAXNICKLEN: return "maxnicklen";
    case TYPE_GAMENUM: return "game_num";
    case TYPE_PATH_CHROOT: return "chroot_path";
    case TYPE_PATH_CHDIR: return "chdir";
    case TYPE_PATH_MKDIR: return "mkdir";
    case TYPE_PATH_GAME: return "game_path";
    case TYPE_NAME_GAME: return "game_name";
    case TYPE_GAME_SHORT_NAME: return "short_name";
    case TYPE_PATH_DGLDIR: return "dglroot";
    case TYPE_PATH_SPOOL: return "spooldir";
    case TYPE_PATH_BANNER: return "banner";
    case TYPE_PATH_CANNED: return "rc_template";
    case TYPE_PATH_SAVEFILEFMT: return "savefilefmt";
    case TYPE_PATH_INPROGRESS: return "inprogressdir";
    case TYPE_GAME_ARGS: return "game_args";
    case TYPE_RC_FMT: return "rc_fmt";
    default: abort();
  }
}

void yyerror(char const* s)
{
  if (!silent)
    fprintf(stderr, "%s:%d:%d: couldn't parse \"%s\": %s\n", config, line, col, yytext, s);
}
