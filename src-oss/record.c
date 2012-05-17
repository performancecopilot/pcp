/*
 * record - play pmRecord*() games
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmafm.h>

/*
 * Usage: record folio creator replay host [...]
 */

int
main(int argc, char **argv)
{
    pmRecordHost	*rhp[10];
    int			sts;
    int			i;
    FILE		*f;
    extern int		errno;

    if (argc < 5) {
	printf("Usage: record folio creator replay host [...]\n");
	exit(1);
    }

    f = pmRecordSetup(argv[1], argv[2], atoi(argv[3]));
    if (f == NULL) {
	printf("pmRecordSetup(\"%s\", ...): %s\n",
		argv[1], pmErrStr(-errno));
	exit(1);
    }

    for (i = 4; i < argc; i++) {
	printf("host: %s\n", argv[i]);
	sts = pmRecordAddHost(argv[i], i == 4, &rhp[i-4]);
	if (sts < 0) {
	    printf("pmRecordAddHost(\"%s\", %d, ...): %s\n",
		argv[i], i==4, pmErrStr(sts));
	    exit(1);
	}
	printf("f_config: %s", rhp[i-4]->f_config == NULL ? "NULL" : "OK");
	printf(" f_ipc: %d pid: %" FMT_PID " status: %d\n", rhp[i-4]->fd_ipc, rhp[i-4]->pid, rhp[i-4]->status);
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

    for (i = 4; i < argc; i++) {
	printf("host: %s\n", argv[i]);
	printf("f_config: %s", rhp[i-4]->f_config == NULL ? "NULL" : "OK");
	printf(" f_ipc: %d pid: %" FMT_PID " status: %d\n", rhp[i-4]->fd_ipc, rhp[i-4]->pid, rhp[i-4]->status);
	printf("logfile: %s\n", rhp[i-4]->logfile);
    }

    printf("\nsend some control requests ...\n");
    for (i = 4; i < argc; i++) {
	printf("host: %s\n", argv[i]);
	if (i % 4 == 0) {
	    printf(" OFF\n");
	    sts = pmRecordControl(rhp[i-4], PM_REC_OFF, NULL);
	    if (sts < 0)
		printf("pmRecordControl: %s\n", pmErrStr(sts));
	}
	else if (i % 4 == 1) {
	    printf(" DETACH\n");
	    sts = pmRecordControl(rhp[i-4], PM_REC_DETACH, NULL);
	    if (sts < 0)
		printf("pmRecordControl: %s\n", pmErrStr(sts));
	}
	else if (i % 4 == 2) {
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
    for (i = 4; i < argc; i++) {
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
    for (i = 4; i < argc; i++) {
	printf("host: %s\n", argv[i]);
	printf("f_config: %s", rhp[i-4]->f_config == NULL ? "NULL" : "OK");
	printf(" f_ipc: %d pid: %" FMT_PID " status: %d\n", rhp[i-4]->fd_ipc, rhp[i-4]->pid, rhp[i-4]->status);
	printf("logfile: %s\n", rhp[i-4]->logfile);
    }

    exit(0);
}
