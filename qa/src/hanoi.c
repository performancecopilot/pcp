/*
 *  $Header: /usr/bench/src/musbus/RCS/hanoi.c,v 3.6 1993/07/02 21:17:11 kenj Exp $
 *
 * This code comes from the Musbus benchmark that was written by me
 * and released into the public domain circa 1984.
 * - Ken McDonell, Oct 2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PRINT 0
#define DISK 3
#define other(i,j) (6-(i+j))
int num[4];
long cnt;

void
mov(int n, int f, int t)
{
	int o;
	if(n == 1) {
		num[f]--;
		num[t]++;
		if(PRINT)printf("Move from %d to %d, result: A:%d B:%d C%d\n",
			f,t,num[1],num[2],num[3]);
		cnt++;
		return;
	}
	o = other(f,t);
	mov(n-1,f,o);
	mov(1,f,t);
	mov(n-1,o,t);
	return;
}

int
main(int argc, char **argv)
{
	int disk;
	disk  = DISK;
	if(argc > 1)disk = atoi(argv[1]);
	num[1] = disk;
	if(PRINT)printf("Start %d on A\n",disk);
	mov(disk,1,3);
	printf("For %d disks, %ld moves\n",disk,cnt);

	exit(0);
}
