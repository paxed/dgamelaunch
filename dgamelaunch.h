/* IMPORTANT defines */

#ifndef __DGAMELAUNCH_H
#define __DGAMELAUNCH_H

struct dg_user
{
	char* username;
	char* email;
	char* env;
	char* password;
	int flags;
};

#define SHED_UID 1031						/* the uid to shed privs to */
#define SHED_GID 1031						/* the gid to shed privs to */
#define MAXUSERS 64000					/* solves some preallocation issues. */

#define LOC_CHROOT "/var/lib/dgamelaunch/"
#define LOC_NETHACK "/bin/nethack"
#define LOC_DGLDIR "/dgldir/rcfiles/"
#define LOC_TTYRECDIR "/dgldir/ttyrec/"
#define LOC_INPROGRESSDIR "/dgldir/inprogress/"
#define LOC_SPOOLDIR "/var/mail"
#define LOC_CANNED "/dgl-default-rcfile"

#endif
