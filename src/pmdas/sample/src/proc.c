/*
 * Fake process metrics and instance domain ... the goal here is to
 * have something manageable and deterministic that behaves like the
 * "proc" PMDA's instance domain.
 * - pid's are unique but wrap
 * - pid's come and go
 *
 * Copyright (c) 2021 Ken McDonell.  All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#include <pcp/pmapi.h>
#include <pcp/pmda.h>

static int ordinal;

typedef struct {
    int			pid;
    int			ordinal;	/* allocation number, -1 if deleted */
    struct timeval	tv;
    char		*iname;
    char		*exec;
} fakeproc_t;

static fakeproc_t	*proctab;

static char	*path_1[] = { "/etc", "/usr", "/usr/local", "/usr/opt" };
static int	n_1 = sizeof(path_1)/sizeof(char *);
static char	*path_2[] = { "", "/bin" };
static int	n_2 = sizeof(path_2)/sizeof(char *);
				/* clowns */
static char	*path_3[] = { "/bozo", "/bobo", "/jester", "/loko", "/pierrot", "/waldo", "/yobo", "/jojo", "/koko", "/lala" };
static int	n_3 = sizeof(path_3)/sizeof(char *);
/* selectors for path_1, path_2 and path_3 */
static int	k_1, k_2, k_3;

#define MAX_PROCS 20
#define MAX_PID 999

/*
 * We need a simple PRNG here that is deterministic after "reset",
 * so we cannot use a libc function because that may be called
 * non-deterministically elsewhere in the PMDA code.
 *
 * This is not very random, but suffices here.
 */

static __uint32_t seed = 1;

static void
_srand(__uint32_t s)
{
    seed = s;
}

static
double _rand(void)
{
    seed = (1103515245 * seed + 12345) % (1<<31);
    return (double)seed / (~(1<<31));
}


void
proc_reset(pmdaIndom *idp)
{
    if (idp->it_set != NULL) {
	free(idp->it_set);
	idp->it_set = NULL;
    }
    idp->it_numinst = 0;
    _srand(1);
}

static int
next_pid(pmdaIndom *idp)
{
    int		i;
    static int	next = 0;

    if (idp == NULL) {
	next = 0;
	return 0;
    }

    for ( ; ; ) {
	next++;
	if (next > MAX_PID)
	    next = 1;
	for (i = 0; i < idp->it_numinst; i++) {
	    if (next == idp->it_set[i].i_inst)
		break;
	}
	if (i == idp->it_numinst) {
	    /* all good, no dup */
	    return next;
	}
    }
}

