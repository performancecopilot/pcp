#include <stdarg.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#if defined (__GLIBC__) && __GLIBC__ >= 2
# define KEY __key
#else
# define KEY key
#endif

int vsemctl(int semid, int semnum, int cmd, void *buf)
{

    if (cmd == SEM_INFO) {
	struct seminfo	*s = buf;
	memset(s, 0, sizeof(*s));
	s->semusz = 2;		/* one semaphore set currently */
	s->semaem = 3;		/* total number of semaphores currently */

	return 1;
    }
    else {
	struct semid_ds *s = buf;
	struct ipc_perm *ipcp = (struct ipc_perm *)(&(s->sem_perm));

	memset(s, 0, sizeof(*s));

	memset(ipcp, 0, sizeof(*ipcp));

	ipcp->KEY = 1111;
	ipcp->uid = 0;
	ipcp->mode = 888;
	s->sem_nsems = 999;

	return 0;
    }
}

int semctl(int semid, int semnum, int cmd, ...)
{
    void *buf;
    va_list arg;
    int sts;

    va_start(arg, cmd);
    buf = va_arg(arg, void *);
    sts = vsemctl(semid, semnum, cmd, buf);
    va_end(arg);

    return sts;
}
