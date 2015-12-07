/*
 |	ee (easy editor)
 |
 |	An easy to use, simple screen oriented editor.
 |
 |	written by Hugh Mahon
 |
 |	THIS MATERIAL IS PROVIDED "AS IS".  THERE ARE
 |	NO WARRANTIES OF ANY KIND WITH REGARD TO THIS
 |	MATERIAL, INCLUDING, BUT NOT LIMITED TO, THE
 |	IMPLIED WARRANTIES OF MERCHANTABILITY AND
 |	FITNESS FOR A PARTICULAR PURPOSE.  Neither
 |	Hewlett-Packard nor Hugh Mahon shall be liable
 |	for errors contained herein, nor for
 |	incidental or consequential damages in
 |	connection with the furnishing, performance or
 |	use of this material.  Neither Hewlett-Packard
 |	nor Hugh Mahon assumes any responsibility for
 |	the use or reliability of this software or
 |	documentation.  This software and
 |	documentation is totally UNSUPPORTED.  There
 |	is no support contract available.  Hewlett-
 |	Packard has done NO Quality Assurance on ANY
 |	of the program or documentation.  You may find
 |	the quality of the materials inferior to
 |	supported materials.
 |
 |	This software is not a product of Hewlett-Packard, Co., or any 
 |	other company.  No support is implied or offered with this software.
 |	You've got the source, and you're on your own.
 |
 |	This software may be distributed under the terms of Larry Wall's 
 |	Artistic license, a copy of which is included in this distribution. 
 |
 |	This notice must be included with this software and any derivatives.
 |
 |	This editor was purposely developed to be simple, both in 
 |	interface and implementation.  This editor was developed to 
 |	address a specific audience: the user who is new to computers 
 |	(especially UNIX).
 |	
 |	ee is not aimed at technical users; for that reason more 
 |	complex features were intentionally left out.  In addition, 
 |	ee is intended to be compiled by people with little computer 
 |	experience, which means that it needs to be small, relatively 
 |	simple in implementation, and portable.
 |
 |	This software and documentation contains
 |	proprietary information which is protected by
 |	copyright.  All rights are reserved.
 |
 |	$Header: /var/cvs/dgamelaunch/ee.c,v 1.10 2004/01/26 16:54:02 joshk Exp $
 |
 */

#include <wchar.h>
wchar_t *ee_copyright_message = 
L"Copyright (c) 1986, 1990, 1991, 1992, 1993, 1994, 1995, 1996 Hugh Mahon ";

wchar_t *ee_long_notice[] = {
	L"This software and documentation contains", 
	L"proprietary information which is protected by", 
	L"copyright.  All rights are reserved."
	};

wchar_t *version = L"@(#) ee, version 1.4.1  $Revision: 1.10 $";

#include <locale.h>
#include <curses.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>

#include <sys/wait.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>

#ifndef SIGCHLD
#define SIGCHLD SIGCLD
#endif

#define MAX_FILE 1048576

#define TAB 9
#define max(a, b)	(a > b ? a : b)
#define min(a, b)	(a < b ? a : b)

/*
 |	defines for type of data to show in info window
 */

#define CONTROL_KEYS 1
#define COMMANDS     2

struct text {
	wchar_t *line;		/* line of characters		*/
	int line_number;	/* line number			*/
	int line_length;	/* actual number of characters in the line */
	int max_length;	/* maximum number of characters the line handles */
	struct text *next_line;		/* next line of text		*/
	struct text *prev_line;		/* previous line of text	*/
	};

struct text *first_line;	/* first line of current buffer		*/
struct text *dlt_line;		/* structure for info on deleted line	*/
struct text *curr_line;		/* current line cursor is on		*/
struct text *tmp_line;		/* temporary line pointer		*/
struct text *srch_line;		/* temporary pointer for search routine */

int d_wrd_len;			/* length of deleted word		*/
int position;			/* offset in bytes from begin of line	*/
int scr_pos;			/* horizontal position			*/
int scr_vert;			/* vertical position on screen		*/
int scr_horz;			/* horizontal position on screen	*/
int tmp_vert, tmp_horz;
int input_file;			/* indicate to read input file		*/
int recv_file;			/* indicate reading a file		*/
int edit;			/* continue executing while true	*/
int gold;			/* 'gold' function key pressed		*/
int fildes;			/* file descriptor			*/
int case_sen;			/* case sensitive search flag		*/
int last_line;			/* last line for text display		*/
int last_col;			/* last column for text display		*/
int horiz_offset = 0;		/* offset from left edge of text	*/
int clear_com_win;		/* flag to indicate com_win needs clearing */
int text_changes = FALSE;	/* indicate changes have been made to text */
int get_fd;			/* file descriptor for reading a file	*/
int info_window = TRUE;		/* flag to indicate if help window visible */
int info_type = CONTROL_KEYS;	/* flag to indicate type of info to display */
int expand_tabs = TRUE;		/* flag for expanding tabs		*/
int right_margin = 0;		/* the right margin 			*/
int observ_margins = TRUE;	/* flag for whether margins are observed */
int out_pipe;			/* flag that info is piped out		*/
int in_pipe;			/* flag that info is piped in		*/
int formatted = FALSE;		/* flag indicating paragraph formatted	*/
int nohighlight = FALSE;	/* turns off highlighting		*/
int eightbit = TRUE;		/* eight bit character flag		*/
int local_LINES = 0;		/* copy of LINES, to detect when win resizes */
int local_COLS = 0;		/* copy of COLS, to detect when win resizes  */
int curses_initialized = FALSE;	/* flag indicating if curses has been started*/
int emacs_keys_mode = TRUE;	/* mode for if emacs key binings are used    */

wchar_t *point;		/* points to current position in line	*/
wchar_t *srch_str;	/* pointer for search string		*/
wchar_t *u_srch_str;	/* pointer to non-case sensitive search	*/
wchar_t *srch_1;		/* pointer to start of suspect string	*/
wchar_t *srch_2;		/* pointer to next character of string	*/
wchar_t *srch_3;
char *in_file_name = NULL;	/* name of input file		*/
char *tmp_file;	/* temporary file name			*/
wchar_t *d_char;		/* deleted character			*/
wchar_t *d_word;		/* deleted word				*/
wchar_t *d_line;		/* deleted line				*/
wchar_t *start_at_line = NULL;	/* move to this line at start of session*/
int in;				/* input character			*/

FILE *temp_fp;			/* temporary file pointer		*/

wchar_t *table[] = { 
	L"^@", L"^A", L"^B", L"^C", L"^D", L"^E", L"^F", L"^G", L"^H", L"\t", L"^J", 
	L"^K", L"^L", L"^M", L"^N", L"^O", L"^P", L"^Q", L"^R", L"^S", L"^T", L"^U", 
	L"^V", L"^W", L"^X", L"^Y", L"^Z", L"^[", L"^\\", L"^]", L"^^", L"^_"
	};

WINDOW *com_win;
WINDOW *text_win;
WINDOW *help_win;
WINDOW *info_win;

#if defined(__STDC__) || defined(__cplusplus)
#define P_(s) s
#else
#define P_(s) ()
#endif


/*
 |	The following structure allows menu items to be flexibly declared.
 |	The first item is the string describing the selection, the second 
 |	is the address of the procedure to call when the item is selected,
 |	and the third is the argument for the procedure.
 |
 |	For those systems with i18n, the string should be accompanied by a
 |	catalog number.  The 'int *' should be replaced with 'void *' on 
 |	systems with that type.
 |
 |	The first menu item will be the title of the menu, with NULL 
 |	parameters for the procedure and argument, followed by the menu items.
 |
 |	If the procedure value is NULL, the menu item is displayed, but no 
 |	procedure is called when the item is selected.  The number of the 
 |	item will be returned.  If the third (argument) parameter is -1, no 
 |	argument is given to the procedure when it is called.
 */

struct menu_entries
{
	wchar_t *item_string;
	int (*procedure)P_((struct menu_entries *));
	struct menu_entries *ptr_argument;
	int (*iprocedure)P_((int));
	void (*nprocedure)P_((void));
	int argument;
};

int main(int argc, char *argv[]);
wchar_t *resiz_line(int factor, struct text *rline, int rpos);
void insert(int character);
void delete(int disp);
void scanline(wchar_t *pos);
int tabshift(int temp_int);
int out_char(WINDOW *window, int character, int column);
int len_char(wchar_t character, int column);
void draw_line(int vertical, int horiz, wchar_t *ptr, int t_pos, int length);
void insert_line(int disp);
struct text *txtalloc(void);
wchar_t *next_word(wchar_t *string);
void prev_word(void);
void control(void);
void emacs_control(void);
void bottom(void);
void top(void);
void nextline(void);
void prevline(void);
void left(int disp);
void right(int disp);
void find_pos(void);
void up(void);
void down(void);
void function_key(void);
void command_prompt(void);
void command(wchar_t *cmd_str1);
int scan(wchar_t *line, int offset, int column);
wchar_t *get_string(wchar_t *prompt, int advance);
int compare(wchar_t *string1, wchar_t *string2, int sensitive);
void goto_line(wchar_t *cmd_str);
void midscreen(int line, wchar_t *pnt);
void check_fp(void);
void get_file(char *file_name);
void get_line(int length, char *in_string, int *append);
void draw_screen(void);
void ee_finish(void);
int quit(int noverify);
void edit_abort(int arg);
void delete_text(void);
int write_file(char *file_name, int fd);
int search(int display_message);
void search_prompt(void);
void del_char(void);
void undel_char(void);
void del_word(void);
void undel_word(void);
void del_line(void);
void undel_line(void);
void adv_word(void);
void move_rel(char direction, int lines);
void eol(void);
void bol(void);
void adv_line(void);
void set_up_term(void);
void resize_check(void);
int menu_op(struct menu_entries menu_list[]);
void paint_menu(struct menu_entries menu_list[], int max_width, int max_height, int list_size, int top_offset, WINDOW *menu_win, int off_start, int vert_size);
void help(void);
void paint_info_win(void);
void no_info_window(void);
void create_info_window(void);
int file_op(int arg);
void leave_op(void);
void redraw(void);
int Blank_Line(struct text *test_line);
void echo_string(wchar_t *string);
int first_word_len(struct text *test_line);
wchar_t *is_in_string(wchar_t *string, wchar_t *substring);
int unique_test(wchar_t *string, wchar_t *list[]);
void strings_init(void);

#undef P_
/*
 |	allocate space here for the strings that will be in the menu
 */

struct menu_entries leave_menu[] = {
	{L"", NULL, NULL, NULL, NULL, -1}, 
	{L"", NULL, NULL, NULL, ee_finish, -1}, 
	{L"", NULL, NULL, quit, NULL, TRUE}, 
	{NULL, NULL, NULL, NULL, NULL, -1}
	};

#define READ_FILE 1
#define WRITE_FILE 2
#define SAVE_FILE 3

struct menu_entries search_menu[] = {
	{L"", NULL, NULL, NULL, NULL, 0}, 
	{L"", NULL, NULL, NULL, search_prompt, -1},
	{L"", NULL, NULL, search, NULL, TRUE},
	{NULL, NULL, NULL, NULL, NULL, -1}
	};

struct menu_entries main_menu[] = {
	{L"", NULL, NULL, NULL, NULL, -1}, 
	{L"", NULL, NULL, NULL, leave_op, -1}, 
	{L"", NULL, NULL, NULL, help, -1},
	{L"", NULL, NULL, file_op, NULL, SAVE_FILE},
	{L"", NULL, NULL, NULL, redraw, -1}, 
	{L"", menu_op, search_menu, NULL, NULL, -1}, 
	{NULL, NULL, NULL, NULL, NULL, -1}
	};

wchar_t *help_text[23];
wchar_t *control_keys[5];

wchar_t *emacs_help_text[22];
wchar_t *emacs_control_keys[5];

wchar_t *command_strings[5];
wchar_t *commands[32];

#define MENU_WARN 1

#define max_alpha_char 36

/*
 |	Declarations for strings for localization
 */

