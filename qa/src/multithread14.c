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
    int			c;
    int			lastc;
    int			sts;
    int			i0 = 0;		/* Fibonacci #(n-1) */
    int			i1 = 1;		/* Fibonacci #n */
    FILE		*f;		/* func_A output */
    FILE		*fp;		/* for pipe */
    __pmExecCtl_t	*argp = NULL;
    char		path[MAXPATHLEN+1];
    char		strbuf[PM_MAXERRMSGLEN];
    char		cmd[MAXCMD+1];

    snprintf(path, MAXPATHLEN, "%s.%d", ctl.basename, iam);

    if ((f = fopen(path, "w")) == NULL) {
	perror("func_A fopen");
	pthread_exit("botch A.1");
    }
    setbuf(f, NULL);

    pthread_barrier_wait(&barrier);

    snprintf(cmd, MAXCMD, "which expr >%s.out.%d", ctl.basename, iam);
    if ((sts = system(cmd)) != 0) {
	fprintf(stderr, "func_A: system() -> %d\n", sts);
	pthread_exit("botch A.99");
    }
    snprintf(path, MAXPATHLEN, "%s.out.%d", ctl.basename, iam);
    if ((fp = fopen(path, "r")) == NULL) {
	perror("func_A fopen .out which");
	pthread_exit("botch A.98");
    }
    i = 0;
    while ((c = fgetc(fp)) != EOF) {
	if (c == '\n') {
	    path[i] = '\0';
	    break;
	}
	path[i++] = c;
    }
    if (pmDebugOptions.appl0)
	fprintf(f, "path=%s\n", path);

    for (i = 0; i < ctl.iter; i++) {
	if ((sts = __pmProcessAddArg(&argp, path)) < 0) {
	    fprintf(f, "Error: func_A: __pmProcessAddArg() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.2");
	}
	sprintf(strbuf, "%d", i0);
	if ((sts = __pmProcessAddArg(&argp, strbuf)) < 0) {
	    fprintf(f, "Error: func_A: __pmProcessAddArg() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.3");
	}
	if ((sts = __pmProcessAddArg(&argp, "+")) < 0) {
	    fprintf(f, "Error: func_A: __pmProcessAddArg() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.4");
	}
	sprintf(strbuf, "%d", i1);
	if ((sts = __pmProcessAddArg(&argp, strbuf)) < 0) {
	    fprintf(f, "Error: func_A: __pmProcessAddArg() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.5");
	}
	if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_STDIN, &fp)) < 0) {
	    fprintf(f, "Error: func_A: __pmProcessPipe() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch A.6");
	}
	fprintf(f, "Answer: ");
	lastc = EOF;
	while ((c = fgetc(fp)) != EOF) {
	    fputc(c, f);
	    lastc = c;
	}
	if (lastc != '\n')
	    fputc('\n', f);
	__pmProcessPipeClose(fp);
	if (argp != NULL) {
	    fprintf(f, "Error: func_A: argp not NULL after __pmProcessPipe()\n");
	    pthread_exit("botch A.7");
	}
	j = i0;
	i0 = i1;
	i1 = i1 + j;
    }

    fclose(f);
    return(NULL);	/* pthread done */
}

/*
 * This uses __pmProcessExec() to run sh(1) to compute N factorial
 */
