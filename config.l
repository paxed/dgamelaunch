/* Lexical analyzer for dgamelaunch's configuration file. */

%{

#include <stdio.h>
#include <string.h>

#include "y.tab.h"
#include "dgamelaunch.h"

unsigned int line = 1, col = 0;
unsigned int comment_begin_line, comment_begin_col;

#define YY_USER_ACTION col += yyleng;

%}

NUMBER		[0-9]+
VALUE		\".*\"
MALSTRING	\"[^\"\n]*\n
WHITE		[\t ]*
COMMENT		^#.*

%%

{NUMBER} { yylval.i = atoi(yytext); return TYPE_NUMBER; }
{VALUE}	{
  yytext[yyleng - 1] = '\0'; /* Kill the trailing quote */
  yylval.s = strdup(yytext + 1); /* kill leading quote */
  return TYPE_VALUE;
}

{MALSTRING} {
  /* yytext already contains a newline, no need for one here */
  fprintf(stderr, "%s: unterminated string constant at line %d, start column %d: %s\n", config, line, col - yyleng + 1, yytext);
}

{WHITE}		{ }
{COMMENT}	{ }

"="		{ return '='; }
"shed_user"	{ return TYPE_SUSER; }
"shed_group"	{ return TYPE_SGROUP; }
"shed_uid"	{ return TYPE_SUID; }
"shed_gid"	{ return TYPE_SGID; }
"maxusers"	{ return TYPE_MAX; }

"chroot_path"	{ return TYPE_PATH_CHROOT; }
"nethack"	{ return TYPE_PATH_NETHACK; }
"dglroot"	{ return TYPE_PATH_DGLDIR; }
"spooldir"	{ return TYPE_PATH_SPOOL; }
"banner"	{ return TYPE_PATH_BANNER; }
"rc_template"	{ return TYPE_PATH_CANNED; }
"passwd"	{ return TYPE_PATH_PASSWD; }
"lockfile"	{ return TYPE_PATH_LOCKFILE; }

\n		{ line++; col = 0; }

. {
  fprintf(stderr, "%s: unrecognized token \"%s\" at line %d, column %d\n", config, yytext, line, col);
}

%%
