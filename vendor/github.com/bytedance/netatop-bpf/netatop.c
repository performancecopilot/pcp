// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
/* Copyright (c) 2021 Sartura
 * Based on minimal.c by Facebook */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/sem.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include "netatop.skel.h"
#include "netatop.h"
#include "server.h"
#include "deal.h"

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
	return vfprintf(stderr, format, args);
}

static volatile sig_atomic_t stop;
struct netatop_bpf *skel;
int semid;
int tgid_map_fd;
int tid_map_fd;
int nr_cpus;


int main(int argc, char **argv)
{
	/*
	** create the semaphore group and initialize it;
	** if it already exists, verify if a netatop bpf 
	** program is already running. And 
	** 
	*/
	struct sembuf		semincr = {0, +1, SEM_UNDO};	
	if ( (semid = semget(SEMAKEY, 0, 0)) >= 0)	// exists?
	{
		if ( semctl(semid, 0, GETVAL, 0) == 1)
		{
			fprintf(stderr, "Another netatop bpf program is already running!");
			exit(3);
		}
	}
	else
	{
		if ( (semid = semget(SEMAKEY, 2, 0600|IPC_CREAT|IPC_EXCL)) >= 0)
		{
			// Initialize the number of netatop bpf program
			(void) semctl(semid, 0, SETVAL, 0);
			// Initialize the number of atop Clients
			(void) semctl(semid, 1, SETVAL, 0);
		}
		else
		{
			perror("cannot create semaphore");
			exit(3);
		}
	}

	nr_cpus = libbpf_num_possible_cpus();

	libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
	/* Set up libbpf errors and debug info callback */
	libbpf_set_print(libbpf_print_fn);

	/* Open load and verify BPF application */
	skel = netatop_bpf__open_and_load();
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	tgid_map_fd = bpf_object__find_map_fd_by_name(skel->obj, "tgid_net_stat");
	// tid_map_fd = bpf_object__find_map_fd_by_name(skel->obj, "tid_net_stat");

	if ( fork() )
		exit(0);
	setsid();

	/*
	** raise semaphore to define a busy netatop
	*/
	if ( semop(semid, &semincr, 1) == -1)
	{
		printf("cannot increment semaphore\n");
		exit(3);
	}

	serv_listen();
}

void bpf_attach(struct netatop_bpf *skel)
{
	int err;
	err = netatop_bpf__attach(skel);
	if (err) {
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		cleanup(skel);
	}
}

void bpf_destroy(struct netatop_bpf *skel)
{
	if (!skel->skeleton)
		return;
	if (skel->skeleton->progs)
		bpf_object__detach_skeleton(skel->skeleton);
}

void cleanup(struct netatop_bpf *skel)
{
	netatop_bpf__destroy(skel);
}
