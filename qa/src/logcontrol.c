/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * Utility to control archive logging for metrics
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		j;
    int		sts;
    int		ctlport;
    int		pid = PM_LOG_PRIMARY_PID;
    int		port = PM_LOG_NO_PORT;
    int		newstate;
    int		errflag = 0;
    char	*host = "localhost";
    char	*namespace = PM_NS_DEFAULT;
    char	*instance = (char *)0;
    char	*endnum;
    int		numpmid;
    pmID	*pmidlist;
    char	*control_arg;
    int		control;
    char	*state_arg;
    int		state;
    pmResult	*request;
    pmDesc	desc;
    int		inst;
    pmResult	*status;
    int		delta = 5000;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:i:n:P:p:r:")) != EOF) {
	switch (c) {

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

	case 'h':	/* hostname for PMCD to contact */
	    host = optarg;
	    break;

	case 'i':	/* instance identifier */
	    instance = optarg;
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 'P':	/* port for pmlogger */
	    port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -P requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 'p':	/* pid for pmlogger */
	    pid = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -p requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 'r':	/* logging delta */
	    delta = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0') {
		fprintf(stderr, "%s: -r requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind > argc-2) {
USAGE:
	fprintf(stderr,
"Usage: %s [options] action metric ...\n"
"\n"
"Options\n"
" -D level	set debug level\n"
" -h hostname	contact PMCD at this host\n"
" -i instance	apply only to this instance\n"
" -n namespace	use and alternative name space\n"
" -P port	port for pmlogger instance\n"
" -p pid	pid for pmlogger instance\n"
" -r delta	requested logging interval (msec)\n"
"\n"
"Actions\n"
" mandatory {on|off|maybe}\n"
" advisory {on|off}\n"
" enquire\n"
		, pmProgname);
	exit(1);
    }

    control_arg = argv[optind++];
    if (strcmp("mandatory", control_arg) == 0)
	control = PM_LOG_MANDATORY;
    else if (strcmp("advisory", control_arg) == 0)
	control = PM_LOG_ADVISORY;
    else if (strcmp("enquire", control_arg) == 0)
	control = PM_LOG_ENQUIRE;
    else
	goto USAGE;

    if (control == PM_LOG_ENQUIRE)
	state = 0;
    else {
	state_arg = argv[optind++];
	if (strcmp("on", state_arg) == 0)
	    state = PM_LOG_ON;
	else if (strcmp("off", state_arg) == 0)
	    state = PM_LOG_OFF;
	else if (control == PM_LOG_MANDATORY && strcmp("maybe", state_arg) == 0)
	    state = PM_LOG_MAYBE;
	else
	    goto USAGE;
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, host)) < 0) {
	printf("%s: Cannot connect to PMCD on host \"%s\": %s\n", pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if ((sts = __pmConnectLogger(host, &pid, &port)) < 0) {
	printf("%s: Cannot connect to pmlogger (%d) on host \"%s\": %s\n",
	    pmProgname, pid, host, pmErrStr(sts));
	exit(1);
    }
    ctlport = sts;

    numpmid = argc - optind;
    pmidlist = (pmID *)malloc(numpmid * sizeof(pmID));

    if ((sts = pmLookupName(numpmid, &argv[optind], pmidlist)) < 0) {
	printf("pmLookupName: %s\n", pmErrStr(sts));
	exit(1);
    }
    for (i = 0; i < numpmid; i++) {
	if (pmidlist[i] == PM_ID_NULL) {
	    printf("Error: metric %s is unknown\n", argv[optind+i]);
	    exit(1);
	}
	if (instance != (char *)0) {
	    if ((sts = pmLookupDesc(pmidlist[i], &desc)) < 0) {
		printf("pmDesc(%s %s): %s\n",
		    argv[optind+i], pmIDStr(pmidlist[i]), pmErrStr(sts));
		exit(1);
	    }
	    if ((inst = pmLookupInDom(desc.indom, instance)) < 0) {
		printf("pmLookupInDom(%s): %s\n", instance, pmErrStr(inst));
		exit(1);
	    }
	    if ((sts = pmDelProfile(desc.indom, 0, (int *)0)) < 0) {
		printf("pmDelProfile(%s): %s\n", pmInDomStr(desc.indom), pmErrStr(inst));
		exit(1);
	    }
	    if ((sts = pmAddProfile(desc.indom, 1, &inst)) < 0) {
		printf("pmAddProfile(%s): %s\n", pmInDomStr(desc.indom), pmErrStr(inst));
		exit(1);
	    }
	}
    }

    if ((sts = pmFetch(numpmid, pmidlist, &request)) < 0) {
	printf("pmFetch: %s\n", pmErrStr(sts));
	exit(1);
    }

    sts = __pmControlLog(ctlport, request, control, state, delta, &status);
    if (sts < 0) {
	printf("%s: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    for (i = 0; i < numpmid; i++) {
	if ((sts = pmLookupDesc(pmidlist[i], &desc)) < 0) {
	    printf("pmDesc(%s): %s\n", pmIDStr(pmidlist[i]), pmErrStr(sts));
	    goto done;
	}
	printf("%s", argv[optind+i]);
	if (status->vset[i]->numval < 0)
	    printf(" Error: %s\n", pmErrStr(status->vset[i]->numval));
	else if (status->vset[i]->numval == 0)
	    printf(" No value?\n");
	else {
	    for (j = 0; j < status->vset[i]->numval; j++) {
		pmValue     *vp = &status->vset[i]->vlist[j];
		char	*p;
		if (status->vset[i]->numval > 1 || desc.indom != PM_INDOM_NULL) {
		    if (j == 0)
			putchar('\n');
		    printf("  inst [%d", vp->inst);
		    if (pmNameInDom(desc.indom, vp->inst, &p) < 0)
			printf(" or ???]");
		    else {
			printf(" or \"%s\"]", p);
			free(p);
		    }
		}
		newstate = PMLC_GET_STATE(vp->value.lval);
		printf("  %d (%s %s", newstate,
		    PMLC_GET_MAND(vp->value.lval) ? "mand" : "adv",
		    PMLC_GET_ON(vp->value.lval) ? "on" : "off");
		if (PMLC_GET_ON(vp->value.lval))
		    printf(", delta=%d msec", PMLC_GET_DELTA(vp->value.lval));
		if (PMLC_GET_INLOG(vp->value.lval))
		    printf(", inlog");
		if (PMLC_GET_AVAIL(vp->value.lval))
		    printf(", avail");
		printf(")\n");
	    }
	}
    }

    free(pmidlist);
    pmFreeResult(request);
    pmFreeResult(status);
    if ((sts = pmWhichContext()) < 0) {
	printf("%s: pmWhichContext: %s\n", pmProgname, pmErrStr(sts));
	goto done;
    }
    pmDestroyContext(sts);

done:
    close(ctlport);
    sleep(1);

    exit(0);
}
