//
// Test QmcMetric class
//

#include <errno.h>
#include <QTextStream>
#include <qmc_context.h>
#include <qmc_group.h>
#include <qmc_metric.h>
#include <qmc_indom.h>

QTextStream cerr(stderr);
QTextStream cout(stdout);

void
changeConf(const char* str)
{
    char	name[MAXPATHLEN];
    FILE	*fp;
    
    pmsprintf(name, sizeof(name), "%s/pmdas/simple/simple.conf", pmGetConfig("PCP_VAR_DIR"));
    fp = fopen(name, "w");
    if (fp == NULL) {
	cerr << pmProgname << ": /var/pcp/pmdas/simple/simple.conf: "
	     << strerror(errno) << endl;
	exit(1);
	/*NOTREACHED*/
    }

    fprintf(fp, "%s\n", str);
    fclose(fp);
}

int
main(int argc, char* argv[])
{
    int		sts = 0;
    int		c;

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

    cerr << "*** Create a single fetch group ***" << endl;
    QmcGroup group1;
    pmflush();

    cerr << endl << "*** Simple metric ***" << endl;
    QmcMetric* hinv_ncpu = group1.addMetric("hinv.ncpu");
    pmflush();
    
    if (hinv_ncpu->status() < 0)
	sts = 1;
    else
	hinv_ncpu->dump(cerr);

    cerr << endl << "*** Metric with an Indom ***" << endl;
    QmcMetric* percpu_user = group1.addMetric("sample.bin");
    pmflush();

    if (percpu_user->status() < 0)
	sts = 1;
    else
	percpu_user->dump(cerr);

    cerr << endl << "*** proc style specific instance ***" << endl;
    QmcMetric* load_avg = group1.addMetric("kernel.all.load[1,5]");
    pmflush();
    
    if (load_avg->status() < 0)
	sts = 1;
    else
	load_avg->dump(cerr);

    cerr << endl << "*** String metric ***" << endl;
    QmcMetric* sample_hullo = group1.addMetric("sample.string.hullo");

    if (sample_hullo->status() < 0)
	sts = 1;
    else
	sample_hullo->dump(cerr);

    cerr << endl << "*** Rate converted metric ***" << endl;
    QmcMetric* sample_seconds = group1.addMetric("sample.seconds");

    if (sample_seconds->status() < 0)
	sts = 1;
    else
	sample_seconds->dump(cerr);

    cerr << endl << "*** Bogus metric ***" << endl;
    QmcMetric* bogus_metric = group1.addMetric("bogus.metric");
    pmflush();

    if (bogus_metric->status() >= 0) {
	pmprintf("%s: Error: bogus.metric was not invalid!\n",
		 pmProgname);
	pmflush();
	sts = 1;
    }
	
    cerr << endl << "*** Bogus instance ***" << endl;
    QmcMetric* bogus_inst = group1.addMetric("kernel.all.load[2]");
    pmflush();
    
    if (bogus_inst->status() >= 0) {
	pmprintf("%s: Error: kernel.all.load[2] was not invalid!\n",
		 pmProgname);
	pmflush();
	sts = 1;
    }

    pmflush();

    sleep(1);

    cerr << endl << "*** Group 1 Fetch 1 ***" << endl;
    group1.fetch();
    hinv_ncpu->dump(cerr);
    percpu_user->dump(cerr);
    load_avg->dump(cerr);
    sample_hullo->dump(cerr);
    sample_seconds->dump(cerr);

    sleep(1);

    cerr << endl << "*** Group 1 Fetch 2 ***" << endl;
    group1.fetch();
    hinv_ncpu->dump(cerr);
    percpu_user->dump(cerr);
    load_avg->dump(cerr);
    sample_hullo->dump(cerr);
    sample_seconds->dump(cerr);

    cerr << endl << "*** Remove an instance ***" << endl;
    load_avg->removeInst(0);
    load_avg->dump(cerr);

    sleep(1);
    
    cerr << endl << "*** Group 1 Fetch 3 ***" << endl;
    group1.fetch();
    hinv_ncpu->dump(cerr);
    percpu_user->dump(cerr);
    load_avg->dump(cerr);
    sample_hullo->dump(cerr);
    sample_seconds->dump(cerr);

    cerr << endl << "*** Add an instance ***" << endl;
    load_avg->addInst("15");
    load_avg->dump(cerr);

    sleep(1);
    
    cerr << endl << "*** Group 1 Fetch 4 ***" << endl;
    group1.fetch();
    hinv_ncpu->dump(cerr);
    percpu_user->dump(cerr);
    load_avg->dump(cerr);
    sample_hullo->dump(cerr);
    sample_seconds->dump(cerr);

    cerr << endl << "*** Creating a new group ***" << endl;
    QmcGroup group2;

    cerr << endl << "*** Adding a metric with a dynamic indom ***" << endl;
    QmcMetric* simple_now = group2.addMetric("simple.now");
    pmflush();
    simple_now->dump(cerr);

    cerr << endl << "*** Group 2 Fetch 1 ***" << endl;
    group2.fetch();
    simple_now->dump(cerr);

    cerr << endl << "*** Change the indom ***" << endl;
    changeConf("sec,min,hour");

    cerr << endl << "*** Group 2 Fetch 2 ***" << endl;
    group2.fetch();
    simple_now->dump(cerr);

    cerr << endl << "*** Updating indom ***" << endl;
    if (simple_now->indom()->changed())
	simple_now->updateIndom();
    else
	cerr << "Nothing to update!" << endl;

    simple_now->dump(cerr);
    simple_now->indom()->dump(cerr);

    cerr << endl << "*** Group 2 Fetch 3 ***" << endl;
    group2.fetch();
    simple_now->dump(cerr);

    cerr << endl << "*** Remove instance from PMDA ***" << endl;
    changeConf("sec,hour");

    cerr << endl << "*** Group 2 Fetch 4 ***" << endl;
    group2.fetch();
    simple_now->dump(cerr);

    cerr << endl << "*** Remove an instance ***" << endl;
    simple_now->removeInst(1);
    simple_now->dump(cerr);
    simple_now->indom()->dump(cerr);

    cerr << endl << "*** Updating indom ***" << endl;
    if (simple_now->indom()->changed())
	simple_now->updateIndom();
    else
	cerr << "Nothing to update!" << endl;

    simple_now->dump(cerr);
    simple_now->indom()->dump(cerr);

    cerr << endl << "*** Group 2 Fetch 5 ***" << endl;
    group2.fetch();
    simple_now->dump(cerr);
    
    cerr << endl << "*** Add another metric with the same indom ***" << endl;
    QmcMetric* simple_now2 = group2.addMetric("simple.now");
    simple_now2->dump(cerr);
    simple_now2->indom()->dump(cerr);

    cerr << endl << "*** Group 2 Fetch 6 ***" << endl;
    group2.fetch();
    simple_now->dump(cerr);
    simple_now2->dump(cerr);
       
    cerr << endl << "*** Add an instance to the PMDA ***" << endl;
    changeConf("sec,min,hour");

    cerr << endl << "*** Group 2 Fetch 7 ***" << endl;
    group2.fetch();
    simple_now->dump(cerr);
    simple_now2->dump(cerr);

    cerr << endl << "*** Updating indom ***" << endl;
    if (simple_now2->indom()->changed())
	simple_now2->updateIndom();
    else
	cerr << "Nothing to update!" << endl;

    simple_now2->dump(cerr);
    simple_now2->indom()->dump(cerr);

    cerr << endl << "*** Group 2 Fetch 8 ***" << endl;
    group2.fetch();
    simple_now->dump(cerr);
    simple_now2->dump(cerr);

    cerr << endl << "*** Exiting ***" << endl;
    return sts;
}
