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
VALUE		\".*\"
MALSTRING	\"[^\"\n]*\n
WHITE		[\t ]*
COMMENT		^#.*

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
  /* yytext already contains a newline, no need for one here */
  fprintf(stderr, "%s:%d:%d: unterminated string constant: %s\n", config, line, col - yyleng + 1, yytext);
  return TYPE_MALSTRING; 
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
  fprintf(stderr, "%s:%d:%d unrecognized token \"%s\"\n", config, line, col, yytext);
}

%%
