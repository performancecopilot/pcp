/*
 * record - play pmRecord*() games
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/pmafm.h>
#include <sys/types.h>
#include <errno.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

/*
 * Usage: record folio creator replay host [...]
 */

static char *reportexit(int status)
{
    static char	buf[80];

    buf[0] = '\0';

    if (status == 0 || status == -1)
	return buf;
#if HAVE_SYS_WAIT_H
    if (WIFEXITED(status)) {
	if (WIFSIGNALED(status))
	    pmsprintf(buf, sizeof(buf), " exit %d, signal %d", WEXITSTATUS(status), WTERMSIG(status));

	else
	    pmsprintf(buf, sizeof(buf), " exit %d", WEXITSTATUS(status));
    }
    else {
	if (WIFSIGNALED(status))
	    pmsprintf(buf, sizeof(buf), " signal %d", WTERMSIG(status));
    }
#endif
    return buf;
}

int
main(int argc, char **argv)
{
    int			c;
    int			errflag = 0;
    pmRecordHost	*rhp[10];
    int			sts;
    int			i;
    int			j;
    FILE		*f;

    /* trim cmd name of leading directory components */
    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

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

    if (optind > argc-5) {
	errflag++;
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] folio creator replay host [...]\n\
\n\
Options:\n\
  -D debugspec    set PCP debugging options\n",
                pmGetProgname());
        exit(1);
    }

    f = pmRecordSetup(argv[optind], argv[optind+1], atoi(argv[optind+2]));
    if (f == NULL) {
	printf("pmRecordSetup(\"%s\", ...): %s\n",
		argv[optind], pmErrStr(-errno));
	exit(1);
    }

    for (i = optind+3; i < argc; i++) {
	printf("host: %s\n", argv[i]);
	sts = pmRecordAddHost(argv[i], i == optind+4, &rhp[i-4]);
	if (sts < 0) {
	    printf("pmRecordAddHost(\"%s\", %d, ...): %s\n",
		argv[i], i == optind+4, pmErrStr(sts));
	    exit(1);
	}
	printf("f_config: %s", rhp[i-4]->f_config == NULL ? "NULL" : "OK");
	printf(" f_ipc: %d pid: %" FMT_PID " status: %d%s\n", rhp[i-4]->fd_ipc, rhp[i-4]->pid, rhp[i-4]->status, reportexit(rhp[i-4]->status));
	printf("logfile: %s\n", rhp[i-4]->logfile);
	fprintf(rhp[i-4]->f_config, "log mandatory on 30sec pmcd.control.timeout\n");
    }

    sts = pmRecordControl(NULL, PM_REC_ON, NULL);
    if (sts < 0) {
	printf("pmRecordControl(NULL, PM_REC_ON, NULL): %s\n",
		pmErrStr(sts));
	exit(1);
    }

    printf("\nsleeping ...\n\n");
    sleep(3);

    for (i = optind+3; i < argc; i++) {
	printf("host: %s\n", argv[i]);
	printf("f_config: %s", rhp[i-4]->f_config == NULL ? "NULL" : "OK");
	printf(" f_ipc: %d pid: %" FMT_PID " status: %d%s\n", rhp[i-4]->fd_ipc, rhp[i-4]->pid, rhp[i-4]->status, reportexit(rhp[i-4]->status));
	printf("logfile: %s\n", rhp[i-4]->logfile);
    }

    printf("\nsend some control requests ...\n");
    for (j = 0, i = optind+3; i < argc; i++, j++) {
	printf("host: %s\n", argv[i]);
	if (j % 4 == 0) {
	    printf(" OFF\n");
	    sts = pmRecordControl(rhp[i-4], PM_REC_OFF, NULL);
	    if (sts < 0)
		printf("pmRecordControl: %s\n", pmErrStr(sts));
	}
	else if (j % 4 == 1) {
	    printf(" DETACH\n");
	    sts = pmRecordControl(rhp[i-4], PM_REC_DETACH, NULL);
	    if (sts < 0)
		printf("pmRecordControl: %s\n", pmErrStr(sts));
	}
	else if (j % 4 == 2) {
	    printf(" STATUS\n");
	    sts = pmRecordControl(rhp[i-4], PM_REC_STATUS, NULL);
	    if (sts < 0)
		printf("pmRecordControl: %s\n", pmErrStr(sts));
	}
	else {
	    printf(" close(fd_ipc)\n");
	    close(rhp[i-4]->fd_ipc);
	}
	sleep(1);
    }

    printf("\nnow stop 'em all ...\n");
    for (i = optind+3; i < argc; i++) {
	printf("host: %s\n", argv[i]);
	sts = pmRecordControl(rhp[i-4], PM_REC_OFF, NULL);
	if (sts < 0)
	    printf("pmRecordControl(... OFF ...): %s\n", pmErrStr(sts));
    }

    sleep(2);

    printf("\nand again to get any exit status ...\n");
    sts = pmRecordControl(NULL, PM_REC_OFF, NULL);
    if (sts < 0)
	printf("pmRecordControl( ... OFF ...): %s\n", pmErrStr(sts));

    printf("\n\ndiscover what's happened ...\n");
    putchar('\n');
    for (i = optind+3; i < argc; i++) {
	printf("host: %s\n", argv[i]);
	printf("f_config: %s", rhp[i-4]->f_config == NULL ? "NULL" : "OK");
	printf(" f_ipc: %d pid: %" FMT_PID " status: %d%s\n", rhp[i-4]->fd_ipc, rhp[i-4]->pid, rhp[i-4]->status, reportexit(rhp[i-4]->status));
	printf("logfile: %s\n", rhp[i-4]->logfile);
    }

    exit(0);
}