wchar_t *com_win_message;		/* to be shown in com_win if no info window */
char *no_file_string;
wchar_t *ascii_code_str;
wchar_t *command_str;
char *char_str;
char *unkn_cmd_str;
wchar_t *non_unique_cmd_msg;
char *line_num_str;
char *line_len_str;
char *current_file_str;
wchar_t *usage0;
wchar_t *usage1;
wchar_t *usage2;
wchar_t *usage3;
wchar_t *usage4;
char *file_is_dir_msg;
char *new_file_msg;
char *cant_open_msg;
wchar_t *open_file_msg;
char *file_read_fin_msg;
char *reading_file_msg;
char *read_only_msg;
char *file_read_lines_msg;
wchar_t *save_file_name_prompt;
char *file_not_saved_msg;
wchar_t *changes_made_prompt;
wchar_t *yes_char;
wchar_t *file_exists_prompt;
char *create_file_fail_msg;
char *writing_file_msg;
char *file_written_msg;
char *searching_msg;
char *str_not_found_msg;
wchar_t *search_prompt_str;
wchar_t *continue_msg;
wchar_t *menu_cancel_msg;
wchar_t *menu_size_err_msg;
wchar_t *press_any_key_msg;
wchar_t *ON;
wchar_t *OFF;
wchar_t *HELP;
wchar_t *SAVE;
wchar_t *READ;
wchar_t *LINE;
wchar_t *FILE_str;
wchar_t *CHARACTER;
wchar_t *REDRAW;
wchar_t *RESEQUENCE;
wchar_t *AUTHOR;
wchar_t *ee_VERSION;
wchar_t *CASE;
wchar_t *NOCASE;
wchar_t *EXPAND;
wchar_t *NOEXPAND;
wchar_t *Exit_string;
wchar_t *QUIT_string;
wchar_t *INFO;
wchar_t *NOINFO;
wchar_t *MARGINS;
wchar_t *NOMARGINS;
wchar_t *AUTOFORMAT;
wchar_t *NOAUTOFORMAT;
wchar_t *Echo;
wchar_t *PRINTCOMMAND;
wchar_t *RIGHTMARGIN;
wchar_t *HIGHLIGHT;
wchar_t *NOHIGHLIGHT;
wchar_t *EIGHTBIT;
wchar_t *NOEIGHTBIT;
wchar_t *EMACS_string;
wchar_t *NOEMACS_string;
wchar_t *cancel_string;
char *menu_too_lrg_msg;
wchar_t *more_above_str, *more_below_str;

#ifndef __STDC__
#ifndef HAS_STDLIB
extern char *malloc();
extern char *realloc();
extern char *getenv();
FILE *fopen();			/* declaration for open function	*/
#endif /* HAS_STDLIB */
#endif /* __STDC__ */

int
main(argc, argv)		/* beginning of main program		*/
int argc;
char *argv[];
{
	int counter;

	setlocale(LC_ALL, "");
	for (counter = 1; counter < 24; counter++)
	{
	  if (!(counter == SIGKILL || counter == SIGSTOP))
		signal(counter, SIG_IGN);
	}

	signal(SIGCHLD, SIG_DFL);
	signal(SIGSEGV, SIG_DFL);
	signal(SIGINT, edit_abort);
	d_char = malloc(3 * sizeof(wchar_t));	/* provide a buffer for multi-byte chars */
	d_word = malloc(150 * sizeof(wchar_t));
	*d_word = '\0';
	d_line = NULL;
	dlt_line = txtalloc();
	dlt_line->line = d_line;
	dlt_line->line_length = 0;
	curr_line = first_line = txtalloc();
	curr_line->line = point = malloc(10 * sizeof(wchar_t));
	curr_line->line_length = 1;
	curr_line->max_length = 10;
	curr_line->prev_line = NULL;
	curr_line->next_line = NULL;
	curr_line->line_number  = 1;
	srch_str = NULL;
	u_srch_str = NULL;
	position = 1;
	scr_pos =0;
	scr_vert = 0;
	scr_horz = 0;
	edit = TRUE;
	gold = case_sen = FALSE;
	strings_init();

	if (argc != 2)
	{
	  puts("need a filename! bailing out.");
	  return 1;
	}
	else
	{
	  tmp_file = strdup(argv[1]);
	  input_file = recv_file = TRUE;
	}
	
	set_up_term();
	if (right_margin == 0)
		right_margin = COLS - 1;
	
	if (!tmp_file)
	{
		wprintw(com_win, no_file_string);
		wrefresh(com_win);
	}
	else
		check_fp();

	clear_com_win = TRUE;

	while(edit) 
	{
		int keyt;
		wrefresh(text_win);
		keyt = wget_wch(text_win, &in);
		if (keyt == ERR)
			exit(0);

		resize_check();

		if (clear_com_win)
		{
			clear_com_win = FALSE;
			wmove(com_win, 0, 0);
			werase(com_win);
			if (!info_window)
			{
				wprintw(com_win, "%S", com_win_message);
			}
			wrefresh(com_win);
		}

		if (keyt == KEY_CODE_YES)
			function_key();
		else if ((in == '\10') || (in == 127))
		{
			in = 8;		/* make sure key is set to backspace */
			delete(TRUE);
		}
		else if ((in > 31) || (in == 9))
			insert(in);
		else if ((in >= 0) && (in <= 31))
		{
			if (emacs_keys_mode)
				emacs_control();
			else
				control();
		}
	}
	return(0);
}

wchar_t *
resiz_line(factor, rline, rpos)	/* resize the line to length + factor*/
int factor;		/* resize factor				*/
struct text *rline;	/* position in line				*/
int rpos;
{
	wchar_t *rpoint;
	int resiz_var;
 
	rline->max_length += factor;
	rpoint = rline->line = realloc(rline->line, rline->max_length * sizeof(wchar_t));
	for (resiz_var = 1 ; (resiz_var < rpos) ; resiz_var++)
		rpoint++;
	return(rpoint);
}

void 
insert(character)		/* insert character into line		*/
int character;			/* new character			*/
{
	int counter;
	int value;
	wchar_t *temp;	/* temporary pointer			*/
	wchar_t *temp2;	/* temporary pointer			*/

	if ((character == '\011') && (expand_tabs))
	{
		counter = len_char('\011', scr_horz);
		for (; counter > 0; counter--)
			insert(' ');
		return;
	}
	text_changes = TRUE;
	if ((curr_line->max_length - curr_line->line_length) < 5)
		point = resiz_line(10, curr_line, position);
	curr_line->line_length++;
	temp = point;
	counter = position;
	while (counter < curr_line->line_length)	/* find end of line */
	{
		counter++;
		temp++;
	}
	temp++;			/* increase length of line by one	*/
	while (point < temp)
	{
		temp2=temp - 1;
		*temp= *temp2;	/* shift characters over by one		*/
		temp--;
	}
	*point = character;	/* insert new character			*/
	wclrtoeol(text_win);
	if ((character >= 0) && (character < ' ')) /* check for TAB character*/
	{
		scr_pos = scr_horz += out_char(text_win, character, scr_horz);
		point++;
		position++;
	}
	else
	{
		waddnwstr(text_win, &character, 1);
		scr_pos = ++scr_horz;
		point++;
		position ++;
	}

	if ((observ_margins) && (right_margin < scr_pos))
	{
		counter = position;
		while (scr_pos > right_margin)
			prev_word();
		if (scr_pos == 0)
		{
			while (position < counter)
				right(TRUE);
		}
		else
		{
			counter -= position;
			insert_line(TRUE);
			for (value = 0; value < counter; value++)
				right(TRUE);
		}
	}

	if ((scr_horz - horiz_offset) > last_col)
	{
		horiz_offset += 8;
		midscreen(scr_vert, point);
	}

	else if ((character != ' ') && (character != '\t'))
		formatted = FALSE;

	draw_line(scr_vert, scr_horz, point, position, curr_line->line_length);
}

void 
delete(disp)			/* delete character		*/
int disp;
{
	wchar_t *tp;
	wchar_t *temp2;
	struct text *temp_buff;
	int temp_vert;
	int temp_pos;
	int del_width = 1;

	if (point != curr_line->line)	/* if not at beginning of line	*/
	{
		text_changes = TRUE;
		temp2 = tp = point;
		tp -= del_width;
		point -= del_width;
		position -= del_width;
		temp_pos = position;
		curr_line->line_length -= del_width;
		if ((*tp < ' ') || (*tp >= 127))	/* check for TAB */
			scanline(tp);
		else
			scr_horz -= del_width;
		scr_pos = scr_horz;
		if (in == 8)
		{
			if (del_width == 1)
				*d_char = *point; /* save deleted character  */
			else
			{
				d_char[0] = *point;
				d_char[1] = *(point + 1);
			}
			d_char[del_width] = '\0';
		}
		while (temp_pos <= curr_line->line_length)
		{
			temp_pos++;
			*tp = *temp2;
			tp++;
			temp2++;
		}
		if (scr_horz < horiz_offset)
		{
			horiz_offset -= 8;
			midscreen(scr_vert, point);
		}
	}
	else if (curr_line->prev_line != NULL)
	{
		text_changes = TRUE;
		left(disp);			/* go to previous line	*/
		temp_buff = curr_line->next_line;
		point = resiz_line(temp_buff->line_length, curr_line, position);
		if (temp_buff->next_line != NULL)
			temp_buff->next_line->prev_line = curr_line;
		curr_line->next_line = temp_buff->next_line;
		temp2 = temp_buff->line;
		if (in == 8)
		{
			d_char[0] = '\n';
			d_char[1] = '\0';
		}
		tp = point;
		temp_pos = 1;
		while (temp_pos < temp_buff->line_length)
		{
			curr_line->line_length++;
			temp_pos++;
			*tp = *temp2;
			tp++;
			temp2++;
		}
		*tp = '\0';
		free(temp_buff->line);
		free(temp_buff);
		temp_buff = curr_line;
		temp_vert = scr_vert;
		scr_pos = scr_horz;
		if (scr_vert < last_line)
		{
			wmove(text_win, scr_vert + 1, 0);
			wdeleteln(text_win);
		}
		while ((temp_buff != NULL) && (temp_vert < last_line))
		{
			temp_buff = temp_buff->next_line;
			temp_vert++;
		}
		if ((temp_vert == last_line) && (temp_buff != NULL))
		{
			tp = temp_buff->line;
			wmove(text_win, last_line,0);
			wclrtobot(text_win);
			draw_line(last_line, 0, tp, 1, temp_buff->line_length);
			wmove(text_win, scr_vert, (scr_horz - horiz_offset));
		}
	}
	draw_line(scr_vert, scr_horz, point, position, curr_line->line_length);
	formatted = FALSE;
}

void 
scanline(pos)	/* find the proper horizontal position for the pointer	*/
wchar_t *pos;
{
	int temp;
	wchar_t *ptr;

	ptr = curr_line->line;
	temp = 0;
	while (ptr < pos)
	{
		if (*ptr <= 8)
			temp += 2;
		else if (*ptr == 9)
			temp += tabshift(temp);
		else if ((*ptr >= 10) && (*ptr <= 31))
			temp += 2;
		else if ((*ptr >= 32) && (*ptr < 127))
			temp++;
		else if (*ptr == 127)
			temp += 2;
		else if (!eightbit)
			temp += 5;
		else
			temp++;
		ptr++;
	}
	scr_horz = temp;
	if ((scr_horz - horiz_offset) > last_col)
	{
		horiz_offset = (scr_horz - (scr_horz % 8)) - (COLS - 8);
		midscreen(scr_vert, point);
	}
	else if (scr_horz < horiz_offset)
	{
		horiz_offset = max(0, (scr_horz - (scr_horz % 8)));
		midscreen(scr_vert, point);
	}
}

int 
tabshift(temp_int)		/* give the number of spaces to shift	*/
int temp_int;
{
	int leftover;

	leftover = ((temp_int + 1) % 8);
	if (leftover == 0)
		return (1);
	else
		return (9 - leftover);
}

int 
out_char(window, character, column)	/* output non-printing character */
WINDOW *window;
int character;
int column;
{
	int i1, i2;
	wchar_t *string;

	if (character == TAB)
	{
		i1 = tabshift(column);
		for (i2 = 0; 
		  (i2 < i1) && (((column+i2+1)-horiz_offset) < last_col); i2++)
		{
			waddch(window, ' ');
		}
		return(i1);
	}
	else if ((character >= '\0') && (character < ' '))
	{
		string = table[(int) character];
	}
	else
	{
		waddnwstr(window, &character, 1);
		return(1);
	}
	for (i2 = 0; (string[i2] != '\0') && (((column+i2+1)-horiz_offset) < last_col); i2++)
		waddch(window, string[i2]);
	return(wcslen(string));
}

int 
len_char(character, column)	/* return the length of the character	*/
wchar_t character;
int column;	/* the column must be known to provide spacing for tabs	*/
{
	int length;

	if (character == '\t')
		length = tabshift(column);
	else if ((character >= 0) && (character < 32))
		length = 2;
	else if ((character >= 32) && (character <= 126))
		length = 1;
	else if (character == 127)
		length = 2;
	else if (((character > 126) || (character < 0)) && (!eightbit))
		length = 5;
	else
		length = 1;

	return(length);
}

