/* Lexical analyzer for dgamelaunch's configuration file. */

%option nounput
%option noyywrap

%{

#include <stdio.h>
#include <string.h>

#include "y.tab.h"
#include "dgamelaunch.h"

unsigned int line = 1, col = 0;
unsigned int comment_begin_line, comment_begin_col;

static void ccomment(void);

#define YY_USER_ACTION col += yyleng;

%}

VALUE		\".*\"
MALSTRING	\"[^\"\n]*\n
WHITE		[\t ]*
COMMENT		^#.*
LONGCOMMENT	"/*"

%%             /* BEGIN RULES SECTION */

{VALUE}	{
  yytext[yyleng - 1] = '\0'; /* Kill the trailing quote */
  yytext++; /* Kill the leading quote */
  yylval.s = strdup(yytext);
  return TYPE_VALUE;
}

{MALSTRING} {
  /* yytext already contains a newline, no need for one here */
  fprintf(stderr, "%s: unterminated string constant at line %d, start column %d: %s\n", config, line, col - yyleng + 1, yytext);
}

{WHITE}		{ }
{COMMENT}	{ }
{LONGCOMMENT}	{
  comment_begin_line = line;
  comment_begin_col = col - 1;
  ccomment();
}

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

\n		{ line++; col = 0; }

. {
  fprintf(stderr, "%s: unrecognized token \"%s\" at line %d, column %d\n", config, yytext, line, col);
}

%%

/* Ripped from ircd-hybrid/src/ircd_lexer.l */
void ccomment(void)
{
  int c;

  while (1)
  {
    while ((c = input()) != '*' && c != EOF)
    {
      if (c == '\n')
      {
        col = 0;
	++line;
      }
      else
        ++col;
    }
      
    if (c == '*')
    {
      ++col;
      while ((c = input()) == '*') ++col;
      if (c == '/') { ++col; break; }
    }
    if (c == EOF)
    {
      fprintf(stderr, "%s: encountered end-of-file in comment starting on line %d, column %d\n", config, col, comment_begin_col);
      exit(1);

      break;
    }
  }
}
