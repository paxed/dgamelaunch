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

static const char rcsid[] = "$Id: nethackstub.c,v 1.4 2004/01/06 13:33:36 jilles Exp $";

#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
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
    exit(1);
}

void
checkmail()
{
    char *mailfile;
    char buf[256];
    int in, n;
    struct flock fl;
    
    mailfile = getenv("MAIL");
    if (getenv("SIMPLEMAIL"))
    {
	in = open(mailfile, O_RDWR);
	if (in != -1)
	{
	    fl.l_start = 0;
	    fl.l_len = 0;
	    fl.l_pid = getpid();
	    fl.l_type = F_WRLCK;
	    fl.l_whence = SEEK_SET;
	    if (fcntl(in, F_SETLK, &fl) != -1)
	    {
		while ((n = read(in, buf, sizeof buf)) > 0)
		{
		    write(STDOUT_FILENO, buf, n);
		}
#define S "End of mail - press return to unlock mailfile\n"
		write(STDOUT_FILENO, S, -1 + sizeof S);
#undef S
		read(STDIN_FILENO, buf, sizeof buf);
		ftruncate(in, (off_t)0);
		/* File will be unlocked automatically when closed */
	    }
	    else
	    {
#define S "Cannot lock mailfile\n"
		write(STDOUT_FILENO, S, -1 + sizeof S);
#undef S
	    }
	    close(in);
	}
	else
	{
#define S "Cannot open mailfile\n"
    write(STDOUT_FILENO, S, -1 + sizeof S);
#undef S
	}
    }
    else
    {
#define S "No SIMPLEMAIL\n"
    write(STDOUT_FILENO, S, -1 + sizeof S);
#undef S
    }
}

int
main(int argc, char *argv[])
{
    char buf[256];
    int showusage = 1, n, i;
    struct sigaction SA;

    /* Clear the screen for the benefit of dgamelaunch's check. */
#define S " \033[H\033[Jnethackstub started with arguments:\n"
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
#define S "i: close stdin - o: close stdout/stderr - m: check mail - q: quit\n"
	    write(STDOUT_FILENO, S, -1 + sizeof S);
#undef S
	}
	n = read(STDIN_FILENO, buf, sizeof buf);
	if (n == -1 && errno == EINTR)
	    continue;
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
		case 'm':
		    checkmail();
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
