/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <pcp/pmapi.h>
#include <pcp/impl.h>

#define SEMAPHORES /* comment this to NOT test semaphores */

#define IPC_N 4
#define NSEMS 8

int shm_list[IPC_N], shm_n=0, shmindom;
int sem_list[IPC_N * NSEMS], sem_n=0, semindom;
int semset_list[IPC_N], semset_n = 0, semsetindom;

static char *metrics[] = {
    "ipc.shm.nattch",
    "ipc.shm.segsz",
#ifdef SEMAPHORES
    "ipc.sem.nsems",
    "ipc.sem.ncnt",
    "ipc.sem.zcnt",
#endif
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
    char	*p;
    int		errflag = 0;
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
    int		iterations = 1;
    int		iter;
    pmResult	*result;
    pmDesc	desc;
    int		id;
    int		e;
#ifdef PCP_DEBUG
    static char	*debug = "[-D N] ";
#else
    static char	*debug = "";
#endif
    static char	*usage = "[-h hostname] [-n namespace] [-i iterations]";
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((c = getopt(argc, argv, "D:h:l:n:i:")) != EOF) {
	switch (c) {

#ifdef PCP_DEBUG
	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;
#endif

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
	fprintf(stderr, "Usage: %s %s%s\n", pmProgname, debug, usage);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }

    sts = pmNewContext(PM_CONTEXT_HOST, host);

    if (sts < 0) {
	printf("%s: Cannot connect to PMCD on host \"%s\": %s\n", pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    if (optind > argc)
	goto USAGE;

    memset(pmids, 0, sizeof(pmids));
    for (npmids=0; metrics[npmids]; npmids++) {
	if ((sts = pmLookupName(1, &metrics[npmids], &pmids[npmids])) < 0) {
	    fprintf(stderr, "%s: metric ``%s'' : %s\n", pmProgname, metrics[npmids], pmErrStr(sts));
	    exit(1);
	}
	fprintf(stderr, "pmid=%s <%s>\n", pmIDStr(pmids[npmids]), metrics[npmids]);
    }

    if ((e = pmLookupDesc(pmids[0], &desc)) < 0) {
        printf("pmLookupDesc: %s\n", pmErrStr(e));
        exit(1);
    }
    shmindom = desc.indom;

#ifdef SEMAPHORES
    if ((e = pmLookupDesc(pmids[2], &desc)) < 0) {
        printf("pmLookupDesc: %s\n", pmErrStr(e));
        exit(1);
    }
    semsetindom = desc.indom;
    if ((e = pmLookupDesc(pmids[3], &desc)) < 0) {
        printf("pmLookupDesc: %s\n", pmErrStr(e));
        exit(1);
    }
    semindom = desc.indom;
#endif

    fprintf(stderr, "shmindom=%s", pmInDomStr(shmindom));
#ifdef SEMAPHORES
    fprintf(stderr, " semsetindom=%s", pmInDomStr(semsetindom));
    fprintf(stderr, " semindom=%s", pmInDomStr(semindom));
#endif
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

#ifdef SEMAPHORES
	if ((id = semget(key++, NSEMS, IPC_CREAT|IPC_EXCL|0777)) >= 0) {
	    semset_list[semset_n++] = id;
	    fprintf(stderr, "SEMSET_%d\n", id);
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
#endif
    }

    pmDelProfile(shmindom, 0, NULL);
    pmAddProfile(shmindom, shm_n, shm_list);

#ifdef SEMAPHORES
    pmDelProfile(semsetindom, 0, NULL);
    pmAddProfile(semsetindom, semset_n, semset_list);
    pmDelProfile(semindom, 0, NULL);
    pmAddProfile(semindom, sem_n, sem_list);
#endif

    fprintf(stderr, "Single Metrics ...\n");
    for (j = 0; j < npmids; j++) {
	sts = pmFetch(1, &pmids[j], &result);
	if (sts < 0) {
	    fprintf(stderr, "%s: metric %s : %s\n", pmProgname, metrics[j], pmErrStr(sts));
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
		    pmProgname, iter, j, pmErrStr(sts));
		exit(1);
	    }
	    __pmDumpResult(stderr, result);
	    pmFreeResult(result);
	}
    }


    /* now test the err conditions */
    _force_err(shmindom, shm_list[0], pmids[0]);

#ifdef SEMAPHORES
    _force_err(semsetindom, semset_list[0], pmids[2]);
    _force_err(semindom, sem_list[0], pmids[3]);
#endif

CLEANUP:
    for (i=0; i < shm_n; i++) {
	if (shm_list[i] >= 0)
	    if (shmctl(shm_list[i], IPC_RMID, 0) < 0)
		perror("shmctl(IPC_RMID)");
    }
#ifdef SEMAPHORES
    for (i=0; i < sem_n; i += NSEMS) {
	if (sem_list[i] >= 0)
	    if (semctl(sem_list[i] >> 16, 0, IPC_RMID) < 0)
		perror("semctl(IPC_RMID)");
    }
#endif

    exit(0);
}