void 
draw_line(vertical, horiz, ptr, t_pos, length)	/* redraw line from current position */
int vertical;	/* current vertical position on screen		*/
int horiz;	/* current horizontal position on screen	*/
wchar_t *ptr;	/* pointer to line				*/
int t_pos;	/* current position (offset in bytes) from bol	*/
int length;	/* length (in bytes) of line			*/
{
	int d;		/* partial length of special or tab char to display  */
	wchar_t *temp;	/* temporary pointer to position in line	     */
	int abs_column;	/* offset in screen units from begin of line	     */
	int column;	/* horizontal position on screen		     */
	int row;	/* vertical position on screen			     */
	int posit;	/* temporary position indicator within line	     */

	abs_column = horiz;
	column = horiz - horiz_offset;
	row = vertical;
	temp = ptr;
	d = 0;
	posit = t_pos;
	if (column < 0)
	{
		wmove(text_win, row, 0);
		wclrtoeol(text_win);
	}
	while (column < 0)
	{
		d = len_char(*temp, abs_column);
		abs_column += d;
		column += d;
		posit++;
		temp++;
	}
	wmove(text_win, row, column);
	wclrtoeol(text_win);
	while ((posit < length) && (column <= last_col))
	{
		if (*temp < 32)
		{
			column += len_char(*temp, abs_column);
			abs_column += out_char(text_win, *temp, abs_column);
		}
		else
		{
			abs_column++;
			column++;
			waddnwstr(text_win, temp, 1);
		}
		posit++;
		temp++;
	}
	if (column < last_col)
		wclrtoeol(text_win);
	wmove(text_win, vertical, (horiz - horiz_offset));
}

void 
insert_line(disp)			/* insert new line		*/
int disp;
{
	int temp_pos;
	int temp_pos2;
	wchar_t *temp;
	wchar_t *extra;
	struct text *temp_nod;

	text_changes = TRUE;
	wmove(text_win, scr_vert, (scr_horz - horiz_offset));
	wclrtoeol(text_win);
	temp_nod= txtalloc();
	temp_nod->line = extra= malloc(10 * sizeof(wchar_t));
	temp_nod->line_length = 1;
	temp_nod->max_length = 10;
	temp_nod->line_number = curr_line->line_number + 1;
	temp_nod->next_line = curr_line->next_line;
	if (temp_nod->next_line != NULL)
		temp_nod->next_line->prev_line = temp_nod;
	temp_nod->prev_line = curr_line;
	curr_line->next_line = temp_nod;
	temp_pos2 = position;
	temp = point;
	if (temp_pos2 < curr_line->line_length)
	{
		temp_pos = 1;
		while (temp_pos2 < curr_line->line_length)
		{
			if ((temp_nod->max_length - temp_nod->line_length)< 5)
				extra = resiz_line(10, temp_nod, temp_pos);
			temp_nod->line_length++;
			temp_pos++;
			temp_pos2++;
			*extra= *temp;
			extra++;
			temp++;
		}
		temp=point;
		*temp = '\0';
		temp = resiz_line((1 - temp_nod->line_length), curr_line, position);
		curr_line->line_length = 1 + temp - curr_line->line;
	}
	curr_line->line_length = position;
	curr_line = temp_nod;
	*extra = '\0';
	position = 1;
	point= curr_line->line;
	if (disp)
	{
		if (scr_vert < last_line)
		{
			scr_vert++;
			wclrtoeol(text_win);
			wmove(text_win, scr_vert, 0);
			winsertln(text_win);
		}
		else
		{
			wmove(text_win, 0,0);
			wdeleteln(text_win);
			wmove(text_win, last_line,0);
			wclrtobot(text_win);
		}
		scr_pos = scr_horz = 0;
		if (horiz_offset)
		{
			horiz_offset = 0;
			midscreen(scr_vert, point);
		}
		draw_line(scr_vert, scr_horz, point, position,
			curr_line->line_length);
	}
}

struct text *txtalloc()		/* allocate space for line structure	*/
{
	return((struct text *) malloc(sizeof( struct text)));
}

wchar_t *next_word(string)		/* move to next word in string		*/
wchar_t *string;
{
	while ((*string != '\0') && ((*string != 32) && (*string != 9)))
		string++;
	while ((*string != '\0') && ((*string == 32) || (*string == 9)))
		string++;
	return(string);
}

int watoi(wchar_t *s)
{
	int x = 0;
	while (*s >= '0' && *s <= '9')
		x = x * 10 + *s++ - '0';
	return x;
}

void 
prev_word()	/* move to start of previous word in text	*/
{
	if (position != 1)
	{
		if ((position != 1) && ((point[-1] == ' ') || (point[-1] == '\t')))
		{	/* if at the start of a word	*/
			while ((position != 1) && ((*point != ' ') && (*point != '\t')))
				left(TRUE);
		}
		while ((position != 1) && ((*point == ' ') || (*point == '\t')))
			left(TRUE);
		while ((position != 1) && ((*point != ' ') && (*point != '\t')))
			left(TRUE);
		if ((position != 1) && ((*point == ' ') || (*point == '\t')))
			right(TRUE);
	}
	else
		left(TRUE);
}

void 
control()			/* use control for commands		*/
{
	wchar_t *string;

	if (in == 1)		/* control a	*/
	{
		string = get_string(ascii_code_str, TRUE);
		if (*string != '\0')
		{
			in = watoi(string);
			wmove(text_win, scr_vert, (scr_horz - horiz_offset));
			insert(in);
		}
		free(string);
	}
	else if (in == 2)	/* control b	*/
		bottom();
	else if (in == 3)	/* control c	*/
	{
		command_prompt();
	}
	else if (in == 4)	/* control d	*/
		down();
	else if (in == 5)	/* control e	*/
		search_prompt();
	else if (in == 6)	/* control f	*/
		undel_char();
	else if (in == 7)	/* control g	*/
		bol();
	else if (in == 8)	/* control h	*/
		delete(TRUE);
	else if (in == 9)	/* control i	*/
		;
	else if (in == 10)	/* control j	*/
		insert_line(TRUE);
	else if (in == 11)	/* control k	*/
		del_char();
	else if (in == 12)	/* control l	*/
		left(TRUE);
	else if (in == 13)	/* control m	*/
		insert_line(TRUE);
	else if (in == 14)	/* control n	*/
		move_rel('d', max(5, (last_line - 5)));
	else if (in == 15)	/* control o	*/
		eol();
	else if (in == 16)	/* control p	*/
		move_rel('u', max(5, (last_line - 5)));
	else if (in == 17)	/* control q	*/
		;
	else if (in == 18)	/* control r	*/
		right(TRUE);
	else if (in == 19)	/* control s	*/
		;
	else if (in == 20)	/* control t	*/
		top();
	else if (in == 21)	/* control u	*/
		up();
	else if (in == 22)	/* control v	*/
		undel_word();
	else if (in == 23)	/* control w	*/
		del_word();
	else if (in == 24)	/* control x	*/
		search(TRUE);
	else if (in == 25)	/* control y	*/
		del_line();
	else if (in == 26)	/* control z	*/
		undel_line();
	else if (in == 27)	/* control [ (escape)	*/
	{
		menu_op(main_menu);
	}	
}

/*
 |	Emacs control-key bindings
 */

void 
emacs_control()
{
	wchar_t *string;

	if (in == 1)		/* control a	*/
		bol();
	else if (in == 2)	/* control b	*/
		left(TRUE);
	else if (in == 3)	/* control c	*/
	{
		command_prompt();
	}
	else if (in == 4)	/* control d	*/
		del_char();
	else if (in == 5)	/* control e	*/
		eol();
	else if (in == 6)	/* control f	*/
		right(TRUE);
	else if (in == 7)	/* control g	*/
		move_rel('u', max(5, (last_line - 5)));
	else if (in == 8)	/* control h	*/
		delete(TRUE);
	else if (in == 9)	/* control i	*/
		;
	else if (in == 10)	/* control j	*/
		undel_char();
	else if (in == 11)	/* control k	*/
		del_line();
	else if (in == 12)	/* control l	*/
		undel_line();
	else if (in == 13)	/* control m	*/
		insert_line(TRUE);
	else if (in == 14)	/* control n	*/
		down();
	else if (in == 15)	/* control o	*/
	{
		string = get_string(ascii_code_str, TRUE);
		if (*string != '\0')
		{
			in = watoi(string);
			wmove(text_win, scr_vert, (scr_horz - horiz_offset));
			insert(in);
		}
		free(string);
	}
	else if (in == 16)	/* control p	*/
		up();
	else if (in == 17)	/* control q	*/
		;
	else if (in == 18)	/* control r	*/
		undel_word();
	else if (in == 19)	/* control s	*/
		;
	else if (in == 20)	/* control t	*/
		top();
	else if (in == 21)	/* control u	*/
		bottom();
	else if (in == 22)	/* control v	*/
		move_rel('d', max(5, (last_line - 5)));
	else if (in == 23)	/* control w	*/
		del_word();
	else if (in == 24)	/* control x	*/
		search(TRUE);
	else if (in == 25)	/* control y	*/
		search_prompt();
	else if (in == 26)	/* control z	*/
		adv_word();
	else if (in == 27)	/* control [ (escape)	*/
	{
		menu_op(main_menu);
	}	
}

void 
bottom()			/* go to bottom of file			*/
{
	while (curr_line->next_line != NULL)
		curr_line = curr_line->next_line;
	point = curr_line->line;
	if (horiz_offset)
		horiz_offset = 0;
	position = 1;
	midscreen(last_line, point);
	scr_pos = scr_horz;
}

void 
top()				/* go to top of file			*/
{
	while (curr_line->prev_line != NULL)
		curr_line = curr_line->prev_line;
	point = curr_line->line;
	if (horiz_offset)
		horiz_offset = 0;
	position = 1;
	midscreen(0, point);
	scr_pos = scr_horz;
}

void 
nextline()			/* move pointers to start of next line	*/
{
	curr_line = curr_line->next_line;
	point = curr_line->line;
	position = 1;
	if (scr_vert == last_line)
	{
		wmove(text_win, 0,0);
		wdeleteln(text_win);
		wmove(text_win, last_line,0);
		wclrtobot(text_win);
		draw_line(last_line,0,point,1,curr_line->line_length);
	}
	else
		scr_vert++;
}

void 
prevline()			/* move pointers to start of previous line*/
{
	curr_line = curr_line->prev_line;
	point = curr_line->line;
	position = 1;
	if (scr_vert == 0)
	{
		winsertln(text_win);
		draw_line(0,0,point,1,curr_line->line_length);
	}
	else
		scr_vert--;
	while (position < curr_line->line_length)
	{
		position++;
		point++;
	}
}

void 
left(disp)				/* move left one character	*/
int disp;
{
	if (point != curr_line->line)	/* if not at begin of line	*/
	{
		point--;
		position--;
		scanline(point);
		wmove(text_win, scr_vert, (scr_horz - horiz_offset));
		scr_pos = scr_horz;
	}
	else if (curr_line->prev_line != NULL)
	{
		if (!disp)
		{
			curr_line = curr_line->prev_line;
			point = curr_line->line + curr_line->line_length;
			position = curr_line->line_length;
			return;
		}
		position = 1;
		prevline();
		scanline(point);
		scr_pos = scr_horz;
		wmove(text_win, scr_vert, (scr_horz - horiz_offset));
	}
}

void 
right(disp)				/* move right one character	*/
int disp;
{
	if (position < curr_line->line_length)
	{
		point++;
		position++;
		scanline(point);
		wmove(text_win, scr_vert, (scr_horz - horiz_offset));
		scr_pos = scr_horz;
	}
	else if (curr_line->next_line != NULL)
	{
		if (!disp)
		{
			curr_line = curr_line->next_line;
			point = curr_line->line;
			position = 1;
			return;
		}
		nextline();
		scr_pos = scr_horz = 0;
		if (horiz_offset)
		{
			horiz_offset = 0;
			midscreen(scr_vert, point);
		}
		wmove(text_win, scr_vert, (scr_horz - horiz_offset));
		position = 1;	
	}
}

void 
find_pos()		/* move to the same column as on other line	*/
{
	scr_horz = 0;
	position = 1;
	while ((scr_horz < scr_pos) && (position < curr_line->line_length))
	{
		if (*point == 9)
			scr_horz += tabshift(scr_horz);
		else if (*point < ' ')
			scr_horz += 2;
		else if ((*point > 127) && ((curr_line->line_length - position) >= 2))
		{
			scr_horz += 2;
			point++;
			position++;
		}
		else
			scr_horz++;
		position++;
		point++;
	}
	if ((scr_horz - horiz_offset) > last_col)
	{
		horiz_offset = (scr_horz - (scr_horz % 8)) - (COLS - 8);
		midscreen(scr_vert, point);
	}
	else if (scr_horz < horiz_offset)
	{
		horiz_offset = max(0, (scr_horz - (scr_horz % 8)));
		midscreen(scr_vert, point);
	}
	wmove(text_win, scr_vert, (scr_horz - horiz_offset));
}

