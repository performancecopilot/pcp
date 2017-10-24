/*
 * record-setarg - simulate pmRecord*() usage
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmafm.h>

/*
 * Usage: record-setarg folio
 */

int
main(int argc, char **argv)
{
    pmRecordHost	*rhp;
    int			sts;
    FILE		*f;
    extern int		errno;
    char		buf[20];

    if (argc < 2) {
	printf("Usage: record-setarg folio\n");
	exit(1);
    }

    f = pmRecordSetup(argv[1], "record-setarg", 0);
    if (f == NULL) {
	printf("pmRecordSetup(\"%s\", ...): %s\n",
		argv[1], pmErrStr(-errno));
	exit(1);
    }

    sts = pmRecordAddHost("localhost", 1, &rhp);
    if (sts < 0) {
	printf("pmRecordAddHost: %s\n", pmErrStr(sts));
	exit(1);
    }
    fprintf(rhp->f_config, "log mandatory on default sample.bin [ \"bin-400\" \"bin-800\" ]\n");

    sts = pmRecordControl(NULL, PM_REC_SETARG, "-t");
    pmsprintf(buf, sizeof(buf), "%dsec", 1);
    sts += pmRecordControl(NULL, PM_REC_SETARG, buf);
    pmsprintf(buf, sizeof(buf), "-T%dsec", 10);
    sts += pmRecordControl(NULL, PM_REC_SETARG, buf);
    if (sts < 0) {
	printf("pmRecordControl(NULL, PM_REC_SETARG, NULL): %s\n",
		pmErrStr(sts));
	exit(1);
    }

    sts = pmRecordControl(NULL, PM_REC_ON, NULL);
    if (sts < 0) {
	printf("pmRecordControl(NULL, PM_REC_ON, NULL): %s\n",
		pmErrStr(sts));
	exit(1);
    }

    printf("\nsleeping ...\n\n");
    sleep(12);

    sts = pmRecordControl(rhp, PM_REC_OFF, NULL);
    if (sts < 0)
	printf("pmRecordControl(..., PM_REC_OFF, ...): %s\n", pmErrStr(sts));

    exit(0);
}
