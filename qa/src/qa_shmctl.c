#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>

int shmctl ( int shmid, int cmd, struct shmid_ds *buf )
{
    struct shm_info *i =(struct shm_info *) buf;
    memset(i, 0, sizeof(*i));
    i->shm_tot = 9;
    i->shm_rss = 100;
    i->shm_swp = 9000;
    return 0;
}