void 
up()					/* move up one line		*/
{
	if (curr_line->prev_line != NULL)
	{
		prevline();
		point = curr_line->line;
		find_pos();
	}
}

void 
down()					/* move down one line		*/
{
	if (curr_line->next_line != NULL)
	{
		nextline();
		find_pos();
	}
}

void 
function_key()				/* process function key		*/
{
	if (in == KEY_LEFT)
		left(TRUE);
	else if (in == KEY_RIGHT)
		right(TRUE);
	else if ( in == KEY_HOME)
		top();
	else if ( in == KEY_UP)
		up();
	else if (in == KEY_DOWN)
		down();
	else if (in == KEY_NPAGE)
		move_rel('d', max( 5, (last_line - 5)));
	else if (in == KEY_PPAGE)
		move_rel('u', max(5, (last_line - 5)));
	else if (in == KEY_DL)
		del_line();
	else if (in == KEY_DC)
		del_char();
	else if (in == KEY_BACKSPACE)
		delete(TRUE);
	else if (in == KEY_IL)
	{		/* insert a line before current line	*/
		insert_line(TRUE);
		left(TRUE);
	}
	else if (in == KEY_F(1))
		gold = !gold;
	else if (in == KEY_F(2))
	{
		if (gold)
		{
			gold = FALSE;
			undel_line();
		}
		else
			undel_char();
	}
	else if (in == KEY_F(3))
	{
		if (gold)
		{
			gold = FALSE;
			undel_word();
		}
		else
			del_word();
	}
	else if (in == KEY_F(4))
	{
		if (gold)
		{
			gold = FALSE;
			paint_info_win();
			midscreen(scr_vert, point);
		}
		else
			adv_word();
	}
	else if (in == KEY_F(5))
	{
		if (gold)
		{
			gold = FALSE;
			search_prompt();
		}
		else
			search(TRUE);
	}
	else if (in == KEY_F(6))
	{
		if (gold)
		{
			gold = FALSE;
			bottom();
		}
		else
			top();
	}
	else if (in == KEY_F(7))
	{
		if (gold)
		{
			gold = FALSE;
			eol();
		}
		else
			bol();
	}
	else if (in == KEY_F(8))
	{
		if (gold)
		{
			gold = FALSE;
			command_prompt();
		} 
		else
			adv_line();
	}
}

void 
command_prompt()
{
	wchar_t *cmd_str;
	int result;

	info_type = COMMANDS;
	paint_info_win();
	cmd_str = get_string(command_str, TRUE);
	if ((result = unique_test(cmd_str, commands)) != 1)
	{
		werase(com_win);
		wmove(com_win, 0, 0);
		if (result == 0)
			wprintw(com_win, unkn_cmd_str, cmd_str);
		else
			wprintw(com_win, "%S", non_unique_cmd_msg);

		wrefresh(com_win);

		info_type = CONTROL_KEYS;
		paint_info_win();

		if (cmd_str != NULL)
			free(cmd_str);
		return;
	}
	command(cmd_str);
	wrefresh(com_win);
	wmove(text_win, scr_vert, (scr_horz - horiz_offset));
	info_type = CONTROL_KEYS;
	paint_info_win();
	if (cmd_str != NULL)
		free(cmd_str);
}

void command(wchar_t *cmd_str1)		/* process commands from keyboard	*/
{
	wchar_t *cmd_str2 = NULL;
	wchar_t *cmd_str = cmd_str1;

	clear_com_win = TRUE;
	if (compare(cmd_str, HELP, FALSE))
		help();
	else if (compare(cmd_str, LINE, FALSE))
	{
		wmove(com_win, 0, 0);
		wclrtoeol(com_win);
		wprintw(com_win, line_num_str, curr_line->line_number);
		wprintw(com_win, line_len_str, curr_line->line_length);
	}
	else if (compare(cmd_str, FILE_str, FALSE))
	{
		wmove(com_win, 0, 0);
		wclrtoeol(com_win);
		if (in_file_name == NULL)
			wprintw(com_win, no_file_string);
		else
			wprintw(com_win, current_file_str, in_file_name);
	}
	else if ((*cmd_str >= '0') && (*cmd_str <= '9'))
		goto_line(cmd_str);
	else if (compare(cmd_str, CHARACTER, FALSE))
	{
		wmove(com_win, 0, 0);
		wclrtoeol(com_win);
		wprintw(com_win, char_str, *point);
	}
	else if (compare(cmd_str, REDRAW, FALSE))
		redraw();
	else if (compare(cmd_str, RESEQUENCE, FALSE))
	{
		tmp_line = first_line->next_line;
		while (tmp_line != NULL)
		{
		tmp_line->line_number = tmp_line->prev_line->line_number + 1;
			tmp_line = tmp_line->next_line;
		}
	}
	else if (compare(cmd_str, SAVE, FALSE))
	  write_file(tmp_file, -1);
	else if (compare(cmd_str, AUTHOR, FALSE))
	{
		wmove(com_win, 0, 0);
		wclrtoeol(com_win);
		wprintw(com_win, "written by Hugh Mahon");
	}
	else if (compare(cmd_str, ee_VERSION, FALSE))
	{
		wmove(com_win, 0, 0);
		wclrtoeol(com_win);
		wprintw(com_win, "%s", version);
	}
	else if (compare(cmd_str, CASE, FALSE))
		case_sen = TRUE;
	else if (compare(cmd_str, NOCASE, FALSE))
		case_sen = FALSE;
	else if (compare(cmd_str, EXPAND, FALSE))
		expand_tabs = TRUE;
	else if (compare(cmd_str, NOEXPAND, FALSE))
		expand_tabs = FALSE;
	else if (compare(cmd_str, Exit_string, FALSE))
		ee_finish();
	else if (compare(cmd_str, QUIT_string, FALSE))
		quit(0);
	else if ((*cmd_str == '<') && (!in_pipe))
	{
		in_pipe = TRUE;
		cmd_str++;
		if ((*cmd_str == ' ') || (*cmd_str == '\t'))
			cmd_str = next_word((wchar_t*)cmd_str);
		command(cmd_str);
		in_pipe = FALSE;
	}
	else if ((*cmd_str == '>') && (!out_pipe))
	{
		out_pipe = TRUE;
		cmd_str++;
		if ((*cmd_str == ' ') || (*cmd_str == '\t'))
			cmd_str = next_word((wchar_t*)cmd_str);
		command(cmd_str);
		out_pipe = FALSE;
	}
	else
	{
		wmove(com_win, 0, 0);
		wclrtoeol(com_win);
		wprintw(com_win, unkn_cmd_str, cmd_str);
	}
	if (cmd_str2 != NULL)
		free(cmd_str2);
}

int 
scan(line, offset, column)	/* determine horizontal position for get_string	*/
wchar_t *line;
int offset;
int column;
{
	wchar_t *stemp;
	int i;
	int j;

	stemp = line;
	i = 0;
	j = column;
	while (i < offset)
	{
		i++;
		j += len_char(*stemp, j);
		stemp++;
	}
	return(j);
}

wchar_t *
get_string(prompt, advance)	/* read string from input on command line */
wchar_t *prompt;		/* string containing user prompt message	*/
int advance;		/* if true, skip leading spaces and tabs	*/
{
	wchar_t *string;
	wchar_t *tmp_string;
	wchar_t *nam_str;
	wchar_t *g_point;
	int tmp_int;
	int g_horz, g_position, g_pos;
	int esc_flag;

	g_point = tmp_string = malloc(512 * sizeof(wchar_t));
	wmove(com_win,0,0);
	wclrtoeol(com_win);
	waddwstr(com_win, prompt);
	wrefresh(com_win);
	nam_str = tmp_string;
	clear_com_win = TRUE;
	g_horz = g_position = scan(prompt, wcslen(prompt), 0);
	g_pos = 0;
	do
	{
		int keyt;
		esc_flag = FALSE;
		keyt = wget_wch(com_win, &in);
		if (keyt == ERR)
			exit(0);
		if (keyt == KEY_CODE_YES)
		{
			if (in == KEY_BACKSPACE)
				in = 8;
			else
				continue;
		}
		if (((in == 8) || (in == 127)) && (g_pos > 0))
		{
			tmp_int = g_horz;
			g_pos--;
			g_horz = scan(g_point, g_pos, g_position);
			tmp_int = tmp_int - g_horz;
			for (; 0 < tmp_int; tmp_int--)
			{
				if ((g_horz+tmp_int) < (last_col - 1))
				{
					waddch(com_win, '\010');
					waddch(com_win, ' ');
					waddch(com_win, '\010');
				}
			}
			nam_str--;
		}
		else if ((in != 8) && (in != 127) && (in != '\n') && (in != '\r'))
		{
			if (in == '\026')	/* control-v, accept next character verbatim	*/
			{			/* allows entry of ^m, ^j, and ^h	*/
				int keyt;
				esc_flag = TRUE;
				do
				{
					keyt = wget_wch(com_win, &in);
				
					if (keyt == ERR)
						exit(0);
				} while (keyt != OK);
			}
			*nam_str = in;
			g_pos++;
			if (in < ' ')
				g_horz += out_char(com_win, in, g_horz);
			else
			{
				g_horz++;
				if (g_horz < (last_col - 1))
					waddnwstr(com_win, &in, 1);
			}
			nam_str++;
		}
		wrefresh(com_win);
		if (esc_flag)
			in = '\0';
	} while ((in != '\n') && (in != '\r'));
	*nam_str = '\0';
	nam_str = tmp_string;
	if (((*nam_str == ' ') || (*nam_str == 9)) && (advance))
		nam_str = next_word(nam_str);
	string = malloc((wcslen(nam_str) + 1) * sizeof(wchar_t));
	wcscpy(string, nam_str);
	free(tmp_string);
	wrefresh(com_win);
	return(string);
}

int 
compare(string1, string2, sensitive)	/* compare two strings	*/
wchar_t *string1;
wchar_t *string2;
int sensitive;
{
	wchar_t *strng1;
	wchar_t *strng2;
	int tmp;
	int equal;

	strng1 = string1;
	strng2 = string2;
	tmp = 0;
	if ((strng1 == NULL) || (strng2 == NULL) || (*strng1 == '\0') || (*strng2 == '\0'))
		return(FALSE);
	equal = TRUE;
	while (equal)
	{
		if (sensitive)
		{
			if (*strng1 != *strng2)
				equal = FALSE;
		}
		else
		{
			if (toupper(*strng1) != toupper(*strng2))
				equal = FALSE;
		}
		strng1++;
		strng2++;
		if ((*strng1 == '\0') || (*strng2 == '\0') || (*strng1 == ' ') || (*strng2 == ' '))
			break;
		tmp++;
	}
	return(equal);
}

void goto_line(wchar_t *cmd_str)
{
	int number;
	int i;
	wchar_t *ptr;
	char direction = 0;
	struct text *t_line;

	ptr = cmd_str;
	i= 0;
	while ((*ptr >='0') && (*ptr <= '9'))
	{
		i= i * 10 + (*ptr - '0');
		ptr++;
	}
	number = i;
	i = 0;
	t_line = curr_line;
	while ((t_line->line_number > number) && (t_line->prev_line != NULL))
	{
		i++;
		t_line = t_line->prev_line;
		direction = 'u';
	}
	while ((t_line->line_number < number) && (t_line->next_line != NULL))
	{
		i++;
		direction = 'd';
		t_line = t_line->next_line;
	}
	if ((i < 30) && (i > 0))
	{
		move_rel(direction, i);
	}
	else
	{
		curr_line = t_line;
		point = curr_line->line;
		position = 1;
		midscreen((last_line / 2), point);
		scr_pos = scr_horz;
	}
	wmove(com_win, 0, 0);
	wclrtoeol(com_win);
	wprintw(com_win, line_num_str, curr_line->line_number);
	wmove(text_win, scr_vert, (scr_horz - horiz_offset));
}

void 
midscreen(line, pnt)	/* put current line in middle of screen	*/
int line;
wchar_t *pnt;
{
	struct text *mid_line;
	int i;

	line = min(line, last_line);
	mid_line = curr_line;
	for (i = 0; ((i < line) && (curr_line->prev_line != NULL)); i++)
		curr_line = curr_line->prev_line;
	scr_vert = scr_horz = 0;
	wmove(text_win, 0, 0);
	draw_screen();
	scr_vert = i;
	curr_line = mid_line;
	scanline(pnt);
	wmove(text_win, scr_vert, (scr_horz - horiz_offset));
}

