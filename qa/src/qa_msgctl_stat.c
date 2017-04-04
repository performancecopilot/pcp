#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>

#if defined (__GLIBC__) && __GLIBC__ >= 2
# define KEY __key
#else
# define KEY key
#endif

int msgctl(int shmid, int cmd, struct msqid_ds *buf)
{
    struct msqid_ds *m = (struct msqid_ds *)buf;
    memset(m, 0, sizeof(*m));

    struct ipc_perm *ipcp = (struct ipc_perm *)(&(m->msg_perm));
    memset(ipcp, 0, sizeof(*ipcp));

    ipcp->KEY = 1111;
    ipcp->uid = 0;
    ipcp->mode = 888;
    m->__msg_cbytes = 666;
    m->msg_qnum = 777;
    return 0;
}
