#ifndef __TTYREC_H__
#define __TTYREC_H__

#include <sys/time.h>
#include <sys/types.h>

typedef struct header
{
	struct timeval tv;
	size_t len;
}
Header;


#endif