int
proc_redo_indom(pmdaIndom *idp)
{
    int			i;
    int			t;
    int			m;
    static int		len;
    int			sts;

    if (idp->it_set == NULL) {
	/* first time */
	if ((idp->it_set = (pmdaInstid *)malloc(MAX_PROCS*sizeof(pmdaInstid))) == NULL)
	    return -oserror();
	if ((proctab = (fakeproc_t *)malloc(MAX_PROCS*sizeof(fakeproc_t))) == NULL) {
	    proc_reset(idp);
	    return -oserror();
	}
	/* for the proctab[].iname we do max length alloc and snprintf in there */
	len = 6;	/* "XXXX " + NUL */
	for (m = 0, i = 0; i < n_1; i++) {
	    if ((t = strlen(path_1[i])) > m)
		m = t;
	}
	len += m;
	for (m = 0, i = 0; i < n_2; i++) {
	    if ((t = strlen(path_2[i])) > m)
		m = t;
	}
	len += m;
	for (m = 0, i = 0; i < n_3; i++) {
	    if ((t = strlen(path_3[i])) > m)
		m = t;
	}
	len += m;
	for (i = 0; i < MAX_PROCS; i++) {
	    if ((proctab[i].iname = (char *)malloc(len)) == NULL) {
		sts = -oserror();
		i--;
		while (i >= 0) {
		    free(proctab[i].iname);
		    i--;
		}
		free(proctab);
		proc_reset(idp);
		return sts;
	    }
	    proctab[i].ordinal = -1;
	}
    }
    if (idp->it_numinst == 0) {
	/* initial state or reset */
	k_1 = k_2 = k_3 = 0;
	ordinal = 1;
	next_pid(NULL);
	/* the fixed ones */
	for (i = 0; i < 4; i++) {
	    proctab[i].pid = idp->it_set[i].i_inst = next_pid(idp);
	    proctab[i].ordinal = ordinal++;
	    gettimeofday(&proctab[i].tv, NULL);
	    if (i == 0)
		snprintf(proctab[i].iname, len, "%04d %s", 1, "init");
	    else {
		snprintf(proctab[i].iname, len, "%04d %s%s%s", proctab[i].ordinal,
		    path_1[k_1], path_2[k_2], path_3[k_3]);
		k_1 = (k_1 + 1) % n_1;
		k_2 = (k_2 + 1) % n_2;
		k_3 = (k_3 + 1) % n_3;
	    }
	    proctab[i].exec = rindex(proctab[i].iname, '/');
	    if (proctab[i].exec == NULL) {
		/* no embedded slash */
		proctab[i].exec = proctab[i].iname;
	    }
	    else
		proctab[i].exec++;
	    idp->it_set[i].i_name = proctab[i].iname;
	    idp->it_numinst++;
	}
    }
    else {
	/*
	 * first 4 instances are fixed ... then cull with p(0.075)
	 */
	int	j;
	for (i = 4; i < MAX_PROCS; i++) {
	    if (proctab[i].ordinal == -1)
		continue;
	    if (_rand() < 0.075) {
		/* delete this one */
		for (j = 0; j < idp->it_numinst; j++) {
		    if (idp->it_set[j].i_inst == proctab[i].ordinal) {
			while (j < idp->it_numinst-1) {
			    idp->it_set[j].i_inst = idp->it_set[j+1].i_inst;
			    idp->it_set[j].i_name = idp->it_set[j+1].i_name;
			    j++;
			}
			idp->it_numinst--;
			break;
		    }
		}
		proctab[i].ordinal = -1;
	    }
	}
	/* add new ones into empty slots with p(0.075) */
	for (i = 4; i < MAX_PROCS; i++) {
	    if (proctab[i].ordinal != -1)
		continue;
	    if (_rand() < 0.075) {
		j = idp->it_numinst;
		proctab[i].pid = idp->it_set[j].i_inst = next_pid(idp);
		proctab[i].ordinal = ordinal++;
		gettimeofday(&proctab[i].tv, NULL);
		snprintf(proctab[i].iname, len, "%04d %s%s%s", proctab[i].ordinal,
		    path_1[k_1], path_2[k_2], path_3[k_3]);
		k_1 = (k_1 + 1) % n_1;
		k_2 = (k_2 + 1) % n_2;
		k_3 = (k_3 + 1) % n_3;
		proctab[i].exec = rindex(proctab[i].iname, '/');
		proctab[i].exec++;
		idp->it_set[j].i_name = proctab[i].iname;
		idp->it_numinst++;
	    }
	}
    }

    return 0;
}

int
proc_get_ordinal(int inst)
{
    int		i;
    for (i = 0; i < MAX_PROCS; i++) {
	if (proctab[i].ordinal != -1 && proctab[i].pid == inst)
	    return proctab[i].ordinal;
    }
    return 0;
}

char
*proc_get_exec(int inst)
{
    int		i;
    for (i = 0; i < MAX_PROCS; i++) {
	if (proctab[i].ordinal != -1 && proctab[i].pid == inst)
	    return proctab[i].exec;
    }
    return "botch";
}

__uint64_t
proc_get_time(int inst)
{
    int			i;
    struct timeval	tv;
    for (i = 0; i < MAX_PROCS; i++) {
	if (proctab[i].ordinal != -1 && proctab[i].pid == inst) {
	    gettimeofday(&tv, NULL);
	    /* value is msec */
	    return (__uint64_t)(pmtimevalSub(&tv, &proctab[i].tv) * 1000);
	}
    }
    return 0;
}


