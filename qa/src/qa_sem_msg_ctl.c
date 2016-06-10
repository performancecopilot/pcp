#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>
#include <stdarg.h>

#ifndef HAVE_UNION_SEMUN
union semun {
        int val;
        struct semid_ds *buf;
        unsigned short int *array;
        struct seminfo *__buf;
};
#endif

int semctl(int semid, int semnum, int cmd, ...)
{
    va_list pp;
    va_start(pp, cmd);
    union semun arg;
    arg = va_arg(pp, union semun);
    
    struct seminfo *i = (struct seminfo *)arg.array;
    memset(i, 0, sizeof(*i));
    i->semusz = 7;
    i->semaem = 8;
    return 0;
}

int msgctl(int msgid, int cmd, struct msqid_ds *buf)
{
    struct msginfo *i = (struct msginfo *)buf;

    memset(i, 0, sizeof(*i));
    i->msgpool = 9;
    i->msgmap = 100;
    i->msgtql = 9000;
    return 0;
}
