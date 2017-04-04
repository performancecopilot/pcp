/*
** Copyright (C) 2015 Red Hat.
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2, or (at your option) any
** later version.
**
** This program is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU General Public License for more details.
*/

#include <pcp/pmapi.h>
#include "atop.h"

/*
** Stub functions, disabling functionality that we're not supporting from
** the original atop (not necessarily because there's anything wrong with
** it, more because it needs to be re-thought in a distributed PCP world.
**
** e.g. netatopd and acctproc code would need to exist in PCP PMDAs and be
** accesses via PMAPI metric interfaces.
*/

void
netatop_ipopen(void)
{
}

void
netatop_probe(void)
{
}

void
netatop_signoff(void)
{
}

void
netatop_gettask(pid_t pid, char c, struct tstat *tstat)
{
	(void)pid;
	(void)c;
	(void)tstat;
}

unsigned int
netatop_exitstore(void)
{
	return 0;
}

void
netatop_exiterase(void)
{
}

void
netatop_exithash(char p)
{
	(void)p;
}

void
netatop_exitfind(unsigned long x, struct tstat *a, struct tstat *b)
{
	(void)x;
	(void)a;
	(void)b;
}

int
acctswon(void)
{
	return 0;
}

void
acctswoff(void)
{
}

unsigned long 
acctprocnt(void)
{
	return 0;
}

int
acctphotoproc(struct tstat *tstat, int isproc)
{
	(void)tstat;
	(void)isproc;
	return 0;
}

void
acctrepos(unsigned int a)
{
	(void)a;
}

void
do_pacctdir(char *tagname, char *tagvalue)
{
	(void)tagname;
	(void)tagvalue;
}
