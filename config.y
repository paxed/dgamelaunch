%{

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

static const char* lookup_token (int t);

%}

%union {
	char* s;
	int kt;
	unsigned long i;
}

%token TYPE_SUSER TYPE_SGROUP TYPE_SGID TYPE_SUID TYPE_MAX
%token TYPE_PATH_NETHACK TYPE_PATH_DGLDIR TYPE_PATH_SPOOL
%token TYPE_PATH_BANNER TYPE_PATH_CANNED TYPE_PATH_CHROOT
%token TYPE_PATH_PASSWD TYPE_PATH_LOCKFILE TYPE_MALSTRING
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
  
  if (!myconfig)
  {
    myconfig = calloc(1, sizeof(struct dg_config));
    myconfig->shed_uid = (uid_t)-1;
    myconfig->shed_gid = (gid_t)-1;
  }

  switch ($1)
  {
    case TYPE_SGROUP:
      if (myconfig->shed_gid != (gid_t)-1)
        break;
	
      myconfig->shed_group = strdup($3);
      if ((gr = getgrnam($3)) != NULL)
      {
	myconfig->shed_gid = gr->gr_gid;
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
      if (myconfig->shed_uid != (uid_t)-1)
        break;
	
      if (!strcmp($3, "root"))
      {
        fprintf(stderr, "%s:%d: I refuse to run as root! Aborting.\n", config, line);
	graceful_exit(1);
      }
      myconfig->shed_user = strdup($3);
      if ((usr = getpwnam($3)) != NULL)
      {
        if (usr->pw_uid != 0)
	{
          myconfig->shed_uid = usr->pw_uid;
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
      if (myconfig->chroot) free(myconfig->chroot);
      myconfig->chroot = strdup ($3);
      break;

    case TYPE_PATH_NETHACK:
      if (myconfig->nethack) free(myconfig->nethack);
      myconfig->nethack = strdup ($3);
      break;

    case TYPE_PATH_DGLDIR:
      if (myconfig->dglroot) free(myconfig->dglroot);
      myconfig->dglroot = strdup ($3);
      break;

    case TYPE_PATH_BANNER:
      if (myconfig->banner) free(myconfig->banner);
      myconfig->banner = strdup($3);
      break;

    case TYPE_PATH_CANNED:
      if (myconfig->rcfile) free(myconfig->rcfile);
      myconfig->rcfile = strdup($3);
      break;

    case TYPE_PATH_SPOOL:
      if (myconfig->spool) free (myconfig->spool);
      myconfig->spool = strdup($3);
      break;

    case TYPE_PATH_LOCKFILE:
      if (myconfig->lockfile) free (myconfig->lockfile);
      myconfig->lockfile = strdup($3);
      break;

    case TYPE_PATH_PASSWD:
      if (myconfig->passwd) free(myconfig->passwd);
      myconfig->passwd = strdup($3);
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
  if (!myconfig)
  {
    myconfig = calloc(1, sizeof(struct dg_config));
    myconfig->shed_uid = (uid_t)-1;
    myconfig->shed_gid = (gid_t)-1;
  }
	       
  switch ($1)
  {
    case TYPE_SUID:
      if (!silent && myconfig->shed_uid != (uid_t)-1 && myconfig->shed_uid != $3)
        fprintf(stderr, "%s:%d: 'shed_uid = %lu' entry overrides old setting %d\n",
	  config, line, $3, myconfig->shed_uid);

      /* Naive user protection - do not allow running as user root */
      if ($3 == 0)
      {
        fprintf(stderr, "%s:%d: I refuse to run as uid 0 (root)! Aborting.\n", config, line);
        graceful_exit(1);
      }
     
      myconfig->shed_uid = $3;
      break;

    case TYPE_SGID:
      if (!silent && myconfig->shed_gid != (gid_t)-1 && myconfig->shed_gid != $3)
        fprintf(stderr, "%s:%d: 'shed_gid = %lu' entry overrides old setting %d\n",
	  config, line, $3, myconfig->shed_gid);
      
      myconfig->shed_gid = $3;
      break;

    case TYPE_MAX:
      set_max = 1; /* XXX XXX */
      myconfig->max = $3;
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
	| TYPE_PATH_CHROOT	{ $$ = TYPE_PATH_CHROOT; }
	| TYPE_PATH_NETHACK	{ $$ = TYPE_PATH_NETHACK; }
	| TYPE_PATH_DGLDIR	{ $$ = TYPE_PATH_DGLDIR; }
	| TYPE_PATH_SPOOL	{ $$ = TYPE_PATH_SPOOL; }
	| TYPE_PATH_BANNER	{ $$ = TYPE_PATH_BANNER; }
	| TYPE_PATH_CANNED	{ $$ = TYPE_PATH_CANNED; }
	| TYPE_PATH_PASSWD	{ $$ = TYPE_PATH_PASSWD; }
	| TYPE_PATH_LOCKFILE	{ $$ = TYPE_PATH_LOCKFILE; }
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
    case TYPE_PATH_CHROOT: return "chroot_path";
    case TYPE_PATH_NETHACK: return "nethack";
    case TYPE_PATH_DGLDIR: return "dglroot";
    case TYPE_PATH_SPOOL: return "spooldir";
    case TYPE_PATH_BANNER: return "banner";
    case TYPE_PATH_CANNED: return "rc_template";
    default: abort();
  }
}

void yyerror(char const* s)
{
  if (!silent)
    fprintf(stderr, "%s:%d:%d: couldn't parse \"%s\": %s\n", config, line, col, yytext, s);
}
