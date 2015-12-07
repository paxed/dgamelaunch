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
  graceful_exit(13);
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
  fprintf(stderr, "%s:%d:%d: unterminated string constant: %s\n", config, line, (int)(col - yyleng + 1), yytext);
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
"("		{ return '('; }
")"		{ return ')'; }
"shed_user"	{ return TYPE_SUSER; }
"shed_group"	{ return TYPE_SGROUP; }
"shed_uid"	{ return TYPE_SUID; }
"shed_gid"	{ return TYPE_SGID; }
"maxusers"	{ return TYPE_MAX; }
"maxnicklen"	{ return TYPE_MAXNICKLEN; }
"allow_new_nicks"	{ return TYPE_ALLOW_REGISTRATION; }
"menu_max_idle_time"	{ return TYPE_MENU_MAX_IDLE_TIME; }


menu		{ return TYPE_DEFINE_MENU; }
bannerfile	{ return TYPE_BANNER_FILE; }
cursor		{ return TYPE_CURSOR; }
"chroot_path"	{ return TYPE_PATH_CHROOT; }
"game_name"     { return TYPE_NAME_GAME; }
"short_name"	{ return TYPE_GAME_SHORT_NAME; }
"game_id"	{ return TYPE_GAME_ID; }
"game_path"	{ return TYPE_PATH_GAME; }
"dglroot"	{ return TYPE_PATH_DGLDIR; }
"spooldir"	{ return TYPE_PATH_SPOOL; }
"banner"	{ return TYPE_PATH_BANNER; }
"rc_template"	{ return TYPE_PATH_CANNED; }
"passwd"	{ return TYPE_PATH_PASSWD; }
"lockfile"	{ return TYPE_PATH_LOCKFILE; }
"inprogressdir" { return TYPE_PATH_INPROGRESS; }
"game_args"	{ return TYPE_GAME_ARGS; }
extra_info_file { return TYPE_EXTRA_INFO_FILE; }
"max_idle_time"	{ return TYPE_MAX_IDLE_TIME; }
"rc_fmt"	{ return TYPE_RC_FMT; }
"ttyrecdir"	{ return TYPE_PATH_TTYREC; }
bannervars	{ return TYPE_BANNERVARS; }
timeformat	{ return TYPE_TIMEFORMAT; }
sortmode	{ return TYPE_WATCH_SORTMODE; }
watch_columns	{ return TYPE_WATCH_COLUMNS; }
commands	{ return TYPE_CMDQUEUE; }
postcommands	{ return TYPE_POSTCMDQUEUE; }
encoding	{ return TYPE_ENCODING; }
locale		{ return TYPE_LOCALE; }
default_term	{ return TYPE_DEFTERM; }
utf8esc		{ return TYPE_UTF8ESC; }
flowcontrol	{ return TYPE_FLOWCTRL; }
filemode	{ return TYPE_FILEMODE; }
yes		{ yylval.i = 1; return TYPE_BOOL; }
no		{ yylval.i = 0; return TYPE_BOOL; }
dglstart	{ yylval.i = DGLTIME_DGLSTART; return TYPE_CMDQUEUENAME; }
login		{ yylval.i = DGLTIME_LOGIN;    return TYPE_CMDQUEUENAME; }
register	{ yylval.i = DGLTIME_REGISTER; return TYPE_CMDQUEUENAME; }
gamestart	{ yylval.i = DGLTIME_GAMESTART; return TYPE_CMDQUEUENAME; }
gameend		{ yylval.i = DGLTIME_GAMEEND; return TYPE_CMDQUEUENAME; }
mkdir		{ yylval.i = DGLCMD_MKDIR;  return TYPE_DGLCMD1; }
chdir		{ yylval.i = DGLCMD_CHDIR;  return TYPE_DGLCMD1; }
cp		{ yylval.i = DGLCMD_CP;     return TYPE_DGLCMD2; }
ifnxcp		{ yylval.i = DGLCMD_IF_NX_CP; return TYPE_DGLCMD2; }
unlink		{ yylval.i = DGLCMD_UNLINK; return TYPE_DGLCMD1; }
setenv		{ yylval.i = DGLCMD_SETENV; return TYPE_DGLCMD2; }
exec		{ yylval.i = DGLCMD_EXEC;   return TYPE_DGLCMD2; }
chpasswd	{ yylval.i = DGLCMD_CHPASSWD;   return TYPE_DGLCMD0; }
chmail		{ yylval.i = DGLCMD_CHMAIL;     return TYPE_DGLCMD0; }
watch_menu	{ yylval.i = DGLCMD_WATCH_MENU; return TYPE_DGLCMD0; }
ask_login	{ yylval.i = DGLCMD_LOGIN;      return TYPE_DGLCMD0; }
ask_register	{ yylval.i = DGLCMD_REGISTER;   return TYPE_DGLCMD0; }
quit		{ yylval.i = DGLCMD_QUIT;       return TYPE_DGLCMD0; }
play_game	{ yylval.i = DGLCMD_PLAYGAME;    return TYPE_DGLCMD1; }
play_if_exist	{ yylval.i = DGLCMD_PLAY_IF_EXIST;    return TYPE_DGLCMD2; }
submenu		{ yylval.i = DGLCMD_SUBMENU;     return TYPE_DGLCMD1; }
return		{ yylval.i = DGLCMD_RETURN;      return TYPE_DGLCMD0; }
rawprint	{ yylval.i = DGLCMD_RAWPRINT;    return TYPE_DGLCMD1; }
DEFINE		{ return TYPE_DEFINE_GAME; }


\n		{ line++; col = 0; }

[0-9a-zA-Z_]+ {
  fprintf(stderr, "%s:%d:%d unrecognized token \"%s\"\n", config, line, col, yytext);
  graceful_exit(12);
}

%%
