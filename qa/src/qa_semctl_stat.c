#include <stdarg.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#if defined (__GLIBC__) && __GLIBC__ >= 2
# define KEY __key
#else
# define KEY key
#endif

int vsemctl(int semid, int semnum, int cmd, struct semid_ds *s)
{
    memset(s, 0, sizeof(*s));

    struct ipc_perm *ipcp = (struct ipc_perm *)(&(s->sem_perm));
    memset(ipcp, 0, sizeof(*ipcp));

    ipcp->KEY = 1111;
    ipcp->uid = 0;
    ipcp->mode = 888;
    s->sem_nsems = 999;
    return 0;
}

int semctl(int semid, int semnum, int cmd, ...)
{
    struct semid_ds *sem;
    va_list arg;
    int sts;

    va_start(arg, cmd);
    sem = va_arg(arg, struct semid_ds *);
    sts = vsemctl(semid, semnum, cmd, sem);
    va_end(arg);

    return sts;
}
