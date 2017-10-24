//
// Test event tracing functionality in libqmc
//
// Caller can setup deterministic output by calling
// "pmstore sample.event.reset 1" beforehand.
//

#include <errno.h>
#include <QTextStream>
#include <qmc_context.h>
#include <qmc_group.h>
#include <qmc_metric.h>
#include <qmc_indom.h>

QTextStream cerr(stderr);
QTextStream cout(stdout);

int
main(int argc, char* argv[])
{
    int		sts = 0;
    int		c;

    __pmSetProgname(argv[0]);
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

    cerr << "*** Create a single fetch group ***" << endl;
    QmcGroup group;
    pmflush();

    cerr << endl << "*** Event metric ***" << endl;
    QmcMetric* sample_records = group.addMetric("sample.event.records");
    if (sample_records->status() < 0)
	sts = 1;
    else
	sample_records->dump(cerr);
    pmflush();

    QmcMetric* sample_seconds = group.addMetric("sample.seconds");
    if (sample_seconds->status() < 0)
	sts = 1;
    else
	sample_seconds->dump(cerr);
    pmflush();

    //
    // pmdasample provides a 4-phase fetch pattern for sample.event
    //
    for (int i = 0; i < 4; i++) {
	sleep(1);
	cerr << endl << "*** Group Fetch " << i << " ***" << endl;
	group.fetch();
	sample_records->dump(cerr);
	sample_seconds->dump(cerr);
    }

    cerr << endl << "*** Exiting ***" << endl;
    return sts;
}
