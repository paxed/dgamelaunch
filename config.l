/* Lexical analyzer for dgamelaunch's configuration file. */

%{

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "y.tab.h"
#include "dgamelaunch.h"

unsigned int line = 1, col = 0;

#define YY_USER_ACTION col += yyleng;

%}

NEGNUMBER	-[0-9]+
NUMBER		[0-9]+
VALUE		\"[^"]*\"
MALSTRING	\"[^\"\n]*\n
COMMENT		^[\t ]*#.*
WHITE		[\t ]*

%%

{NEGNUMBER} {
  fprintf(stderr,"%s:%d: negative value not accepted! Fix it now!\n",
      config, line);
  graceful_exit(1);
}

{NUMBER} { 
  yylval.i = strtoul(yytext, NULL, 10);
  return TYPE_NUMBER;
}

{VALUE}	{
  yytext[yyleng - 1] = '\0'; /* Kill the trailing quote */
  yylval.s = strdup(yytext + 1); /* kill leading quote */
  return TYPE_VALUE;
}

{MALSTRING} {
  yytext[yyleng - 1] = '\0'; /* remove trailing newline */
  fprintf(stderr, "%s:%d:%d: unterminated string constant: %s\n", config, line, col - yyleng + 1, yytext);
  return TYPE_MALSTRING; 
}

{WHITE}		{ }
{COMMENT}	{ }

"="		{ return '='; }
","		{ return ','; }
"["		{ return '['; }
"]"		{ return ']'; }
"{"		{ return '{'; }
"}"		{ return '}'; }
"shed_user"	{ return TYPE_SUSER; }
"shed_group"	{ return TYPE_SGROUP; }
"shed_uid"	{ return TYPE_SUID; }
"shed_gid"	{ return TYPE_SGID; }
"maxusers"	{ return TYPE_MAX; }
"maxnicklen"	{ return TYPE_MAXNICKLEN; }
"allow_new_nicks"	{ return TYPE_ALLOW_REGISTRATION; }


"chroot_path"	{ return TYPE_PATH_CHROOT; }
"game_name"     { return TYPE_NAME_GAME; }
"short_name"	{ return TYPE_GAME_SHORT_NAME; }
"game_path"	{ return TYPE_PATH_GAME; }
"dglroot"	{ return TYPE_PATH_DGLDIR; }
"spooldir"	{ return TYPE_PATH_SPOOL; }
"banner"	{ return TYPE_PATH_BANNER; }
"rc_template"	{ return TYPE_PATH_CANNED; }
"passwd"	{ return TYPE_PATH_PASSWD; }
"lockfile"	{ return TYPE_PATH_LOCKFILE; }
"inprogressdir" { return TYPE_PATH_INPROGRESS; }
"game_args"	{ return TYPE_GAME_ARGS; }
"rc_fmt"	{ return TYPE_RC_FMT; }
commands	{ return TYPE_CMDQUEUE; }
yes		{ yylval.i = 1; return TYPE_BOOL; }
no		{ yylval.i = 0; return TYPE_BOOL; }
dglstart	{ yylval.i = DGLTIME_DGLSTART; return TYPE_CMDQUEUENAME; }
login		{ yylval.i = DGLTIME_LOGIN;    return TYPE_CMDQUEUENAME; }
register	{ yylval.i = DGLTIME_REGISTER; return TYPE_CMDQUEUENAME; }
game_start	{ yylval.i = DGLTIME_GAMESTART; return TYPE_CMDQUEUENAME; }
mkdir		{ yylval.i = DGLCMD_MKDIR;  return TYPE_DGLCMD1; }
chdir		{ yylval.i = DGLCMD_CHDIR;  return TYPE_DGLCMD1; }
cp		{ yylval.i = DGLCMD_CP;     return TYPE_DGLCMD2; }
ifnxcp		{ yylval.i = DGLCMD_IF_NX_CP; return TYPE_DGLCMD2; }
unlink		{ yylval.i = DGLCMD_UNLINK; return TYPE_DGLCMD1; }
setenv		{ yylval.i = DGLCMD_SETENV; return TYPE_DGLCMD2; }
exec		{ yylval.i = DGLCMD_EXEC;   return TYPE_DGLCMD2; }
DEFINE		{ return TYPE_DEFINE_GAME; }


\n		{ line++; col = 0; }

[0-9a-zA-Z_]+ {
  fprintf(stderr, "%s:%d:%d unrecognized token \"%s\"\n", config, line, col, yytext);
  graceful_exit(1);
}

%%
