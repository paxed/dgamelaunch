/* IMPORTANT defines */

#ifndef __DGAMELAUNCH_H
#define __DGAMELAUNCH_H

struct dg_user
{
	char *username;
	char *email;
	char *env;
	char *password;
	int flags;
};

#define SHED_UID 5							/* the uid to shed privs to */
#define SHED_GID 60							/* the gid to shed privs to */
#define MAXUSERS 64000					/* solves some preallocation issues. */

#define LOC_CHROOT		"/var/lib/dgamelaunch/"
#define LOC_NETHACK		"/bin/nethack"
#define LOC_DGLROOT		"/dgldir/"
#define LOC_DGLDIR		LOC_DGLROOT "rcfiles/"
#define LOC_TTYRECDIR		LOC_DGLROOT "ttyrec/"
#define LOC_INPROGRESSDIR	LOC_DGLROOT "inprogress/"
#define LOC_SPOOLDIR		"/var/mail"
#define LOC_CANNED		"/dgl-default-rcfile"

#endif
