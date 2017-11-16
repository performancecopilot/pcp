//
// Test QmcDesc class
//

#include <QTextStream>
#include <qmc_source.h>
#include <qmc_desc.h>

QTextStream cerr(stderr);
QTextStream cout(stdout);

int
main(int argc, char* argv[])
{
    int		sts = 0;
    int		c;
    char	buf[MAXHOSTNAMELEN];
    QString	source;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {
	case 'D':
	    sts = pmSetDebug(optarg);
            if (sts < 0) {
		pmprintf("%s: unrecognized debug options specification (%s)\n",
			 pmGetProgname(), optarg);
                sts = 1;
            }
            break;
	case '?':
	default:
	    sts = 1;
	    break;
	}
    }

    if (sts) {
	pmprintf("Usage: %s\n", pmGetProgname());
	pmflush();
	exit(1);
        /*NOTREACHED*/
    }

    (void)gethostname(buf, MAXHOSTNAMELEN);
    buf[MAXHOSTNAMELEN-1] = '\0';
    source = buf;

    fprintf(stderr,"*** Compare metric descriptor with pminfo output ***\n");
    QmcSource *src = QmcSource::getSource(PM_CONTEXT_HOST, source, false);

    if (src->status() < 0) {
	pmprintf("%s: Error: Unable to create context to \"%s\": %s\n",
		 pmGetProgname(), buf, pmErrStr(src->status()));
	pmflush();
	return 1;
    }

    /* Linux hinv.ncpu PMID: 60.0.32 */
    pmID hinv_ncpu = pmID_build(60, 0, 32);
    QmcDesc hinv_ncpu_pmc(hinv_ncpu);
    pmDesc hinv_ncpu_desc = hinv_ncpu_pmc.desc();

    if (hinv_ncpu_pmc.status() < 0) {
	pmprintf("\n%s: Error: hinv.ncpu: %s\n",
		 pmGetProgname(), pmErrStr(hinv_ncpu_pmc.status()));
	pmflush();
	sts = 1;
    }

    printf("hinv.ncpu\n");
    pmPrintDesc(stdout, &hinv_ncpu_desc);
    fflush(stdout);
    fflush(stderr);
    if (system("pminfo -d hinv.ncpu") < 0) {
	pmprintf("%s: Error: Unable to run pminfo\n", pmGetProgname());
	pmflush();
	sts = 1;
    }
    fflush(stdout);
    fflush(stderr);

    fprintf(stderr, "\n*** Fetch a bad descriptor ***\n");
    pmID bad = pmID_build(42,42,42);
    QmcDesc bad_pmc(bad);
    
    if (bad_pmc.status() < 0) {
	pmprintf("%s: Error: Bogus metric: %s\n",
		 pmGetProgname(), pmErrStr(bad_pmc.status()));
	pmflush();
    }
    else
	sts = 1;

    return sts;
}

