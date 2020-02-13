/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <pcp/pmapi.h>
#include "libpcp.h"

#define IPC_N 4
#define NSEMS 8

int shm_list[IPC_N], shm_n=0, shmindom;
int sem_list[IPC_N * NSEMS], sem_n=0, semindom;

static char *metrics[] = {
    "ipc.shm.nattch",
    "ipc.shm.segsz",
    "ipc.sem.nsems",
NULL };

static int npmids;
static pmID pmids[sizeof(metrics) / sizeof(metrics[0])];

static void
_force_err_noprofile(pmInDom indom, pmID pmid)
{
    pmResult	*result;
    int		sts;

    pmAddProfile(indom, 0, NULL);
    sts = pmFetch(1, &pmid, &result);
    fprintf(stderr, "\n\ndeliberate error check (no explicit profile) : %s\n", pmErrStr(sts));
    __pmDumpResult(stderr, result);
    pmFreeResult(result);
}

static void
_force_err_unknown_inst(pmInDom indom, pmID pmid)
{
    pmResult	*result;
    int		sts;
    int		i;

    pmDelProfile(indom, 0, NULL);
    i = -3;
    pmAddProfile(indom, 1, &i);
    sts = pmFetch(1, &pmid, &result);
    fprintf(stderr, "\n\ndeliberate error check (1 unknown instance) : %s\n", pmErrStr(sts));
    __pmDumpResult(stderr, result);
    pmFreeResult(result);
}

static void
_force_err_unknown_and_known_inst(pmInDom indom, int inst, pmID pmid)
{
    pmResult	*result;
    int		sts;
    int		i;

    pmDelProfile(indom, 0, NULL);
    i = -3; pmAddProfile(indom, 1, &i);
    pmAddProfile(indom, 1, &inst);
    sts = pmFetch(1, &pmid, &result);
    fprintf(stderr, "\n\ndeliberate error check (1 unknown instance + 1 known) : %s\n", pmErrStr(sts));
    __pmDumpResult(stderr, result);
    pmFreeResult(result);
}

static void
_force_err(pmInDom indom, int inst, pmID pmid)
{
    _force_err_noprofile(indom, pmid);
    _force_err_unknown_inst(indom, pmid);
    _force_err_unknown_and_known_inst(indom, inst, pmid);
}

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		j;
    key_t	key;
    int		sts;
    int		errflag = 0;
    char	*host = "local:";
    char	*namespace = PM_NS_DEFAULT;
    int		iterations = 1;
    int		iter;
    pmResult	*result;
    pmDesc	desc;
    int		id;
    int		e;
    static char	*usage = "[-D debugspec] [-h hostname] [-n namespace] [-i iterations]";
    extern char	*optarg;
    extern int	optind;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:l:n:i:")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'h':	/* hostname for PMCD to contact */
	    host = optarg;
	    break;

	case 'i':	/* iterations */
	    iterations = atoi(optarg);
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
USAGE:
	fprintf(stderr, "Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), namespace, pmErrStr(sts));
	exit(1);
    }

    sts = pmNewContext(PM_CONTEXT_HOST, host);

    if (sts < 0) {
	printf("%s: Cannot connect to PMCD on host \"%s\": %s\n", pmGetProgname(), host, pmErrStr(sts));
	exit(1);
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    if (optind > argc)
	goto USAGE;

    memset(pmids, 0, sizeof(pmids));
    for (npmids=0; metrics[npmids]; npmids++) {
	if ((sts = pmLookupName(1, &metrics[npmids], &pmids[npmids])) < 0) {
	    fprintf(stderr, "%s: metric ``%s'' : %s\n", pmGetProgname(), metrics[npmids], pmErrStr(sts));
	    exit(1);
	}
	fprintf(stderr, "pmid=%s <%s>\n", pmIDStr(pmids[npmids]), metrics[npmids]);
    }

    if ((e = pmLookupDesc(pmids[0], &desc)) < 0) {
        printf("pmLookupDesc: %s\n", pmErrStr(e));
        exit(1);
    }
    shmindom = desc.indom;

    if ((e = pmLookupDesc(pmids[2], &desc)) < 0) {
        printf("pmLookupDesc: %s\n", pmErrStr(e));
        exit(1);
    }
    semindom = desc.indom;

    fprintf(stderr, "shmindom=%s", pmInDomStr(shmindom));
    fprintf(stderr, "semindom=%s", pmInDomStr(semindom));
    fputc('\n', stderr);

    key = (key_t)0xabcd0000;
    for (i=0; i < IPC_N; i++) {
	if ((id = shmget(key++, 4096, IPC_CREAT|IPC_EXCL|0777)) >= 0) {
	    shm_list[shm_n++] = id;
	    fprintf(stderr, "SHMID_%d\n", id);
	}
	else {
	    perror("shmget");
	    goto CLEANUP;
	}

	if ((id = semget(key++, NSEMS, IPC_CREAT|IPC_EXCL|0777)) >= 0) {
	    for (j=0; j < NSEMS; j++) {
		sem_list[sem_n++] = (id << 16) | j;
		fprintf(stderr, "SEMID_%d.%d ", id, j);
	    }
	    fputc('\n', stderr);
	}
	else {
	    perror("semget");
	    goto CLEANUP;
	}
    }

    pmDelProfile(shmindom, 0, NULL);
    pmAddProfile(shmindom, shm_n, shm_list);

    pmDelProfile(semindom, 0, NULL);
    pmAddProfile(semindom, sem_n, sem_list);

    fprintf(stderr, "Single Metrics ...\n");
    for (j = 0; j < npmids; j++) {
	sts = pmFetch(1, &pmids[j], &result);
	if (sts < 0) {
	    fprintf(stderr, "%s: metric %s : %s\n", pmGetProgname(), metrics[j], pmErrStr(sts));
	    exit(1);
	}
	__pmDumpResult(stderr, result);
	pmFreeResult(result);
    }

    fprintf(stderr, "Cascading Sets of Metrics ...\n");
    for (iter=0; iter < iterations; iter++) {
	fprintf(stderr, "Iteration: %d\n", iter);
	for (j = 0; j < npmids; j++) {
	    sts = pmFetch(j+1, pmids, &result);
	    if (sts < 0) {
		fprintf(stderr, "%s: iteration %d cascade %d : %s\n",
		    pmGetProgname(), iter, j, pmErrStr(sts));
		exit(1);
	    }
	    __pmDumpResult(stderr, result);
	    pmFreeResult(result);
	}
    }


    /* now test the err conditions */
    _force_err(shmindom, shm_list[0], pmids[0]);
    _force_err(semindom, sem_list[0], pmids[2]);

CLEANUP:
    for (i=0; i < shm_n; i++) {
	if (shm_list[i] >= 0)
	    if (shmctl(shm_list[i], IPC_RMID, 0) < 0)
		perror("shmctl(IPC_RMID)");
    }
    for (i=0; i < sem_n; i += NSEMS) {
	if (sem_list[i] >= 0)
	    if (semctl(sem_list[i] >> 16, 0, IPC_RMID) < 0)
		perror("semctl(IPC_RMID)");
    }

    exit(0);
}
