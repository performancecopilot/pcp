/*
 * Copyright (c) 2011-2021 Ken McDonell.  All Rights Reserved.
 *
 * exercise multi-threaded pmProcess*() services
 */

#include <stdio.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include <pthread.h>
#include "libpcp.h"

#ifndef HAVE_PTHREAD_BARRIER_T
#include "pthread_barrier.h"
#endif

static pthread_barrier_t barrier;
static pthread_mutex_t mymutex = PTHREAD_MUTEX_INITIALIZER;

struct {
    int		iter;
    char	*basename;
} ctl = { 10, NULL };

#define MAXCMD MAXPATHLEN+512

/*
 * This one uses _pmProcessPipe() to execute expr(1) to compute
 * the Fibonacci series ...
 */
static void *
func_A(void *arg)
{
    int			iam = *((int *)arg);
    int			i;
    int			j;
    char		c;
    int			lastc;
    int			sts;
    int			i0 = 0;		/* Fibonacci #(n-1) */
    int			i1 = 1;		/* Fibonacci #n */
    int			f;		/* func_A output */
    FILE		*fp;		/* for pipe */
    int			fd;		/* for answer */
    __pmExecCtl_t	*argp = NULL;
    char		out[MAXPATHLEN+1];
    char		path[MAXPATHLEN+1];
    char		answer[MAXPATHLEN+1];
    char		strbuf[PM_MAXERRMSGLEN];
    char		cmd[MAXCMD+1];

    pthread_barrier_wait(&barrier);

    snprintf(out, MAXPATHLEN, "%s.%d", ctl.basename, iam);

    if ((f = open(out, O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0) {
	perror("func_A open");
	pthread_exit("botch A.1");
    }

    snprintf(answer, MAXPATHLEN, "%s.out.%d", ctl.basename, iam);
    if (pmDebugOptions.appl0)
	fprintf(stderr, "[tid %d] answer=%s\n", iam, answer);
    (void)unlink(answer);
    snprintf(cmd, MAXCMD, "which expr >%s", answer);
    pthread_mutex_lock(&mymutex);
    if ((sts = system(cmd)) != 0) {
	fprintf(stderr, "func_A: system() -> %d\n", sts);
	pthread_exit("botch A.99");
    }
    pthread_mutex_unlock(&mymutex);
    if ((fd = open(answer, O_RDONLY)) < 0) {
	perror("func_A open .out which");
	pthread_exit("botch A.98");
    }
    i = 0;
    while (read(fd, &c, 1) == 1) {
	if (c == '\n') {
	    path[i] = '\0';
	    break;
	}
	path[i++] = c;
    }
    close(fd);
    if (pmDebugOptions.appl0)
	fprintf(stderr, "[tid %d] path=%s\n", iam, path);

    for (i = 0; i < ctl.iter; i++) {
	if ((sts = __pmProcessAddArg(&argp, path)) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_A: __pmProcessAddArg() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.2");
	}
	pmsprintf(strbuf, PM_MAXLABELJSONLEN, "%d", i0);
	if ((sts = __pmProcessAddArg(&argp, strbuf)) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_A: __pmProcessAddArg() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.3");
	}
	if ((sts = __pmProcessAddArg(&argp, "+")) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_A: __pmProcessAddArg() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.4");
	}
	pmsprintf(strbuf, PM_MAXLABELJSONLEN, "%d", i1);
	if ((sts = __pmProcessAddArg(&argp, strbuf)) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_A: __pmProcessAddArg() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.5");
	}
	if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_ALL, &fp)) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_A: __pmProcessPipe() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.6");
	}

	sts = write(f, "Answer: ", strlen("Answer: "));
	if (sts != strlen("Answer: "))
	    fprintf(stderr, "write: botch #1: %d != %zd\n", sts, strlen("Answer: "));
	lastc = EOF;
	while (read(fileno(fp), &c, 1) == 1) {
	    sts = write(f, &c, 1);
	    if (sts != 1)
		fprintf(stderr, "write: botch #2: %d != %d\n", sts, 1);
	    lastc = c;
	}
	if (lastc != '\n') {
	    sts = write(f, "\n", 1);
	    if (sts != 1)
		fprintf(stderr, "write: botch #3: %d != %d\n", sts, 1);
	}
	if ((sts = __pmProcessPipeClose(fp) < 0)) {
	    fprintf(stderr, "[tid %d] Error: func_A: __pmProcessPipeClose() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.7");
	}
	if (argp != NULL) {
	    fprintf(stderr, "[tid %d] Error: func_A: argp not NULL after __pmProcessPipe()\n", iam);
	    pthread_exit("botch A.8");
	}
	j = i0;		/* next in Fibonnaci series */
	i0 = i1;
	i1 = i1 + j;
    }

    close(f);
    return(NULL);
}

