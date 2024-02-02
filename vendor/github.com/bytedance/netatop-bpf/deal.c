#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "netatop.h"
#include "deal.h"

// void deal(int fd, struct netpertask *npt)
// {
//     if (npt->type == 'g') { 
//         deal_process(fd);
//     } 
//     // else if (npt->type == 't'){
//     //     if (bpf_map_lookup_elem(tid_map_fd, &pid, stats) != 0) {
//     //         return;
//     //     }
//     // } 
// }
/*
 * the number of exited processes in the elapsed interval
*/
int exited_num = 0;
unsigned long long exited_pid[40960];
int client_flag;

void deal_process(int fd)
{
    struct netpertask npt;
    unsigned long long lookup_key, next_key;
    

    struct taskcount *stats = calloc(nr_cpus, sizeof(struct taskcount));
	lookup_key = 1;

    /*
     * delete exited process
    */
    for (int i = 0; i < exited_num; i++) {
         bpf_map_delete_elem(tgid_map_fd, &exited_pid[i]);
    }
    exited_num = 0;
    while(bpf_map_get_next_key(tgid_map_fd, &lookup_key, &next_key) == 0) {
        lookup_key = next_key;
        bpf_map_lookup_elem(tgid_map_fd, &next_key, stats);
        if (kill(next_key, 0) && errno == ESRCH && fd == client_flag ) {
            exited_pid[exited_num++] = next_key;
        }

        // printf("%-6d %-16lld %-6lld %-16lld %-6lld\n", next_key.pid, task_count_process.tcprcvpacks, task_count_process.tcprcvbytes, task_count_process.udprcvpacks, task_count_process.udprcvbytes);

        struct taskcount data = {
            .tcpsndpacks = 0,
            .tcpsndbytes = 0,
            .tcprcvpacks = 0,
            .tcprcvbytes = 0,
            .udpsndpacks = 0,
            .udpsndbytes = 0,
            .udprcvpacks = 0,
            .udprcvbytes = 0,
        };

        for (int i = 0; i < nr_cpus; i++) {
            data.tcpsndpacks += stats[i].tcpsndpacks;
            data.tcpsndbytes += stats[i].tcpsndbytes;
            data.tcprcvpacks += stats[i].tcprcvpacks;
            data.tcprcvbytes += stats[i].tcprcvbytes;
            data.udpsndpacks += stats[i].udpsndpacks;
            data.udpsndbytes += stats[i].udpsndbytes;
            data.udprcvpacks += stats[i].udprcvpacks;
            data.udprcvbytes += stats[i].udprcvbytes;
        } 
        // 如果字节数超过2^64?
        // if (data.tcpsndbytes >  || data.tcprcvbytes || data.udpsndbytes|| data.udprcvbytes) 
        // 	bpf_map_delete_elem(tgid_map_fd, &next_key);            
        
        struct netpertask npt = {
            .id = next_key,
            .tc = data
        };

        send(fd, &npt, sizeof(npt), 0);
    }
    free(stats);
}
