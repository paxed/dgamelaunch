/*
 * Copyright (c) 2004, Jilles Tjoelker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 * 2. Redistributions in binary form must reproduce the
 *    above copyright notice, this list of conditions and
 *    the following disclaimer in the documentation and/or
 *    other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#include <curses.h>

/* Assumes noecho(). */
/* As with getnstr(), maxlen does not include the '\0' character. */

int
mygetnstr(char *buf, int maxlen, int doecho)
{
    int c, i;

    i = 0;
    for (;;)
    {
	c = getch();
	if (c == 8 || c == 127 || c == KEY_BACKSPACE || c == KEY_DC ||
		c == KEY_LEFT)
	{
	    if (i > 0)
	    {
		i--;
		if (doecho)
		    addstr("\010 \010");
	    }
	    else
		beep();
	}
	else if (c == 21 || c == 24 || c == KEY_DL) /* ^U/^X */
	{
	    while (i > 0)
	    {
		i--;
		if (doecho)
		    addstr("\010 \010");
	    }
	}
	else if (c == 23) /* ^W */
	{
	    while (i > 0 && buf[i - 1] == ' ')
	    {
		i--;
		if (doecho)
		    addstr("\010 \010");
	    }
	    while (i > 0 && buf[i - 1] != ' ')
	    {
		i--;
		if (doecho)
		    addstr("\010 \010");
	    }
	}
	else if ((c >= ' ' && c <= '~') || (c >= 0xA0 && c <= 0xFF))
	{
	    if (i < maxlen)
	    {
		buf[i] = c;
		i++;
		if (doecho)
		    addch(c);
	    }
	    else
		beep();
	}
	else if (c == 10 || c == 13 || c == KEY_ENTER || c == KEY_RESIZE)
	    break;
	else if (c == ERR)
	{
	    buf[i] = 0;
	    return ERR;
	}
	else
	    beep();
    }
    buf[i] = 0;
    return OK;
}

/* vim:ts=8:cin:sw=4
 */