/*
 * This uses __pmProcessExec() to run sh(1) to compute N factorial
 */
static void *
func_B(void *arg)
{
    int			iam = *((int *)arg);
    int			i;
    char		c;
    int			lastc;
    int			sts;
    int			f;		/* func_B output */
    int			fd;		/* for answer */
    __pmExecCtl_t	*argp = NULL;
    char		out[MAXPATHLEN+1];
    char		path[MAXPATHLEN+1];
    char		answer[MAXPATHLEN+1];
    char		strbuf[PM_MAXERRMSGLEN];
    char		cmd[MAXCMD+1];

    pthread_barrier_wait(&barrier);

    snprintf(out, MAXPATHLEN, "%s.%d", ctl.basename, iam);

    if ((f = open(out, O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0) {
	perror("func_B open");
	pthread_exit("botch B.1");
    }

    snprintf(answer, MAXPATHLEN, "%s.out.%d", ctl.basename, iam);
    if (pmDebugOptions.appl0)
	fprintf(stderr, "[tid %d] answer=%s\n", iam, answer);
    (void)unlink(answer);
    snprintf(cmd, MAXCMD, "which sh >%s", answer);
    pthread_mutex_lock(&mymutex);
    if ((sts = system(cmd)) != 0) {
	fprintf(stderr, "func_B: system() -> %d\n", sts);
	pthread_exit("botch B.99");
    }
    pthread_mutex_unlock(&mymutex);
    if ((fd = open(answer, O_RDONLY)) < 0) {
	perror("func_B open .out which");
	pthread_exit("botch B.98");
    }
    i = 0;
    while (read(fd, &c, 1) == 1) {
	if (c == '\n') {
	    path[i] = '\0';
	    break;
	}
	path[i++] = c;
    }
    close(fd);
    if (pmDebugOptions.appl0)
	fprintf(stderr, "[tid %d] path=%s\n", iam, path);

    for (i = 0; i < ctl.iter; i++) {
	if ((sts = __pmProcessAddArg(&argp, path)) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_B: __pmProcessAddArg() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch B.2");
	}
	if ((sts = __pmProcessAddArg(&argp, "-c")) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_B: __pmProcessAddArg() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch B.3");
	}
	snprintf(cmd, MAXCMD, "i=1; f=1; while [ $i -le %d ]; do f=`expr $f \\* $i`; i=`expr $i + 1`; done; echo $f >%s.out.%d", i, ctl.basename, iam);
	if ((sts = __pmProcessAddArg(&argp, cmd)) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_B: __pmProcessAddArg() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch B.4");
	}
	(void)unlink(answer);
	if ((sts = __pmProcessExec(&argp, PM_EXEC_TOSS_ALL, PM_EXEC_WAIT)) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_B: __pmProcessExec() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch B.6");
	}
	if ((fd = open(answer, O_RDONLY)) < 0) {
	    perror("func_B open .out");
	    pthread_exit("botch B.7");
	}

	sts = write(f, "Answer: ", strlen("Answer: "));
	if (sts != strlen("Answer: "))
	    fprintf(stderr, "write: botch #4: %d != %zd\n", sts, strlen("Answer: "));
	lastc = EOF;
	while (read(fd, &c, 1) == 1) {
	    sts = write(f, &c, 1);
	    if (sts != 1)
		fprintf(stderr, "write: botch #5: %d != %d\n", sts, 1);
	    lastc = c;
	}
	if (lastc != '\n') {
	    sts = write(f, "\n", 1);
	    if (sts != 1)
		fprintf(stderr, "write: botch #6: %d != %d\n", sts, 1);
	}
	close(fd);
	if (argp != NULL) {
	    fprintf(stderr, "[tid %d] Error: func_B: argp not NULL after __pmProcessExec()\n", iam);
	    pthread_exit("botch B.7");
	}
    }

    close(f);
    return(NULL);
}

/*
 * This one uses _pmProcessPipe() to execute a non-existing binary
 */
static void *
func_C(void *arg)
{
    int			iam = *((int *)arg);
    int			i;
    int			c;
    int			lastc;
    int			sts;
    int			f;		/* func_C output */
    FILE		*fp;		/* for pipe */
    __pmExecCtl_t	*argp = NULL;
    char		out[MAXPATHLEN+1];
    char		strbuf[PM_MAXERRMSGLEN];

    pthread_barrier_wait(&barrier);

    snprintf(out, MAXPATHLEN, "%s.%d", ctl.basename, iam);

    if ((f = open(out, O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0) {
	perror("func_C open");
	pthread_exit("botch C.1");
    }

    for (i = 0; i < ctl.iter; i++) {
	if ((sts = __pmProcessAddArg(&argp, "/no/such/executable")) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_C: __pmProcessAddArg() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch C.2");
	}
	if ((sts = __pmProcessAddArg(&argp, "/no/such/executable")) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_C: __pmProcessAddArg() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch C.2");
	}
	if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_STDIN, &fp)) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_C: __pmProcessPipe() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch C.3");
	}

	sts = write(f, "Answer: ", strlen("Answer: "));
	if (sts != strlen("Answer: "))
	    fprintf(stderr, "write: botch #7: %d != %zd\n", sts, strlen("Answer: "));
	lastc = EOF;
	while ((c = fgetc(fp)) != EOF) {
	    sts = write(f, (char *)&c, 1);
	    if (sts != 1)
		fprintf(stderr, "write: botch #8: %d != %d\n", sts, 1);
	    lastc = c;
	}
	if (lastc != '\n') {
	    sts = write(f, "\n", 1);
	    if (sts != 1)
		fprintf(stderr, "write: botch #9: %d != %d\n", sts, 1);
	}
	if ((sts = __pmProcessPipeClose(fp) < 0)) {
	    fprintf(stderr, "[tid %d] Warning: func_C: __pmProcessPipeClose() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	}
	if (argp != NULL) {
	    fprintf(stderr, "[tid %d] Error: func_C: argp not NULL after __pmProcessPipe()\n", iam);
	    pthread_exit("botch C.4");
	}
    }

    close(f);
    return(NULL);
}


/*
 * This uses __pmProcessUnPickArgs() and __pmProcessPipe() to run a sed
 */
static void *
func_D(void *arg)
{
    int			iam = *((int *)arg);
    int			i;
    char		c;
    int			lastc;
    int			sts;
    int			f;		/* func_D output */
    FILE		*fp;		/* for pipe */
    int			fd;		/* for answer */
    __pmExecCtl_t	*argp = NULL;
    char		out[MAXPATHLEN+1];
    char		path[MAXPATHLEN+1];
    char		answer[MAXPATHLEN+1];
    char		strbuf[PM_MAXERRMSGLEN];
    char		cmd[MAXCMD+1];

    pthread_barrier_wait(&barrier);

    snprintf(out, MAXPATHLEN, "%s.%d", ctl.basename, iam);

    if ((f = open(out, O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0) {
	perror("func_D open");
	pthread_exit("botch D.1");
    }

    snprintf(answer, MAXPATHLEN, "%s.out.%d", ctl.basename, iam);
    if (pmDebugOptions.appl0)
	fprintf(stderr, "[tid %d] answer=%s\n", iam, answer);
    (void)unlink(answer);
    snprintf(cmd, MAXCMD, "which sed >%s", answer);
    pthread_mutex_lock(&mymutex);
    if ((sts = system(cmd)) != 0) {
	fprintf(stderr, "func_D: system() -> %d\n", sts);
	pthread_exit("botch D.99");
    }
    pthread_mutex_unlock(&mymutex);
    if ((fd = open(answer, O_RDONLY)) < 0) {
	perror("func_D open .out which");
	pthread_exit("botch D.98");
    }
    i = 0;
    while (read(fd, &c, 1) == 1) {
	if (c == '\n') {
	    path[i] = '\0';
	    break;
	}
	path[i++] = c;
    }
    close(fd);
    if (pmDebugOptions.appl0)
	fprintf(stderr, "[tid %d] path=%s\n", iam, path);

    for (i = 0; i < ctl.iter; i++) {
	snprintf(cmd, MAXCMD, "%s -n -e 's/bozo/& the boofhead/' -e %dp %s.data", path, i+1, ctl.basename);
	if ((sts = __pmProcessUnpickArgs(&argp, cmd)) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_D: __pmProcessUnpickArgs() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch D.4");
	}
	if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_STDIN, &fp)) < 0) {
	    fprintf(stderr, "[tid %d] Error: func_D: __pmProcessPipe() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch D.6");
	}

	sts = write(f, "Answer: ", strlen("Answer: "));
	if (sts != strlen("Answer: "))
	    fprintf(stderr, "write: botch #10: %d != %zd\n", sts, strlen("Answer: "));
	lastc = EOF;
	while (read(fileno(fp), &c, 1) == 1) {
	    sts = write(f, &c, 1);
	    if (sts != 1)
		fprintf(stderr, "write: botch #11: %d != %d\n", sts, 1);
	    lastc = c;
	}
	if (lastc != '\n') {
	    sts = write(f, "\n", 1);
	    if (sts != 1)
		fprintf(stderr, "write: botch #12: %d != %d\n", sts, 1);
	}
	if ((sts = __pmProcessPipeClose(fp) < 0)) {
	    fprintf(stderr, "[tid %d] Error: func_D: __pmProcessPipeClose() -> %s\n", iam, pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch D.7");
	}
	if (argp != NULL) {
	    fprintf(stderr, "[tid %d] Error: func_D: argp not NULL after __pmProcessPipe()\n", iam);
	    pthread_exit("botch D.8");
	}
    }

    close(f);
    return(NULL);
}

static void
wait_for_thread(int id, pthread_t tid)
{
    int		sts;
    char	*msg;

    sts = pthread_join(tid, (void *)&msg);
    if (sts == 0) {
	if (msg == PTHREAD_CANCELED)
	    printf("thread %d: pthread_join: cancelled?\n", id);
	else if (msg != NULL)
	    printf("thread %d: pthread_join: %s\n", id, msg);
    }
    else
	printf("thread %d: pthread_join: error: %s\n", id, strerror(sts));
}

int
main(int argc, char **argv)
{
    pthread_t	*tid;
    int		*ip;
    int		sts;
    char	*endnum;
    int		errflag = 0;
    int		c;
    int		nthread = 4;	/* one each for func_A, func_B, func_C and func_D */
    char	xflag = '\0';	/* for -x */
    int		i;

    pmSetProgname(argv[0]);

    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    while ((c = getopt(argc, argv, "i:n:D:x:")) != EOF) {
	switch (c) {

	case 'i':
	    ctl.iter = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || ctl.iter < 0) {
		fprintf(stderr, "%s: -i requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 'n':
	    nthread = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || nthread < 0) {
		fprintf(stderr, "%s: -n requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'x':	/* run only one function ... option is A, B, C or D */
	    if (strcmp(optarg, "A") == 0 || strcmp(optarg, "B") == 0 ||
		strcmp(optarg, "C") == 0 || strcmp(optarg, "D") == 0)
		xflag = optarg[0];
	    else {
		fprintf(stderr, "%s: bad -x option (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc-1) {
	fprintf(stderr, "Usage: %s [options] basename\n", pmGetProgname());
	fprintf(stderr, "options:\n");
	fprintf(stderr, "  -i iter	iteration count [default 10]\n");
	fprintf(stderr, "  -n nthread	thread count [default 3]\n");
	fprintf(stderr, "  -D debug\n");
	fprintf(stderr, "  -x {A|B|C|D} run onyl one function => -n 1\n");
	exit(1);
    }

    tid = (pthread_t *)malloc(nthread * sizeof(tid[0]));
    /* TODO - check malloc() */
    ip = (int *)malloc(nthread * sizeof(ip[0]));
    /* TODO - check malloc() */

    ctl.basename = argv[optind];

    sts = pthread_barrier_init(&barrier, NULL, nthread);
    if (sts != 0) {
	printf("pthread_barrier_init: sts=%d\n", sts);
	exit(1);
    }

    for (i = 0; i < nthread; i++) {
	/* need thread-private data for "arg", aka thread number */
	ip[i] = i;

	if ((xflag == '\0' && (i % 4) == 0) || xflag == 'A') {
	    sts = pthread_create(&tid[i], NULL, func_A, &ip[i]);
	    if (sts != 0) {
		printf("func_create: tid_A: sts=%d\n", sts);
		exit(1);
	    }
	}
	else if ((xflag == '\0' && (i % 4) == 1) || xflag == 'B') {
	    sts = pthread_create(&tid[i], NULL, func_B, &ip[i]);
	    if (sts != 0) {
		printf("func_create: tid_B: sts=%d\n", sts);
		exit(1);
	    }
	}
	else if ((xflag == '\0' && (i % 4) == 2) || xflag == 'C') {
	    sts = pthread_create(&tid[i], NULL, func_C, &ip[i]);
	    if (sts != 0) {
		printf("func_create: tid_C: sts=%d\n", sts);
		exit(1);
	    }
	}
	else if ((xflag == '\0' && (i % 4) == 3) || xflag == 'D') {
	    sts = pthread_create(&tid[i], NULL, func_D, &ip[i]);
	    if (sts != 0) {
		printf("func_create: tid_D: sts=%d\n", sts);
		exit(1);
	    }
	}
    }

    for (i = 0; i < nthread; i++) {
	wait_for_thread(i, tid[i]);
    }

    free(tid);
    free(ip);

    sleep(1);

    exit(0);
}
