%{

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
%token <s> TYPE_VALUE
%token <i> TYPE_NUMBER
%type  <kt> KeyType

%%

Configuration: KeyPairs
	| { fprintf(stderr, "%s: no settings, proceeding with defaults\n", config); }
	;

KeyPairs: KeyPairs KeyPair
	| KeyPair
	;

KeyPair: KeyType '=' TYPE_VALUE {
  struct group* gr;
  struct passwd* usr;
  
  if (!myconfig)
    myconfig = calloc(1, sizeof(struct dg_config));

  switch ($1)
  {
    case TYPE_SGROUP:
      if ((gr = getgrnam($3)) != NULL)
        myconfig->shed_gid = gr->gr_gid;
      else
        fprintf(stderr, "%s: no such group '%s'\n", config, $3);
      
      break;
    case TYPE_SUSER:
      if ((usr = getpwnam($3)) != NULL)
        myconfig->shed_uid = usr->pw_uid;
      else
        fprintf(stderr, "%s: no such group '%s'\n", config, $3);
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

    default:
      fprintf(stderr, "%s: token %s does not take a string, bailing out\n",
        config, lookup_token($1));
      exit(1);
      
  }

  free($3);
}
	| KeyType '=' TYPE_NUMBER {
  switch ($1)
  {
    case TYPE_SUID:
      if (getpwuid($3) != NULL)
        myconfig->shed_uid = $3;
      else
        fprintf(stderr, "%s: no such uid %lu\n", config, $3);
	
      break;

    case TYPE_SGID:
      if (getgrgid($3) != NULL)
        myconfig->shed_gid = $3;
      else
        fprintf(stderr, "%s: no such gid %lu\n", config, $3);
      break;

    case TYPE_MAX:
      myconfig->max = $3;
      break;

    default:
      fprintf(stderr, "%s: token %s does not take a number, bailing out\n",
        config, lookup_token($1)); 
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
  fprintf(stderr, "%s: couldn't parse \"%s\" at line %d, column %d: %s\n", config, yytext, line, col, s);
}