void 
check_fp()		/* open or close files according to flags */
{
	int line_num;
	int temp;
	struct stat buf;

	clear_com_win = TRUE;
	tmp_vert = scr_vert;
	tmp_horz = scr_horz;
	tmp_line = curr_line;
	if (input_file)
		in_file_name = tmp_file;
	
	temp = stat(tmp_file, &buf);
	buf.st_mode &= ~07777;
	if ((temp != -1) && (buf.st_mode != 0100000) && (buf.st_mode != 0))
	{
		wprintw(com_win, file_is_dir_msg, tmp_file);
		wrefresh(com_win);
		if (input_file)
		{
			quit(0);
			return;
		}
		else
			return;
	}
	if ((get_fd = open(tmp_file, O_RDONLY)) == -1)
	{
		wmove(com_win, 0, 0);
		wclrtoeol(com_win);
		if (input_file)
			wprintw(com_win, new_file_msg, tmp_file);
		else
			wprintw(com_win, cant_open_msg, tmp_file);
		wrefresh(com_win);
		wmove(text_win, scr_vert, (scr_horz - horiz_offset));
		wrefresh(text_win);
		recv_file = FALSE;
		input_file = FALSE;
		return;
	}
	else
		get_file(tmp_file);

	recv_file = FALSE;
	line_num = curr_line->line_number;
	scr_vert = tmp_vert;
	scr_horz = tmp_horz;
	if (input_file)
		curr_line= first_line;
	else
		curr_line = tmp_line;
	point = curr_line->line;
	draw_screen();
	if (input_file)
	{
		input_file = FALSE;
		if (start_at_line != NULL)
		{
			line_num = watoi(start_at_line) - 1;
			move_rel('d', line_num);
			line_num = 0;
			start_at_line = NULL;
		}
	}
	else
	{
		wmove(com_win, 0, 0);
		wclrtoeol(com_win);
		text_changes = TRUE;
		if ((tmp_file != NULL) && (*tmp_file != '\0'))
			wprintw(com_win, file_read_fin_msg, tmp_file);
	}
	wrefresh(com_win);
	wmove(text_win, scr_vert, (scr_horz - horiz_offset));
	wrefresh(text_win);
}

void 
get_file(file_name)	/* read specified file into current buffer	*/
char *file_name;
{
	int can_read;		/* file has at least one character	*/
	int length;		/* length of line read by read		*/
	int append;		/* should text be appended to current line */
	struct text *temp_line;
	char ro_flag = FALSE;
	char in_string[MAX_FILE+1];

	if (recv_file)		/* if reading a file			*/
	{
		wmove(com_win, 0, 0);
		wclrtoeol(com_win);
		wprintw(com_win, reading_file_msg, file_name);
		if (access(file_name, 2))	/* check permission to write */
		{
			if ((errno == ENOTDIR) || (errno == EACCES) || (errno == EROFS) || (errno == ETXTBSY) || (errno == EFAULT))
			{
				wprintw(com_win, read_only_msg);
				ro_flag = TRUE;
			}
		}
		wrefresh(com_win);
	}
	if (curr_line->line_length > 1)	/* if current line is not blank	*/
	{
		insert_line(FALSE);
		left(FALSE);
		append = FALSE;
	}
	else
		append = TRUE;
	can_read = FALSE;		/* test if file has any characters  */
	length = read(get_fd, in_string, sizeof(in_string) - 1);
	// in DGL, config files are better capped
	if (length != -1)
	{
		can_read = TRUE;  /* if set file has at least 1 character   */
		in_string[length] = 0;
		get_line(length, in_string, &append);
	}
	if ((can_read) && (curr_line->line_length == 1) && (curr_line->prev_line))
	{
		temp_line = curr_line->prev_line;
		temp_line->next_line = curr_line->next_line;
		if (temp_line->next_line != NULL)
			temp_line->next_line->prev_line = temp_line;
		if (curr_line->line != NULL)
			free(curr_line->line);
		free(curr_line);
		curr_line = temp_line;
	}
	if (input_file)	/* if this is the file to be edited display number of lines	*/
	{
		wmove(com_win, 0, 0);
		wclrtoeol(com_win);
		wprintw(com_win, file_read_lines_msg, in_file_name, curr_line->line_number);
		if (ro_flag)
			wprintw(com_win, read_only_msg);
		wrefresh(com_win);
	}
	else if (can_read)	/* not input_file and file is non-zero size */
		text_changes = TRUE;

	if (recv_file)		/* if reading a file			*/
	{
		in = EOF;
	}
}

void 
get_line(length, in_str, append)	/* read string and split into lines */
int length;		/* length of string read by read		*/
char *in_str;		/* string read by read				*/
int *append;	/* TRUE if must append more text to end of current line	*/
{
	wchar_t *str1;
	wchar_t *str2;
	int num;		/* offset from start of string		*/
	int char_count;		/* length of new line (or added portion	*/
	int temp_counter;	/* temporary counter value		*/
	struct text *tline;	/* temporary pointer to new line	*/
	int first_time;		/* if TRUE, the first time through the loop */
	wchar_t in_string[MAX_FILE];
	length = mbstowcs(in_string, in_str, sizeof(in_string)/sizeof(wchar_t));

	if (length == -1) {
	    wmove(com_win, 0, 0);
	    wprintw(com_win, "ERROR: Encountered a strange character. --more--");
	    wclrtoeol(com_win);
	    wrefresh(com_win);
	    (void) wget_wch(com_win, &in);
	    resetty();
	    endwin();
	    exit(0);
	}

	str2 = in_string;
	num = 0;
	first_time = TRUE;
	while (num < length)
	{
		if (!first_time)
		{
			if (num < length)
			{
				str2++;
				num++;
			}
		}
		else
			first_time = FALSE;
		str1 = str2;
		char_count = 1;
		/* find end of line	*/
		while ((*str2 != '\n') && (num < length))
		{
			str2++;
			num++;
			char_count++;
		}
		if (!(*append))	/* if not append to current line, insert new one */
		{
			tline = txtalloc();	/* allocate data structure for next line */
			tline->line_number = curr_line->line_number + 1;
			tline->next_line = curr_line->next_line;
			tline->prev_line = curr_line;
			curr_line->next_line = tline;
			if (tline->next_line != NULL)
				tline->next_line->prev_line = tline;
			curr_line = tline;
			curr_line->line = point = (wchar_t *) malloc(char_count * sizeof(wchar_t));
			curr_line->line_length = char_count;
			curr_line->max_length = char_count;
		}
		else
		{
			point = resiz_line(char_count, curr_line, curr_line->line_length); 
			curr_line->line_length += (char_count - 1);
		}
		for (temp_counter = 1; temp_counter < char_count; temp_counter++)
		{
			*point = *str1;
			point++;
			str1++;
		}
		*point = '\0';
		*append = FALSE;
		if ((num == length) && (*str2 != '\n'))
			*append = TRUE;
	}
}

void 
draw_screen()		/* redraw the screen from current postion	*/
{
	struct text *temp_line;
	wchar_t *line_out;
	int temp_vert;

	temp_line = curr_line;
	temp_vert = scr_vert;
	wclrtobot(text_win);
	while ((temp_line != NULL) && (temp_vert <= last_line))
	{
		line_out = temp_line->line;
		draw_line(temp_vert, 0, line_out, 1, temp_line->line_length);
		temp_vert++;
		temp_line = temp_line->next_line;
	}
	wmove(text_win, temp_vert, 0);
	wmove(text_win, scr_vert, (scr_horz - horiz_offset));
}

void 
ee_finish()	/* prepare to exit edit session	*/
{
	char *file_name = in_file_name;

	/*
	 |	changes made here should be reflected in the 'save' 
	 |	portion of file_op()
	 */

//	if ((file_name == NULL) || (*file_name == '\0'))
//		file_name = get_string(save_file_name_prompt, TRUE);

	if ((file_name == NULL) || (*file_name == '\0'))
	{
		wmove(com_win, 0, 0);
		wprintw(com_win, file_not_saved_msg);
		wclrtoeol(com_win);
		wrefresh(com_win);
		clear_com_win = TRUE;
		return;
	}

	if (write_file(file_name, -1))
	{
		text_changes = FALSE;
		quit(0);
	}
}

int 
quit(noverify)		/* exit editor			*/
int noverify;
{
	wchar_t *ans;

	touchwin(text_win);
	wrefresh(text_win);
	if ((text_changes) && (!noverify))
	{
		ans = get_string(changes_made_prompt, TRUE);
		if (toupper(*ans) == toupper(*yes_char))
			text_changes = FALSE;
		else
			return(0);
		free(ans);
	}
	
	if (info_window)
	  wrefresh(info_win);
	wrefresh(com_win);
	resetty();
	endwin();
	putchar('\n');
	exit(0);
}

void 
edit_abort(arg)
int arg;
{
	wrefresh(com_win);
	resetty();
	endwin();
	putchar('\n');
	exit(1);
}

void 
delete_text()
{
	while (curr_line->next_line != NULL)
		curr_line = curr_line->next_line;
	while (curr_line != first_line)
	{
		free(curr_line->line);
		curr_line = curr_line->prev_line;
		free(curr_line->next_line);
	}
	curr_line->next_line = NULL;
	*curr_line->line = '\0';
	curr_line->line_length = 1;
	curr_line->line_number = 1;
	point = curr_line->line;
	scr_pos = scr_vert = scr_horz = 0;
	position = 1;
}

/* If fd >= 0, then use the previously opened file. This is a 
   hack to get safe tempfile handling in ispell.*/
int write_file(char *file_name, int fd)
{
	wchar_t cr;
	wchar_t *tmp_point;
	struct text *out_line;
	int lines, charac;
	int temp_pos;
	int write_flag = TRUE;

	charac = lines = 0;
	if ((fd < 0) &&
            ((in_file_name == NULL) || strcmp(in_file_name, file_name)))
	{
		if ((temp_fp = fopen(file_name, "r")))
		{
			tmp_point = get_string(file_exists_prompt, TRUE);
			if (toupper(*tmp_point) == toupper(*yes_char))
				write_flag = TRUE;
			else 
				write_flag = FALSE;
			fclose(temp_fp);
			free(tmp_point);
		}
	}

	clear_com_win = TRUE;

	if (write_flag)
	{
                if (fd < 0) 
                {
                        temp_fp = fopen(file_name, "w");
                }
                else
                {
                        temp_fp = fdopen(fd, "w");
                }

		if (temp_fp == NULL)
		{
			clear_com_win = TRUE;
			wmove(com_win,0,0);
			wclrtoeol(com_win);
			wprintw(com_win, create_file_fail_msg, file_name);
			wrefresh(com_win);
			return(FALSE);
		}
		else
		{
			wmove(com_win,0,0);
			wclrtoeol(com_win);
			wprintw(com_win, writing_file_msg, file_name);
			wrefresh(com_win);
			cr = '\n';
			out_line = first_line;
			while (out_line != NULL)
			{
				temp_pos = 1;
				tmp_point= out_line->line;
				while (temp_pos < out_line->line_length)
				{
					fputwc(*tmp_point, temp_fp);
					tmp_point++;
					temp_pos++;
				}
				charac += out_line->line_length;
				out_line = out_line->next_line;
				fputwc(cr, temp_fp);
				lines++;
			}
			fclose(temp_fp);
			wmove(com_win,0,0);
			wclrtoeol(com_win);
			wprintw(com_win, file_written_msg, file_name, lines, charac);
			wrefresh(com_win);
			return(TRUE);
		}
	}
	else
		return(FALSE);
}

int 
search(display_message)		/* search for string in srch_str	*/
int display_message;
{
	int lines_moved;
	int iter;
	int found;

	if ((srch_str == NULL) || (*srch_str == '\0'))
		return(FALSE);
	if (display_message)
	{
		wmove(com_win, 0, 0);
		wclrtoeol(com_win);
		wprintw(com_win, searching_msg);
		wrefresh(com_win);
		clear_com_win = TRUE;
	}
	lines_moved = 0;
	found = FALSE;
	srch_line = curr_line;
	srch_1 = point;
	if (position < curr_line->line_length)
		srch_1++;
	iter = position + 1;
	while ((!found) && (srch_line != NULL))
	{
		while ((iter < srch_line->line_length) && (!found))
		{
			srch_2 = srch_1;
			if (case_sen)	/* if case sensitive		*/
			{
				srch_3 = srch_str;
			while ((*srch_2 == *srch_3) && (*srch_3 != '\0'))
				{
					found = TRUE;
					srch_2++;
					srch_3++;
				}	/* end while	*/
			}
			else		/* if not case sensitive	*/
			{
				srch_3 = u_srch_str;
			while ((toupper(*srch_2) == *srch_3) && (*srch_3 != '\0'))
				{
					found = TRUE;
					srch_2++;
					srch_3++;
				}
			}	/* end else	*/
			if (!((*srch_3 == '\0') && (found)))
			{
				found = FALSE;
				if (iter < srch_line->line_length)
					srch_1++;
				iter++;
			}
		}
		if (!found)
		{
			srch_line = srch_line->next_line;
			if (srch_line != NULL)
				srch_1 = srch_line->line;
			iter = 1;
			lines_moved++;
		}
	}
	if (found)
	{
		if (display_message)
		{
			wmove(com_win, 0, 0);
			wclrtoeol(com_win);
			wrefresh(com_win);
		}
		if (lines_moved == 0)
		{
			while (position < iter)
				right(TRUE);
		}
		else
		{
			if (lines_moved < 30)
			{
				move_rel('d', lines_moved);
				while (position < iter)
					right(TRUE);
			}
			else 
			{
				curr_line = srch_line;
				point = srch_1;
				position = iter;
				scanline(point);
				scr_pos = scr_horz;
				midscreen((last_line / 2), point);
			}
		}
	}
	else
	{
		if (display_message)
		{
			wmove(com_win, 0, 0);
			wclrtoeol(com_win);
			wprintw(com_win, str_not_found_msg, srch_str);
			wrefresh(com_win);
		}
		wmove(text_win, scr_vert,(scr_horz - horiz_offset));
	}
	return(found);
}

