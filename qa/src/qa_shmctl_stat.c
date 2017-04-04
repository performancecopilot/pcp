#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>

#if defined (__GLIBC__) && __GLIBC__ >= 2
# define KEY __key
#else
# define KEY key
#endif

int shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    struct shmid_ds *j = (struct shmid_ds *)buf;   
    memset(j, 0, sizeof(*j));

    struct ipc_perm *ipcp = (struct ipc_perm *)(&(j->shm_perm));
    memset(ipcp, 0, sizeof(*ipcp));

    ipcp->KEY = 1111;
    ipcp->uid = 0;
    ipcp->mode = 888;
    j->shm_segsz = 666;
    j->shm_nattch = 777;
    return 0;
}