static void *
func_B(void *arg)
{
    int			iam = *((int *)arg);
    int			i;
    int			c;
    int			lastc;
    int			sts;
    FILE		*f;		/* func_B output */
    FILE		*fp;		/* for answer */
    __pmExecCtl_t	*argp = NULL;
    char		path[MAXPATHLEN+1];
    char		out[MAXPATHLEN+1];
    char		strbuf[PM_MAXERRMSGLEN];
    char		cmd[MAXCMD+1];

    snprintf(path, MAXPATHLEN, "%s.%d", ctl.basename, iam);

    if ((f = fopen(path, "w")) == NULL) {
	perror("func_B fopen");
	pthread_exit("botch B.1");
    }
    setbuf(f, NULL);

    pthread_barrier_wait(&barrier);

    snprintf(cmd, MAXCMD, "which sh >%s.out.%d", ctl.basename, iam);
    if ((sts = system(cmd)) != 0) {
	fprintf(stderr, "func_B: system() -> %d\n", sts);
	pthread_exit("botch B.99");
    }
    snprintf(path, MAXPATHLEN, "%s.out.%d", ctl.basename, iam);
    if ((fp = fopen(path, "r")) == NULL) {
	perror("func_B fopen .out which");
	pthread_exit("botch B.98");
    }
    i = 0;
    while ((c = fgetc(fp)) != EOF) {
	if (c == '\n') {
	    path[i] = '\0';
	    break;
	}
	path[i++] = c;
    }
    if (pmDebugOptions.appl0)
	fprintf(f, "path=%s\n", path);

    for (i = 0; i < ctl.iter; i++) {
	if ((sts = __pmProcessAddArg(&argp, path)) < 0) {
	    fprintf(f, "Error: func_B: __pmProcessAddArg() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch B.2");
	}
	if ((sts = __pmProcessAddArg(&argp, "-c")) < 0) {
	    fprintf(f, "Error: func_B: __pmProcessAddArg() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch B.3");
	}
	snprintf(cmd, MAXCMD, "i=1; f=1; while [ $i -le %d ]; do f=`expr $f \\* $i`; i=`expr $i + 1`; done; echo $f >%s.out.%d", i, ctl.basename, iam);
	if ((sts = __pmProcessAddArg(&argp, cmd)) < 0) {
	    fprintf(f, "Error: func_B: __pmProcessAddArg() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch B.4");
	}
	if ((sts = __pmProcessExec(&argp, PM_EXEC_TOSS_STDIN, PM_EXEC_WAIT)) < 0) {
	    fprintf(f, "Error: func_B: __pmProcessExec() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch B.6");
	}
	snprintf(out, MAXPATHLEN, "%s.out.%d", ctl.basename, iam);
	if ((fp = fopen(out, "r")) == NULL) {
	    perror("func_B fopen .out");
	    pthread_exit("botch B.7");
	}
	fprintf(f, "Answer: ");
	lastc = EOF;
	while ((c = fgetc(fp)) != EOF) {
	    fputc(c, f);
	    lastc = c;
	}
	if (lastc != '\n')
	    fputc('\n', f);
	fclose(fp);
	if (argp != NULL) {
	    fprintf(f, "Error: func_B: argp not NULL after __pmProcessExec()\n");
	    pthread_exit("botch B.7");
	}
    }

    fclose(f);
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
    FILE		*f;		/* func_C output */
    FILE		*fp;		/* for pipe */
    __pmExecCtl_t	*argp = NULL;
    char		path[MAXPATHLEN+1];
    char		strbuf[PM_MAXERRMSGLEN];

    snprintf(path, MAXPATHLEN, "%s.%d", ctl.basename, iam);

    if ((f = fopen(path, "w")) == NULL) {
	perror("func_C fopen");
	pthread_exit("botch C.1");
    }
    setbuf(f, NULL);

    pthread_barrier_wait(&barrier);

    for (i = 0; i < ctl.iter; i++) {
	if ((sts = __pmProcessAddArg(&argp, "/no/such/executable")) < 0) {
	    fprintf(f, "Error: func_C: __pmProcessAddArg() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch C.2");
	}
	if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_STDIN, &fp)) < 0) {
	    fprintf(f, "Error: func_C: __pmProcessPipe() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch C.3");
	}
	fprintf(f, "Answer: ");
	lastc = EOF;
	while ((c = fgetc(fp)) != EOF) {
	    fputc(c, f);
	    lastc = c;
	}
	if (lastc != '\n')
	    fputc('\n', f);
	__pmProcessPipeClose(fp);
	if (argp != NULL) {
	    fprintf(f, "Error: func_C: argp not NULL after __pmProcessPipe()\n");
	    pthread_exit("botch C.4");
	}
    }

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
    int			c;
    int			lastc;
    int			sts;
    FILE		*f;		/* func_D output */
    FILE		*fp;		/* for answer */
    __pmExecCtl_t	*argp = NULL;
    char		path[MAXPATHLEN+1];
    char		strbuf[PM_MAXERRMSGLEN];
    char		cmd[MAXCMD+1];

    snprintf(path, MAXPATHLEN, "%s.%d", ctl.basename, iam);

    if ((f = fopen(path, "w")) == NULL) {
	perror("func_D fopen");
	pthread_exit("botch D.1");
    }
    setbuf(f, NULL);

    pthread_barrier_wait(&barrier);

    snprintf(cmd, MAXCMD, "which sed >%s.out.%d", ctl.basename, iam);
    if ((sts = system(cmd)) != 0) {
	fprintf(stderr, "func_D: system() -> %d\n", sts);
	pthread_exit("botch D.99");
    }
    snprintf(path, MAXPATHLEN, "%s.out.%d", ctl.basename, iam);
    if ((fp = fopen(path, "r")) == NULL) {
	perror("func_D fopen .out which");
	pthread_exit("botch D.98");
    }
    i = 0;
    while ((c = fgetc(fp)) != EOF) {
	if (c == '\n') {
	    path[i] = '\0';
	    break;
	}
	path[i++] = c;
    }
    if (pmDebugOptions.appl0)
	fprintf(f, "path=%s\n", path);

    for (i = 0; i < ctl.iter; i++) {
	snprintf(cmd, MAXCMD, "%s -n -e 's/bozo/& the boofhead/' -e %dp %s.data", path, i+1, ctl.basename);
	if ((sts = __pmProcessUnpickArgs(&argp, cmd)) < 0) {
	    fprintf(f, "Error: func_D: __pmProcessUnpickArgs() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch D.4");
	}
	if ((sts = __pmProcessPipe(&argp, "r", PM_EXEC_TOSS_STDIN, &fp)) < 0) {
	    fprintf(f, "Error: func_D: __pmProcessPipe() -> %s\n", pmErrStr_r(sts, strbuf, sizeof(strbuf)));
	    pthread_exit("botch D.6");
	}
	fprintf(f, "Answer: ");
	lastc = EOF;
	while ((c = fgetc(fp)) != EOF) {
	    fputc(c, f);
	    lastc = c;
	}
	if (lastc != '\n')
	    fputc('\n', f);
	__pmProcessPipeClose(fp);
	if (argp != NULL) {
	    fprintf(f, "Error: func_D: argp not NULL after __pmProcessPipe()\n");
	    pthread_exit("botch D.7");
	}
    }

    fclose(f);
    return(NULL);
}