void 
search_prompt()		/* prompt and read search string (srch_str)	*/
{
	if (srch_str != NULL)
		free(srch_str);
	if ((u_srch_str != NULL) && (*u_srch_str != '\0'))
		free(u_srch_str);
	srch_str = get_string(search_prompt_str, FALSE);
	gold = FALSE;
	srch_3 = srch_str;
	srch_1 = u_srch_str = (wchar_t*)malloc((wcslen(srch_str) + 1) * sizeof(wchar_t));
	while (*srch_3 != '\0')
	{
		*srch_1 = toupper(*srch_3);
		srch_1++;
		srch_3++;
	}
	*srch_1 = '\0';
	search(TRUE);
}

void 
del_char()			/* delete current character	*/
{
	in = 8;  /* backspace */
	if (position < curr_line->line_length)	/* if not end of line	*/
	{
		if ((*point > 127) && ((curr_line->line_length - position) >= 2))
		{
			point++;
			position++;
		}
		position++;
		point++;
		scanline(point);
		delete(TRUE);
	}
	else
	{
		right(FALSE);
		delete(FALSE);
	}
}

void 
undel_char()			/* undelete last deleted character	*/
{
	if (d_char[0] == '\n')	/* insert line if last del_char deleted eol */
		insert_line(TRUE);
	else
	{
		in = d_char[0];
		insert(in);
		if (d_char[1] != '\0')
		{
			in = d_char[1];
			insert(in);
		}
	}
}

void 
del_word()			/* delete word in front of cursor	*/
{
	int tposit;
	int difference;
	wchar_t *d_word2;
	wchar_t *d_word3;
	wchar_t tmp_char[3];

	if (d_word != NULL)
		free(d_word);
	d_word = (wchar_t*)malloc(curr_line->line_length * sizeof(wchar_t));
	tmp_char[0] = d_char[0];
	tmp_char[1] = d_char[1];
	tmp_char[2] = d_char[2];
	d_word3 = point;
	d_word2 = d_word;
	tposit = position;
	while ((tposit < curr_line->line_length) && 
				((*d_word3 != ' ') && (*d_word3 != '\t')))
	{
		tposit++;
		*d_word2 = *d_word3;
		d_word2++;
		d_word3++;
	}
	while ((tposit < curr_line->line_length) && 
				((*d_word3 == ' ') || (*d_word3 == '\t')))
	{
		tposit++;
		*d_word2 = *d_word3;
		d_word2++;
		d_word3++;
	}
	*d_word2 = '\0';
	d_wrd_len = difference = d_word2 - d_word;
	d_word2 = point;
	while (tposit < curr_line->line_length)
	{
		tposit++;
		*d_word2 = *d_word3;
		d_word2++;
		d_word3++;
	}
	curr_line->line_length -= difference;
	*d_word2 = '\0';
	draw_line(scr_vert, scr_horz,point,position,curr_line->line_length);
	d_char[0] = tmp_char[0];
	d_char[1] = tmp_char[1];
	d_char[2] = tmp_char[2];
	text_changes = TRUE;
	formatted = FALSE;
}

void 
undel_word()		/* undelete last deleted word		*/
{
	int temp;
	int tposit;
	wchar_t *tmp_old_ptr;
	wchar_t *tmp_space;
	wchar_t *tmp_ptr;
	wchar_t *d_word_ptr;

	/*
	 |	resize line to handle undeleted word
	 */
	if ((curr_line->max_length - (curr_line->line_length + d_wrd_len)) < 5)
		point = resiz_line(d_wrd_len, curr_line, position);
	tmp_ptr = tmp_space = (wchar_t*)malloc((curr_line->line_length + d_wrd_len) * sizeof(wchar_t));
	d_word_ptr = d_word;
	temp = 1;
	/*
	 |	copy d_word contents into temp space
	 */
	while (temp <= d_wrd_len)
	{
		temp++;
		*tmp_ptr = *d_word_ptr;
		tmp_ptr++;
		d_word_ptr++;
	}
	tmp_old_ptr = point;
	tposit = position;
	/*
	 |	copy contents of line from curent position to eol into 
	 |	temp space
	 */
	while (tposit < curr_line->line_length)
	{
		temp++;
		tposit++;
		*tmp_ptr = *tmp_old_ptr;
		tmp_ptr++;
		tmp_old_ptr++;
	}
	curr_line->line_length += d_wrd_len;
	tmp_old_ptr = point;
	*tmp_ptr = '\0';
	tmp_ptr = tmp_space;
	tposit = 1;
	/*
	 |	now copy contents from temp space back to original line
	 */
	while (tposit < temp)
	{
		tposit++;
		*tmp_old_ptr = *tmp_ptr;
		tmp_ptr++;
		tmp_old_ptr++;
	}
	*tmp_old_ptr = '\0';
	free(tmp_space);
	draw_line(scr_vert, scr_horz, point, position, curr_line->line_length);
}

void 
del_line()			/* delete from cursor to end of line	*/
{
	wchar_t *dl1;
	wchar_t *dl2;
	int tposit;

	if (d_line != NULL)
		free(d_line);
	d_line = (wchar_t*)malloc(curr_line->line_length * sizeof(wchar_t));
	dl1 = d_line;
	dl2 = point;
	tposit = position;
	while (tposit < curr_line->line_length)
	{
		*dl1 = *dl2;
		dl1++;
		dl2++;
		tposit++;
	}
	dlt_line->line_length = 1 + tposit - position;
	*dl1 = *point = '\0';
	curr_line->line_length = position;
	wclrtoeol(text_win);
	if (curr_line->next_line != NULL)
	{
		right(FALSE);
		delete(FALSE);
	}
	text_changes = TRUE;
}

void 
undel_line()			/* undelete last deleted line		*/
{
	wchar_t *ud1;
	wchar_t *ud2;
	int tposit;

	if (dlt_line->line_length == 0)
		return;

	insert_line(TRUE);
	left(TRUE);
	point = resiz_line(dlt_line->line_length, curr_line, position);
	curr_line->line_length += dlt_line->line_length - 1;
	ud1 = point;
	ud2 = d_line;
	tposit = 1;
	while (tposit < dlt_line->line_length)
	{
		tposit++;
		*ud1 = *ud2;
		ud1++;
		ud2++;
	}
	*ud1 = '\0';
	draw_line(scr_vert, scr_horz,point,position,curr_line->line_length);
}

void 
adv_word()			/* advance to next word		*/
{
while ((position < curr_line->line_length) && ((*point != 32) && (*point != 9)))
		right(TRUE);
while ((position < curr_line->line_length) && ((*point == 32) || (*point == 9)))
		right(TRUE);
}

void move_rel(char direction, int lines)	/* move relative to current line	*/
{
	int i;
	wchar_t *tmp;

	if (direction == 'u')
	{
		scr_pos = 0;
		while (position > 1)
			left(TRUE);
		for (i = 0; i < lines; i++)
		{
			up();
		}
		if ((last_line > 5) && ( scr_vert < 4))
		{
			tmp = point;
			tmp_line = curr_line;
			for (i= 0;(i<5)&&(curr_line->prev_line != NULL); i++)
			{
				up();
			}
			scr_vert = scr_vert + i;
			curr_line = tmp_line;
			point = tmp;
			scanline(point);
		}
	}
	else
	{
		if ((position != 1) && (curr_line->next_line != NULL))
		{
			nextline();
			scr_pos = scr_horz = 0;
			if (horiz_offset)
			{
				horiz_offset = 0;
				midscreen(scr_vert, point);
			}
		}
		else
			adv_line();
		for (i = 1; i < lines; i++)
		{
			down();
		}
		if ((last_line > 10) && (scr_vert > (last_line - 5)))
		{
			tmp = point;
			tmp_line = curr_line;
			for (i=0; (i<5) && (curr_line->next_line != NULL); i++)
			{
				down();
			}
			scr_vert = scr_vert - i;
			curr_line = tmp_line;
			point = tmp;
			scanline(point);
		}
	}
	wmove(text_win, scr_vert, (scr_horz - horiz_offset));
}

void 
eol()				/* go to end of line			*/
{
	if (position < curr_line->line_length)
	{
		while (position < curr_line->line_length)
			right(TRUE);
	}
	else if (curr_line->next_line != NULL)
	{
		right(TRUE);
		while (position < curr_line->line_length)
			right(TRUE);
	}
}

void 
bol()				/* move to beginning of line	*/
{
	if (point != curr_line->line)
	{
		while (point != curr_line->line)
			left(TRUE);
	}
	else if (curr_line->prev_line != NULL)
	{
		scr_pos = 0;
		up();
	}
}

void 
adv_line()	/* advance to beginning of next line	*/
{
	if ((point != curr_line->line) || (scr_pos > 0))
	{
		while (position < curr_line->line_length)
			right(TRUE);
		right(TRUE);
	}
	else if (curr_line->next_line != NULL)
	{
		scr_pos = 0;
		down();
	}
}

void 
set_up_term()		/* set up the terminal for operating with ae	*/
{
	if (!curses_initialized)
	{
		initscr();
		savetty();
		noecho();
		raw();
		nonl();
		curses_initialized = TRUE;
	}

	if (((LINES > 15) && (COLS >= 80)) && info_window)
		last_line = LINES - 8;
	else
	{
		info_window = FALSE;
		last_line = LINES - 2;
	}

	idlok(stdscr, TRUE);
	com_win = newwin(1, COLS, (LINES - 1), 0);
	keypad(com_win, TRUE);
	idlok(com_win, TRUE);
	wrefresh(com_win);
	if (!info_window)
		text_win = newwin((LINES - 1), COLS, 0, 0);
	else
		text_win = newwin((LINES - 7), COLS, 6, 0);
	keypad(text_win, TRUE);
	idlok(text_win, TRUE);
	wrefresh(text_win);
	help_win = newwin((LINES - 1), COLS, 0, 0);
	keypad(help_win, TRUE);
	idlok(help_win, TRUE);
	if (info_window)
	{
		info_type = CONTROL_KEYS;
		info_win = newwin(6, COLS, 0, 0);
		werase(info_win);
		paint_info_win();
	}

	last_col = COLS - 1;
	local_LINES = LINES;
	local_COLS = COLS;
}

void 
resize_check()
{
	if ((LINES == local_LINES) && (COLS == local_COLS))
		return;

	if (info_window)
		delwin(info_win);
	delwin(text_win);
	delwin(com_win);
	delwin(help_win);
	set_up_term();
	redraw();
	wrefresh(text_win);
}

static char item_alpha[] = "abcdefghijklmnopqrstuvwxyz0123456789 ";

