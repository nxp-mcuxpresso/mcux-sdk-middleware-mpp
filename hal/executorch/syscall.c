/* 
* File added to suppress linker warning - linked with addition of new SDK components
* Use syscall stubs for time functions
*/

#include <sys/time.h>
#include <errno.h>

int _gettimeofday(struct timeval *tv, void *tz)
{
    (void)tz;
    if (tv) {
        tv->tv_sec  = 0;
        tv->tv_usec = 0;
    }
    return 0;
}
