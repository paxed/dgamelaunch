/* dgl-wall sends a message to all logged in users. If you specified a
 * configuration file to dgamelaunch, you must do so as well for dgl-wall,
 * or else the message won't be sent. */

#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "dgamelaunch.h"

int
main (int argc, char** argv)
{
  int c, i, len;
  char buf[82], *ptr = buf, *from = NULL;
  struct dg_game ** games = NULL;
  struct flock fl = { 0 };
  struct passwd* pw = getpwuid(getuid());
  FILE* spool = NULL;

  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  *ptr = '\0';

  while ((c = getopt(argc, argv, "f:F:")) != -1)
  {
    switch (c)
    {
      case 'f':
        config = strdup(optarg);
	break;
      case 'F':
	from = strdup(optarg);
	break;
      default:
	goto usage;
    }
  }
  
  if (from == NULL)
    from = pw->pw_name;

  while (optind < argc)
  {
    if (strlen(buf) != 0)
      strlcat (buf, " ", sizeof(buf));
    strlcat (buf, argv[optind++], sizeof(buf));
  }
  if (strlen(buf) > 80)
  {
    fprintf(stderr, "Error: Message is too long! (80 chars max)\n");
    return 1;
  }

  if (strlen(buf) == 0)
  {
usage:
    fprintf(stderr, "Usage: %s [-f config] [-F fromname] message\n", argv[0]);
    return 1;
  }

  create_config();

  if (chroot (myconfig->chroot))
  {
    perror("Couldn't change root directory");
    return 1;
  }

  if (chdir ("/"))
  {
    perror("Couldn't chdir to root directory");
    return 1;
  }
  
  games = populate_games (&len);

  if (len == 0)
  {
    fprintf(stderr, "Error: no one's logged in!\n");
    return 1;
  }

  for (i = 0; i < len; i++)
  {
    char* fname = NULL;
    size_t len;

    len = strlen(myconfig->spool) + strlen(games[i]->name) + 2;
    fname = malloc (len + 1);

    snprintf(fname, len, "%s/%s", myconfig->spool, games[i]->name);
    
    if ((spool = fopen(fname, "a")) == NULL)
    {
      fprintf (stderr, "Warning: couldn't open mailbox for %s.\n", games[i]->name);
      free (fname);
      continue;
    }

    while (fcntl (fileno(spool), F_SETLK, &fl) == -1)
    {
      if (errno != EAGAIN)
	fprintf (stderr, "Warning: fcntl errored out trying to lock mailbox for %s\n",
	    games[i]->name);
      sleep(1);
    }
   
    /* Now that we have the lock, begin the write */

    fprintf(spool, "%s:%s\n", from, buf);
    fclose(spool);
    free(fname);
  }

  puts("Message sent successfully.");
  return 0;
}

