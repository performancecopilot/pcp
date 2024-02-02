#include <error.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <zlib.h>
#include <time.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include "server.h"
#include "netatop.h"
#include "deal.h"

int cleanup_and_go = 0;

/*
** signal catchers:
**    set flag to be verified in main loop to cleanup and terminate
*/
static void
cleanup_flag(int sig)
{
	cleanup_and_go = sig;
}

/*
* Create a server endpoint of a connection.
*/
int serv_listen()
{
    int            sock_fd, len, err, rval;
    struct sockaddr_un    un, cli_un;
    char *name = NETATOP_SOCKET;
    
    /* create a UNIX domain stream socket */
    if((sock_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        return(-1);
    unlink(name);    /* in case it already exists */

    /* fill in socket address structure */
    memset(&un, 0, sizeof(un));
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, name);
    len = offsetof(struct sockaddr_un, sun_path) + strlen(name);

    /* bind the name to the descriptor */
    if(bind(sock_fd, (struct sockaddr *)&un, len) < 0)
    {
        rval = -2;
        goto errout;
    }
    if(listen(sock_fd, 10) < 0)    /* tell kernel we're a server */
    {
        rval = -3;
        goto errout;
    }

    if(chmod(un.sun_path, 0777) < 0)
    {
        rval = -4;
        goto errout;
    }

    struct sigaction	sigcleanup;

	/*
 	** prepare cleanup signal handler
	*/
	memset(&sigcleanup, 0, sizeof sigcleanup);
	sigemptyset(&sigcleanup.sa_mask);
	sigcleanup.sa_handler	= cleanup_flag;
	(void) sigaction(SIGTERM, &sigcleanup, (struct sigaction *)0);

    /*
	** signal handling
	*/
	(void) signal(SIGHUP, SIG_IGN);

	(void) sigaction(SIGINT,  &sigcleanup, (struct sigaction *)0);
	(void) sigaction(SIGQUIT, &sigcleanup, (struct sigaction *)0);
	(void) sigaction(SIGTERM, &sigcleanup, (struct sigaction *)0);


    struct epoll_event ev, events[1000];
    int epoll_fd = epoll_create(10000);   /* create an epoll handle */
    ev.data.fd = sock_fd;   /* set fd associated with the event to be processed */
    ev.events = EPOLLIN;    /* set the type of event to handle */
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &ev); /* register epoll events */
   
    while(! cleanup_and_go)
    {
        int fd_num = epoll_wait(epoll_fd, events, 10000, 1000);
        for (int i = 0; i < fd_num; i++)
        {
            if (events[i].data.fd == sock_fd) // new client
            {
                len = sizeof(un);
                int conn_fd;
                if((conn_fd = accept(sock_fd, (struct sockaddr *)&cli_un, &len)) < 0)
                    return(-5);
                ev.data.fd = conn_fd;
                ev.events = EPOLLIN;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev);
                
                // Confirm the number of clients connected to netatop
                int num = semctl(semid, 1, GETVAL, 0);
                if ( num == 0)
                {
                    // If there is no client connection before new 
                    // connection, the bpf program will not be loaded.
                    bpf_attach(skel);
                    client_flag = conn_fd;
                }
                struct sembuf semincr = {1, +1, SEM_UNDO};
                if ( semop(semid, &semincr, 1) == -1)
                {
                    printf("cannot increment semaphore\n");
                    exit(3);
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                struct netpertask npt;
                int n = recv(events[i].data.fd, &npt, sizeof(npt), 0);
                if (n == 0) {
                    close(events[i].data.fd);

                    struct sembuf		semincr = {1, -1, SEM_UNDO};
                    if ( semop(semid, &semincr, 1) == -1)
                    {
                        printf("cannot increment semaphore\n");
                        exit(3);
                    }
                    if (NUMCLIENTS == 0)
                    {
                        /*
                        ** The last client connection is closed，
                        ** unload bpf program.
                        */
                        bpf_destroy(skel); 
                    }
                    continue;
                } 
                // deal(events[i].data.fd, &npt);
                deal_process(events[i].data.fd);
                npt.id = 0;
                send(events[i].data.fd, &npt, sizeof(npt), 0);
            }
        }
    }

    // delete NETATOP_SOCKET
    unlink(name);

errout:
    err = errno;
    close(sock_fd);
    close(epoll_fd);
    return(rval);
}