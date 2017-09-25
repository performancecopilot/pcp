//
// Test QmcIndom class
//

#include <QTextStream>
#include <qmc_context.h>
#include <qmc_source.h>
#include <qmc_desc.h>
#include <qmc_indom.h>

QTextStream cerr(stderr);
QTextStream cout(stdout);

int
main(int argc, char* argv[])
{
    int		sts = 0;
    int		c;
    __pmContext	*ctxp;

    pmProgname = basename(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {
	case 'D':
	    sts = pmSetDebug(optarg);
            if (sts < 0) {
		pmprintf("%s: unrecognized debug options specification (%s)\n",
			 pmProgname, optarg);
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
	pmprintf("Usage: %s\n", pmProgname);
	pmflush();
	exit(1);
        /*NOTREACHED*/
    }

    fprintf(stderr, "*** Lookup an indom ***\n");
    QString source = QString("archives/oview-short");
    QmcSource *src = QmcSource::getSource(PM_CONTEXT_ARCHIVE, source, false);

    if (src->status() < 0) {
	pmprintf("%s: Error: Unable to create context to \"%s\": %s\n",
		pmProgname, (const char *)source.toLatin1(),
		pmErrStr(src->status()));
	pmflush();
	return 1;
    }

    pmID hinv_map_cpu = pmid_build(1, 26, 9);
    QmcDesc hinv_map_cpu_pmc(hinv_map_cpu);

    if (hinv_map_cpu_pmc.status() < 0) {
	pmprintf("\n%s: Error: hinv.map.cpu: %s\n",
		 pmProgname, pmErrStr(hinv_map_cpu_pmc.status()));
	pmflush();
	return 1;
    }

    QmcIndom indom(PM_CONTEXT_ARCHIVE, hinv_map_cpu_pmc);

    if (indom.status() < 0) {
	pmprintf("%s: Error: hinv.map.cpu: %s\n",
		 pmProgname, pmErrStr(indom.status()));
	pmflush();
	return 1;
    }

    indom.dump(cout);

    if (indom.diffProfile()) {
	pmprintf("%s: Error: Profile requires updating but there is nothing in it\n",
		 pmProgname);
	sts = 1;
    }

    fprintf(stderr, "\n*** Reference one instance ***\n");
    indom.lookup("cpu:1.3.a");

    if (!indom.diffProfile()) {
	pmprintf("%s: Error: Profile should require updating but flag not set\n",
		 pmProgname);
	sts = 1;
    }

    indom.dump(cout);
    indom.genProfile();

    ctxp = __pmHandleToPtr(pmWhichContext());
    if (ctxp == NULL) {
	pmprintf("%s: Error: no current context first time?\n", pmProgname);
	pmflush();
	return 1;
    }
    __pmDumpProfile(stderr, PM_INDOM_NULL, ctxp->c_instprof);
    PM_UNLOCK(ctxp->c_lock);

    if (indom.diffProfile()) {
	pmprintf("%s: Error: Profile just generated but flag still set\n",
		 pmProgname);
	sts = 1;
    }

    fprintf(stderr, "\n*** Reference all instances ***\n");
    indom.refAll();

    if (!indom.diffProfile()) {
	pmprintf("%s: Error: All instances referenced but profile flag unset\n",
		 pmProgname);
	sts = 1;
    }

    indom.dump(cout);
    indom.genProfile();

    ctxp = __pmHandleToPtr(pmWhichContext());
    if (ctxp == NULL) {
	pmprintf("%s: Error: no current context second time?\n", pmProgname);
	pmflush();
	return 1;
    }
    __pmDumpProfile(stderr, PM_INDOM_NULL, ctxp->c_instprof);
    PM_UNLOCK(ctxp->c_lock);

    if (indom.diffProfile()) {
	pmprintf("%s: Error: Profile just generated but flag still set\n",
		 pmProgname);
	sts = 1;
    }


    pmflush();
    return sts;
}