static void
wait_for_thread(char *name, pthread_t tid)
{
    int		sts;
    char	*msg;

    sts = pthread_join(tid, (void *)&msg);
    if (sts == 0) {
	if (msg == PTHREAD_CANCELED)
	    printf("thread %s: pthread_join: cancelled?\n", name);
	else if (msg != NULL)
	    printf("thread %s: pthread_join: %s\n", name, msg);
    }
    else
	printf("thread %s: pthread_join: error: %s\n", name, strerror(sts));
}

int
main(int argc, char **argv)
{
    pthread_t	*tid;
    int		sts;
    char	*endnum;
    int		errflag = 0;
    int		c;
    int		nthread = 4;	/* one each for func_A, func_B, func_C and func_D */
    int		i;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "i:n:D:")) != EOF) {
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
	exit(1);
    }

    tid = (pthread_t *)malloc(nthread * sizeof(tid[0]));
    /* TODO - check malloc() */

    ctl.basename = argv[optind];

    sts = pthread_barrier_init(&barrier, NULL, nthread);
    if (sts != 0) {
	printf("pthread_barrier_init: sts=%d\n", sts);
	exit(1);
    }

    for (i = 0; i < nthread; i++) {
	/* need thread-private data for "arg", aka thread number */
	int	*ip = (int *)malloc(sizeof(int));
	*ip = i;

	if ((i % 4) == 0) {
	    sts = pthread_create(&tid[i], NULL, func_A, ip);
	    if (sts != 0) {
		printf("func_create: tid_A: sts=%d\n", sts);
		exit(1);
	    }
	}
	else if ((i % 4) == 1) {
	    sts = pthread_create(&tid[i], NULL, func_B, ip);
	    if (sts != 0) {
		printf("func_create: tid_B: sts=%d\n", sts);
		exit(1);
	    }
	}
	else if ((i % 4) == 2) {
	    sts = pthread_create(&tid[i], NULL, func_C, ip);
	    if (sts != 0) {
		printf("func_create: tid_C: sts=%d\n", sts);
		exit(1);
	    }
	}
	else if ((i % 4) == 3) {
	    sts = pthread_create(&tid[i], NULL, func_D, ip);
	    if (sts != 0) {
		printf("func_create: tid_D: sts=%d\n", sts);
		exit(1);
	    }
	}
    }


    for (i = 0; i < nthread; i++) {
	char	label[9];
	snprintf(label, 8, "tid_%03d", i);
	wait_for_thread(label, tid[i]);
    }

    exit(0);
}