int menu_op(struct menu_entries menu_list[])
{
	WINDOW *temp_win;
	int max_width, max_height;
	int x_off, y_off;
	int counter;
	int length;
	int input;
	int temp = 0;
	int list_size;
	int top_offset;		/* offset from top where menu items start */
	int vert_size;		/* vertical size for menu list item display */
	int off_start = 1;	/* offset from start of menu items to start display */


	/*
	 |	determine number and width of menu items
	 */

	list_size = 1;
	while (menu_list[list_size + 1].item_string != NULL)
		list_size++;
	max_width = 0;
	for (counter = 0; counter <= list_size; counter++)
	{
		if ((length = wcslen(menu_list[counter].item_string)) > max_width)
			max_width = length;
	}
	max_width += 3;
	max_width = max(max_width, wcslen(menu_cancel_msg));
	max_width = max(max_width, max(wcslen(more_above_str), wcslen(more_below_str)));
	max_width += 6;

	/*
	 |	make sure that window is large enough to handle menu
	 |	if not, print error message and return to calling function
	 */

	if (max_width > COLS)
	{
		wmove(com_win, 0, 0);
		werase(com_win);
		wprintw(com_win, menu_too_lrg_msg);
		wrefresh(com_win);
		clear_com_win = TRUE;
		return(0);
	}

	top_offset = 0;

	if (list_size > LINES)
	{
		max_height = LINES;
		if (max_height > 11)
			vert_size = max_height - 8;
		else
			vert_size = max_height;
	}
	else
	{
		vert_size = list_size;
		max_height = list_size;
	}

	if (LINES >= (vert_size + 8))
	{
		if (menu_list[0].argument != MENU_WARN)
			max_height = vert_size + 8;
		else
			max_height = vert_size + 7;
		top_offset = 4;
	}
	x_off = (COLS - max_width) / 2;
	y_off = (LINES - max_height - 1) / 2;
	temp_win = newwin(max_height, max_width, y_off, x_off);
	keypad(temp_win, TRUE);

	paint_menu(menu_list, max_width, max_height, list_size, top_offset, temp_win, off_start, vert_size);

	counter = 1;
	do
	{
		int keyt;
		if (off_start > 2)
			wmove(temp_win, (1 + counter + top_offset - off_start), 3);
		else
			wmove(temp_win, (counter + top_offset - off_start), 3);

		wrefresh(temp_win);
		keyt = wget_wch(temp_win, &in);
		input = in;
		if (keyt == ERR)
			exit(0);

		if (keyt == OK && isalnum(tolower(input)))
		{
			if (isalpha(tolower(input)))
			{
				temp = 1 + tolower(input) - 'a';
			}
			else if (isdigit(input))
			{
				temp = (2 + 'z' - 'a') + (input - '0');
			}

			if (temp <= list_size)
			{
				input = '\n';
				counter = temp;
			}
		}
		else if (keyt == OK)
		{
			switch (input)
			{
				case ' ':	/* space	*/
				case '\004':	/* ^d, down	*/
					counter++;
					if (counter > list_size)
						counter = 1;
					break;
				case '\010':	/* ^h, backspace*/
				case '\025':	/* ^u, up	*/
				case 127:	/* ^?, delete	*/
					counter--;
					if (counter == 0)
						counter = list_size;
					break;
				case '\033':	/* escape key	*/
					if (menu_list[0].argument != MENU_WARN)
						counter = 0;
					break;
				case '\014':	/* ^l       	*/
				case '\022':	/* ^r, redraw	*/
					paint_menu(menu_list, max_width, max_height, 
						list_size, top_offset, temp_win, 
						off_start, vert_size);
					break;
				default:
					break;
			}
		}
		else
		{
			switch (input)
			{
				case KEY_RIGHT:
				case KEY_DOWN:
					counter++;
					if (counter > list_size)
						counter = 1;
					break;
				case 127:	/* ^?, delete	*/
				case KEY_BACKSPACE:
				case KEY_LEFT:
				case KEY_UP:
					counter--;
					if (counter == 0)
						counter = list_size;
					break;
				case '\033':	/* escape key	*/
					if (menu_list[0].argument != MENU_WARN)
						counter = 0;
					break;
				case '\014':	/* ^l       	*/
				case '\022':	/* ^r, redraw	*/
					paint_menu(menu_list, max_width, max_height, 
						list_size, top_offset, temp_win, 
						off_start, vert_size);
					break;
				default:
					break;
			}
		}
	
		if (((list_size - off_start) >= (vert_size - 1)) && 
			(counter > (off_start + vert_size - 3)) && 
				(off_start > 1))
		{
			if (counter == list_size)
				off_start = (list_size - vert_size) + 2;
			else
				off_start++;

			paint_menu(menu_list, max_width, max_height, 
				   list_size, top_offset, temp_win, off_start, 
				   vert_size);
		}
		else if ((list_size != vert_size) && 
				(counter > (off_start + vert_size - 2)))
		{
			if (counter == list_size)
				off_start = 2 + (list_size - vert_size);
			else if (off_start == 1)
				off_start = 3;
			else
				off_start++;

			paint_menu(menu_list, max_width, max_height, 
				   list_size, top_offset, temp_win, off_start, 
				   vert_size);
		}
		else if (counter < off_start)
		{
			if (counter <= 2)
				off_start = 1;
			else
				off_start = counter;

			paint_menu(menu_list, max_width, max_height, 
				   list_size, top_offset, temp_win, off_start, 
				   vert_size);
		}
	}
	while ((input != '\r') && (input != '\n') && (counter != 0));

	werase(temp_win);
	wrefresh(temp_win);
	delwin(temp_win);

	if ((menu_list[counter].procedure != NULL) || 
	    (menu_list[counter].iprocedure != NULL) || 
	    (menu_list[counter].nprocedure != NULL))
	{
		if (menu_list[counter].argument != -1)
			(*menu_list[counter].iprocedure)(menu_list[counter].argument);
		else if (menu_list[counter].ptr_argument != NULL)
			(*menu_list[counter].procedure)(menu_list[counter].ptr_argument);
		else
			(*menu_list[counter].nprocedure)();
	}

	if (info_window)
		paint_info_win();
	redraw();

	return(counter);
}

void 
paint_menu(menu_list, max_width, max_height, list_size, top_offset, menu_win, 
	   off_start, vert_size)
struct menu_entries menu_list[];
int max_width, max_height, list_size, top_offset;
WINDOW *menu_win;
int off_start, vert_size;
{
	int counter, temp_int;

	werase(menu_win);

	/*
	 |	output top and bottom portions of menu box only if window 
	 |	large enough 
	 */

	if (max_height > vert_size)
	{
		wmove(menu_win, 1, 1);
		if (!nohighlight)
			wstandout(menu_win);
		waddch(menu_win, '+');
		for (counter = 0; counter < (max_width - 4); counter++)
			waddch(menu_win, '-');
		waddch(menu_win, '+');

		wmove(menu_win, (max_height - 2), 1);
		waddch(menu_win, '+');
		for (counter = 0; counter < (max_width - 4); counter++)
			waddch(menu_win, '-');
		waddch(menu_win, '+');
		wstandend(menu_win);
		wmove(menu_win, 2, 3);
		waddwstr(menu_win, menu_list[0].item_string);
		wmove(menu_win, (max_height - 3), 3);
		if (menu_list[0].argument != MENU_WARN)
			waddwstr(menu_win, menu_cancel_msg);
	}
	if (!nohighlight)
		wstandout(menu_win);

	for (counter = 0; counter < (vert_size + top_offset); counter++)
	{
		if (top_offset == 4)
		{
			temp_int = counter + 2;
		}
		else
			temp_int = counter;

		wmove(menu_win, temp_int, 1);
		waddch(menu_win, '|');
		wmove(menu_win, temp_int, (max_width - 2));
		waddch(menu_win, '|');
	}
	wstandend(menu_win);

	if (list_size > vert_size)
	{
		if (off_start >= 3)
		{
			temp_int = 1;
			wmove(menu_win, top_offset, 3);
			waddwstr(menu_win, more_above_str);
		}
		else
			temp_int = 0;

		for (counter = off_start; 
			((temp_int + counter - off_start) < (vert_size - 1));
				counter++)
		{
			wmove(menu_win, (top_offset + temp_int + 
						(counter - off_start)), 3);
			if (list_size > 1)
				wprintw(menu_win, "%c) ", item_alpha[min((counter - 1), max_alpha_char)]);
			waddwstr(menu_win, menu_list[counter].item_string);
		}

		wmove(menu_win, (top_offset + (vert_size - 1)), 3);

		if (counter == list_size)
		{
			if (list_size > 1)
				wprintw(menu_win, "%c) ", item_alpha[min((counter - 1), max_alpha_char)]);
			waddwstr(menu_win, menu_list[counter].item_string);
		}
		else
			waddwstr(menu_win, more_below_str);
	}
	else
	{
		for (counter = 1; counter <= list_size; counter++)
		{
			wmove(menu_win, (top_offset + counter - 1), 3);
			if (list_size > 1)
				wprintw(menu_win, "%c) ", item_alpha[min((counter - 1), max_alpha_char)]);
			waddwstr(menu_win, menu_list[counter].item_string);
		}
	}
}

void 
help()
{
	int counter, dummy;

	werase(help_win);
	clearok(help_win, TRUE);
	for (counter = 0; counter < 22; counter++)
	{
		wmove(help_win, counter, 0);
		waddwstr(help_win, (emacs_keys_mode) ? 
			emacs_help_text[counter] : help_text[counter]);
	}
	wrefresh(help_win);
	werase(com_win);
	wmove(com_win, 0, 0);
	waddwstr(com_win, press_any_key_msg);
	wrefresh(com_win);
	counter = wget_wch(com_win, &dummy);
	if (counter == ERR)
		exit(0);
	werase(com_win);
	wmove(com_win, 0, 0);
	werase(help_win);
	wrefresh(help_win);
	wrefresh(com_win);
	redraw();
}

void 
paint_info_win()
{
	int counter;

	if (!info_window)
		return;

	werase(info_win);
	for (counter = 0; counter < 5; counter++)
	{
		wmove(info_win, counter, 0);
		wclrtoeol(info_win);
		if (info_type == CONTROL_KEYS)
			waddwstr(info_win, (emacs_keys_mode) ? 
			  emacs_control_keys[counter] : control_keys[counter]);
		else if (info_type == COMMANDS)
			waddwstr(info_win, command_strings[counter]);
	}
	wmove(info_win, 5, 0);
	if (!nohighlight)
		wstandout(info_win);
	waddstr(info_win, "===============================================================================");
	wstandend(info_win);
	wrefresh(info_win);
}

void 
no_info_window()
{
	if (!info_window)
		return;
	delwin(info_win);
	delwin(text_win);
	info_window = FALSE;
	last_line = LINES - 2;
	text_win = newwin((LINES - 1), COLS, 0, 0);
	keypad(text_win, TRUE);
	idlok(text_win, TRUE);
	clearok(text_win, TRUE);
	midscreen(scr_vert, point);
	wrefresh(text_win);
	clear_com_win = TRUE;
}

void 
create_info_window()
{
	if (info_window)
		return;
	last_line = LINES - 8;
	delwin(text_win);
	text_win = newwin((LINES - 7), COLS, 6, 0);
	keypad(text_win, TRUE);
	idlok(text_win, TRUE);
	werase(text_win);
	info_window = TRUE;
	info_win = newwin(6, COLS, 0, 0);
	werase(info_win);
	info_type = CONTROL_KEYS;
	midscreen(min(scr_vert, last_line), point);
	clearok(info_win, TRUE);
	paint_info_win();
	wrefresh(text_win);
	clear_com_win = TRUE;
}

int 
file_op(arg)
int arg;
{
	char *string;
	int flag;

	if (arg == SAVE_FILE)
	{
	/*
	 |	changes made here should be reflected in ee_finish()
	 */

		if (in_file_name)
			flag = TRUE;
		else
			flag = FALSE;

		string = in_file_name;
//		if ((string == NULL) || (*string == '\0'))
//			string = get_string(save_file_name_prompt, TRUE);
		if ((string == NULL) || (*string == '\0'))
		{
			wmove(com_win, 0, 0);
			wprintw(com_win, file_not_saved_msg);
			wclrtoeol(com_win);
			wrefresh(com_win);
			clear_com_win = TRUE;
			return(0);
		}
		if (write_file(string, -1))
		{
			in_file_name = string;
			text_changes = FALSE;
		}
		else if (!flag)
			free(string);
	}
	return(0);
}

void 
leave_op()
{
	if (text_changes)
	{
		menu_op(leave_menu);
	}
	else
		quit(TRUE);
}

void 
redraw()
{
	if (info_window)
        {
                clearok(info_win, TRUE);
        	paint_info_win();
        }
        else
		clearok(text_win, TRUE);
	midscreen(scr_vert, point);
}

/*
 |	The following routines will "format" a paragraph (as defined by a 
 |	block of text with blank lines before and after the block).
 */

int 
Blank_Line(test_line)	/* test if line has any non-space characters	*/
struct text *test_line;
{
	wchar_t *line;
	int length;
	
	if (test_line == NULL)
		return(TRUE);

	length = 1;
	line = test_line->line;

	/*
	 |	To handle troff/nroff documents, consider a line with a 
	 |	period ('.') in the first column to be blank.  To handle mail 
	 |	messages with included text, consider a line with a '>' blank.
	 */

	if ((*line == '.') || (*line == '>'))
		return(TRUE);

	while (((*line == ' ') || (*line == '\t')) && (length < test_line->line_length))
	{
		length++;
		line++;
	}
	if (length != test_line->line_length)
		return(FALSE);
	else
		return(TRUE);
}

