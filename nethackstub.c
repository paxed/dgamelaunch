/* nethackstub.c
 * Copyright (c) 2004 Jilles Tjoelker <jilles@stack.nl>
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * This program can be used instead of nethack to test dgamelaunch.
 */

static const char rcsid[] = "$Id: nethackstub.c,v 1.3 2004/01/05 17:44:30 joshk Exp $";

#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>

void
sighup(int sig)
{
    (void)sig;
#define S "SIGHUP received.\n"
    write(STDOUT_FILENO, S, -1 + sizeof S);
#undef S
}

void
sigterm(int sig)
{
    (void)sig;
#define S "SIGTERM received.\n"
    write(STDOUT_FILENO, S, -1 + sizeof S);
#undef S
}

int
main(int argc, char *argv[])
{
    char buf[256];
    int showusage = 1, n, i;
    struct sigaction SA;

#define S "nethackstub started with arguments:\n"
    write(STDOUT_FILENO, S, -1 + sizeof S);
#undef S
    for (i = 1; i < argc; i++)
    {
	write(STDOUT_FILENO, argv[i], strlen(argv[i]));
#define S "\n"
	write(STDOUT_FILENO, S, -1 + sizeof S);
#undef S
    }

    sigemptyset(&SA.sa_mask);
    SA.sa_flags = 0;
    SA.sa_handler = sighup;
    sigaction(SIGHUP, &SA, NULL);
    SA.sa_handler = sigterm;
    sigaction(SIGTERM, &SA, NULL);

    for (;;)
    {
	if (showusage)
	{
#define S "i: close stdin - o: close stdout/stderr - q: quit\n"
	    write(STDOUT_FILENO, S, -1 + sizeof S);
#undef S
	}
	n = read(STDIN_FILENO, buf, sizeof buf);
	if (n <= 0)
	    break;
	for (i = 0; i < n; i++)
	{
	    switch (tolower(buf[i]))
	    {
		case 'i':
		    close(STDIN_FILENO);
		    break;
		case 'o':
		    close(STDOUT_FILENO);
		    close(STDERR_FILENO);
		    break;
		case 'q':
		    return 0;
		    break;
		case '\r':
		case '\n':
		case ' ':
		    break;
		default:
		    showusage = 1;
	    }
	}
    }

    for (;;)
	pause();

    return 0;
}

/* vim:ts=8:cin:sw=4
 */