void 
echo_string(string)	/* echo the given string	*/
wchar_t *string;
{
	wchar_t *temp;
	int Counter;

		temp = string;
		while (*temp != '\0')
		{
			if (*temp == '\\')
			{
				temp++;
				if (*temp == 'n')
					putchar('\n');
				else if (*temp == 't')
					putchar('\t');
				else if (*temp == 'b')
					putchar('\b');
				else if (*temp == 'r')
					putchar('\r');
				else if (*temp == 'f')
					putchar('\f');
				else if ((*temp == 'e') || (*temp == 'E'))
					putchar('\033');	/* escape */
				else if (*temp == '\\')
					putchar('\\');
				else if (*temp == '\'')
					putchar('\'');
				else if ((*temp >= '0') && (*temp <= '9'))
				{
					Counter = 0;
					while ((*temp >= '0') && (*temp <= '9'))
					{
						Counter = (8 * Counter) + (*temp - '0');
						temp++;
					}
					putchar(Counter);
					temp--;
				}
				temp++;
			}
			else
			{
				putchar(*temp);
				temp++;
			}
		}

	fflush(stdout);
}

int
first_word_len(test_line)
struct text *test_line;
{
	int counter;
	wchar_t *pnt;

	if (test_line == NULL)
		return(0);

	pnt = test_line->line;
	if ((pnt == NULL) || (*pnt == '\0') || 
	    (*pnt == '.') || (*pnt == '>'))
		return(0);

	if ((*pnt == ' ') || (*pnt == '\t'))
	{
		pnt = next_word(pnt);
	}

	if (*pnt == '\0')
		return(0);

	counter = 0;
	while ((*pnt != '\0') && ((*pnt != ' ') && (*pnt != '\t')))
	{
		pnt++;
		counter++;
	}
	while ((*pnt != '\0') && ((*pnt == ' ') || (*pnt == '\t')))
	{
		pnt++;
		counter++;
	}
	return(counter);
}

wchar_t *
is_in_string(string, substring)	/* a strchr() look-alike for systems without
				   strchr() */
wchar_t * string, *substring;
{
	wchar_t *full, *sub;

	for (sub = substring; (sub != NULL) && (*sub != '\0'); sub++)
	{
		for (full = string; (full != NULL) && (*full != '\0'); 
				full++)
		{
			if (*sub == *full)
				return(full);
		}
	}
	return(NULL);
}

/*
 |	The following routine tests the input string against the list of 
 |	strings, to determine if the string is a unique match with one of the 
 |	valid values.
 */

int 
unique_test(string, list)
wchar_t *string;
wchar_t *list[];
{
	int counter;
	int num_match;
	int result;

	num_match = 0;
	counter = 0;
	while (list[counter] != NULL)
	{
		result = compare(string, list[counter], FALSE);
		if (result)
			num_match++;
		counter++;
	}
	return(num_match);
}

/*
 |	The following is to allow for using message catalogs which allow 
 |	the software to be 'localized', that is, to use different languages 
 |	all with the same binary.  For more information, see your system 
 |	documentation, or the X/Open Internationalization Guide.
 */

void 
strings_init()
{
	leave_menu[0].item_string  = L"leave menu";
	leave_menu[1].item_string  = L"save changes";
	leave_menu[2].item_string  = L"no save";
	search_menu[0].item_string = L"search menu";
	search_menu[1].item_string = L"search for ...";
	search_menu[2].item_string = L"search";
	main_menu[0].item_string  = L"main menu";
	main_menu[1].item_string  = L"leave editor";
	main_menu[2].item_string  = L"help";
	main_menu[3].item_string  = L"save file";
	main_menu[4].item_string  = L"redraw screen";
	main_menu[5].item_string  = L"search";
	help_text[0] = L"Control keys:                                                              "; 
	help_text[1] = L"^a ascii code           ^i tab                  ^r right                   ";
	help_text[2] = L"^b bottom of text       ^j newline              ^t top of text             ";
	help_text[3] = L"^c command              ^k delete char          ^u up                      ";
	help_text[4] = L"^d down                 ^l left                 ^v undelete word           ";
	help_text[5] = L"^e search prompt        ^m newline              ^w delete word             ";
	help_text[6] = L"^f undelete char        ^n next page            ^x search                  ";
	help_text[7] = L"^g begin of line        ^o end of line          ^y delete line             ";
	help_text[8] = L"^h backspace            ^p prev page            ^z undelete line           ";
	help_text[9] = L"^[ (escape) menu                                                           ";
	help_text[10] = L"                                                                           ";
	help_text[11] = L"Commands:                                                                  ";
	help_text[12] = L"help    : get this info                 file    : print file name          ";
	help_text[13] = L"read    : (disabled)                    char    : ascii code of char       ";
	help_text[14] = L"write   : (disabled)                    case    : case sensitive search    ";
	help_text[15] = L"exit    : leave and save                nocase  : case insensitive search  ";
	help_text[16] = L"quit    : leave, no save                !cmd    : (disabled)               ";
	help_text[17] = L"line    : display line #                0-9     : go to line \"#\"           ";
	help_text[18] = L"expand  : expand tabs                   noexpand: do not expand tabs         ";
	help_text[19] = L"                                                                             ";
	help_text[20] = L"  ee [+#] [-i] [-e] [-h] [file(s)]                                            ";
	help_text[21] = L"+# :go to line #  -i :no info window  -e : don't expand tabs  -h :no highlight";
	control_keys[0] = L"^[ (escape) menu  ^e search prompt  ^y delete line    ^u up     ^p prev page  ";
	control_keys[1] = L"^a ascii code     ^x search         ^z undelete line  ^d down   ^n next page  ";
	control_keys[2] = L"^b bottom of text ^g begin of line  ^w delete word    ^l left                 ";
	control_keys[3] = L"^t top of text    ^o end of line    ^v undelete word  ^r right                ";
	control_keys[4] = L"^c command        ^k delete char    ^f undelete char                          ";
	command_strings[0] = L"help : get help info  |file  : print file name         |line : print line # ";
	command_strings[1] = L"read : (disabled)     |char  : ascii code of char      |0-9 : go to line \"#\"";
	command_strings[2] = L"save: save changes    |case  : case sensitive search   |exit : leave and save ";
	command_strings[3] = L"!cmd : (disabled)     |nocase: ignore case in search   |quit : leave, no save";
	command_strings[4] = L"expand: expand tabs   |noexpand: do not expand tabs                           ";
	com_win_message = L"    press Escape (^[) for menu";
	no_file_string = "no file";
	ascii_code_str = L"ascii code: ";
	command_str = L"command: ";
	char_str = "character = %d";
	unkn_cmd_str = "unknown command \"%S\"";
	non_unique_cmd_msg = L"entered command is not unique";
	line_num_str = "line %d  ";
	line_len_str = "length = %d";
	current_file_str = "current file is \"%S\" ";
	usage0 = L"usage: %s [-i] [-e] [-h] [+line_number] [file(s)]\n";
	usage1 = L"       -i   turn off info window\n";
	usage2 = L"       -e   do not convert tabs to spaces\n";
	usage3 = L"       -h   do not use highlighting\n";
	file_is_dir_msg = "file \"%s\" is a directory";
	new_file_msg = "new file \"%s\"";
	cant_open_msg = "can't open \"%s\"";
	open_file_msg = L"file \"%s\", %d lines";
	file_read_fin_msg = "finished reading file \"%s\"";
	reading_file_msg = "reading file \"%s\"";
	read_only_msg = ", read only";
	file_read_lines_msg = "file \"%s\", %d lines";
	save_file_name_prompt = L"enter name of file: ";
	file_not_saved_msg = "no filename entered: file not saved";
	changes_made_prompt = L"changes have been made, are you sure? (y/n [n]) ";
	yes_char = L"y";
	file_exists_prompt = L"file already exists, overwrite? (y/n) [n] ";
	create_file_fail_msg = "unable to create file \"%s\"";
	writing_file_msg = "writing file \"%s\"";
	file_written_msg = "\"%s\" %d lines, %d characters";
	searching_msg = "           ...searching";
	str_not_found_msg = "string \"%S\" not found";
	search_prompt_str = L"search for: ";
	continue_msg = L"press return to continue ";
	menu_cancel_msg = L"press Esc to cancel";
	menu_size_err_msg = L"menu too large for window";
	press_any_key_msg = L"press any key to continue ";
	ON = L"ON";
	OFF = L"OFF";
	HELP = L"HELP";
	SAVE = L"SAVE";
	READ = L"READ";
	LINE = L"LINE";
	FILE_str = L"FILE";
	CHARACTER = L"CHARACTER";
	REDRAW = L"REDRAW";
	RESEQUENCE = L"RESEQUENCE";
	AUTHOR = L"AUTHOR";
	ee_VERSION = L"VERSION";
	CASE = L"CASE";
	NOCASE = L"NOCASE";
	EXPAND = L"EXPAND";
	NOEXPAND = L"NOEXPAND";
	Exit_string = L"EXIT";
	QUIT_string = L"QUIT";
	INFO = L"INFO";
	NOINFO = L"NOINFO";
	MARGINS = L"MARGINS";
	NOMARGINS = L"NOMARGINS";
	AUTOFORMAT = L"AUTOFORMAT";
	NOAUTOFORMAT = L"NOAUTOFORMAT";
	Echo = L"ECHO";
	PRINTCOMMAND = L"PRINTCOMMAND";
	RIGHTMARGIN = L"RIGHTMARGIN";
	HIGHLIGHT = L"HIGHLIGHT";
	NOHIGHLIGHT = L"NOHIGHLIGHT";
	EIGHTBIT = L"EIGHTBIT";
	NOEIGHTBIT = L"NOEIGHTBIT";
	/*
	 |	additions
	 */
	emacs_help_text[0] = help_text[0];
	emacs_help_text[1] = L"^a beginning of line    ^i tab                  ^r restore word            ";
	emacs_help_text[2] = L"^b back 1 char          ^j undel char           ^t top of text             ";
	emacs_help_text[3] = L"^c command              ^k delete line          ^u bottom of text          ";
	emacs_help_text[4] = L"^d delete char          ^l undelete line        ^v next page               ";
	emacs_help_text[5] = L"^e end of line          ^m newline              ^w delete word             ";
	emacs_help_text[6] = L"^f forward 1 char       ^n next line            ^x search                  ";
	emacs_help_text[7] = L"^g go back 1 page       ^o ascii char insert    ^y search prompt           ";
	emacs_help_text[8] = L"^h backspace            ^p prev line            ^z next word               ";
	emacs_help_text[9] = help_text[9];
	emacs_help_text[10] = help_text[10];
	emacs_help_text[11] = help_text[11];
	emacs_help_text[12] = help_text[12];
	emacs_help_text[13] = help_text[13];
	emacs_help_text[14] = help_text[14];
	emacs_help_text[15] = help_text[15];
	emacs_help_text[16] = help_text[16];
	emacs_help_text[17] = help_text[17];
	emacs_help_text[18] = help_text[18];
	emacs_help_text[19] = help_text[19];
	emacs_help_text[20] = help_text[20];
	emacs_help_text[21] = help_text[21];
	emacs_control_keys[0] = L"^[ (escape) menu  ^y search prompt  ^k delete line   ^p prev li   ^g prev page";
	emacs_control_keys[1] = L"^o ascii code     ^x search         ^l undelete line ^n next li   ^v next page";
	emacs_control_keys[2] = L"^u end of file    ^a begin of line  ^w delete word   ^b back 1 char           ";
	emacs_control_keys[3] = L"^t top of text    ^e end of line    ^r restore word  ^f forward 1 char        ";
	emacs_control_keys[4] = L"^c command        ^d delete char    ^j undelete char ^z next word              ";
	EMACS_string = L"EMACS";
	NOEMACS_string = L"NOEMACS";
	usage4 = L"       +#   put cursor at line #\n";
	menu_too_lrg_msg = "menu too large for window";
	more_above_str = L"^^more^^";
	more_below_str = L"VVmoreVV";
	

	commands[0] = HELP;
	commands[1] = SAVE;
	commands[2] = READ;
	commands[3] = LINE;
	commands[4] = FILE_str;
	commands[5] = REDRAW;
	commands[6] = RESEQUENCE;
	commands[7] = AUTHOR;
	commands[8] = ee_VERSION;
	commands[9] = CASE;
	commands[10] = NOCASE;
	commands[11] = EXPAND;
	commands[12] = NOEXPAND;
	commands[13] = Exit_string;
	commands[14] = QUIT_string;
	commands[15] = L"<";
	commands[16] = L">";
	commands[17] = L"!";
	commands[18] = L"0";
	commands[19] = L"1";
	commands[20] = L"2";
	commands[21] = L"3";
	commands[22] = L"4";
	commands[23] = L"5";
	commands[24] = L"6";
	commands[25] = L"7";
	commands[26] = L"8";
	commands[27] = L"9";
	commands[28] = CHARACTER;
	commands[29] = NULL;
}

