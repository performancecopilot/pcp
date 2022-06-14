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
	cerr << pmGetProgname() << ": /var/pcp/pmdas/simple/simple.conf: "
	     << strerror(errno) << Qt::endl;
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

    cerr << "*** Create a single fetch group ***" << Qt::endl;
    QmcGroup group1;
    pmflush();

    cerr << Qt::endl << "*** Simple metric ***" << Qt::endl;
    QmcMetric* hinv_ncpu = group1.addMetric("hinv.ncpu");
    pmflush();
    
    if (hinv_ncpu->status() < 0)
	sts = 1;
    else
	hinv_ncpu->dump(cerr);

    cerr << Qt::endl << "*** Metric with an Indom ***" << Qt::endl;
    QmcMetric* percpu_user = group1.addMetric("sample.bin");
    pmflush();

    if (percpu_user->status() < 0)
	sts = 1;
    else
	percpu_user->dump(cerr);

    cerr << Qt::endl << "*** proc style specific instance ***" << Qt::endl;
    QmcMetric* load_avg = group1.addMetric("kernel.all.load[1,5]");
    pmflush();
    
    if (load_avg->status() < 0)
	sts = 1;
    else
	load_avg->dump(cerr);

    cerr << Qt::endl << "*** String metric ***" << Qt::endl;
    QmcMetric* sample_hullo = group1.addMetric("sample.string.hullo");

    if (sample_hullo->status() < 0)
	sts = 1;
    else
	sample_hullo->dump(cerr);

    cerr << Qt::endl << "*** Rate converted metric ***" << Qt::endl;
    QmcMetric* sample_seconds = group1.addMetric("sample.seconds");

    if (sample_seconds->status() < 0)
	sts = 1;
    else
	sample_seconds->dump(cerr);

    cerr << Qt::endl << "*** Bogus metric ***" << Qt::endl;
    QmcMetric* bogus_metric = group1.addMetric("bogus.metric");
    pmflush();

    if (bogus_metric->status() >= 0) {
	pmprintf("%s: Error: bogus.metric was not invalid!\n",
		 pmGetProgname());
	pmflush();
	sts = 1;
    }
	
    cerr << Qt::endl << "*** Bogus instance ***" << Qt::endl;
    QmcMetric* bogus_inst = group1.addMetric("kernel.all.load[2]");
    pmflush();
    
    if (bogus_inst->status() >= 0) {
	pmprintf("%s: Error: kernel.all.load[2] was not invalid!\n",
		 pmGetProgname());
	pmflush();
	sts = 1;
    }

    pmflush();

    sleep(1);

    cerr << Qt::endl << "*** Group 1 Fetch 1 ***" << Qt::endl;
    group1.fetch();
    hinv_ncpu->dump(cerr);
    percpu_user->dump(cerr);
    load_avg->dump(cerr);
    sample_hullo->dump(cerr);
    sample_seconds->dump(cerr);

    sleep(1);

    cerr << Qt::endl << "*** Group 1 Fetch 2 ***" << Qt::endl;
    group1.fetch();
    hinv_ncpu->dump(cerr);
    percpu_user->dump(cerr);
    load_avg->dump(cerr);
    sample_hullo->dump(cerr);
    sample_seconds->dump(cerr);

    cerr << Qt::endl << "*** Remove an instance ***" << Qt::endl;
    load_avg->removeInst(0);
    load_avg->dump(cerr);

    sleep(1);
    
    cerr << Qt::endl << "*** Group 1 Fetch 3 ***" << Qt::endl;
    group1.fetch();
    hinv_ncpu->dump(cerr);
    percpu_user->dump(cerr);
    load_avg->dump(cerr);
    sample_hullo->dump(cerr);
    sample_seconds->dump(cerr);

    cerr << Qt::endl << "*** Add an instance ***" << Qt::endl;
    load_avg->addInst("15");
    load_avg->dump(cerr);

    sleep(1);
    
    cerr << Qt::endl << "*** Group 1 Fetch 4 ***" << Qt::endl;
    group1.fetch();
    hinv_ncpu->dump(cerr);
    percpu_user->dump(cerr);
    load_avg->dump(cerr);
    sample_hullo->dump(cerr);
    sample_seconds->dump(cerr);

    cerr << Qt::endl << "*** Creating a new group ***" << Qt::endl;
    QmcGroup group2;

    cerr << Qt::endl << "*** Adding a metric with a dynamic indom ***" << Qt::endl;
    QmcMetric* simple_now = group2.addMetric("simple.now");
    pmflush();
    simple_now->dump(cerr);

    cerr << Qt::endl << "*** Group 2 Fetch 1 ***" << Qt::endl;
    group2.fetch();
    simple_now->dump(cerr);

    cerr << Qt::endl << "*** Change the indom ***" << Qt::endl;
    changeConf("sec,min,hour");

    cerr << Qt::endl << "*** Group 2 Fetch 2 ***" << Qt::endl;
    group2.fetch();
    simple_now->dump(cerr);

    cerr << Qt::endl << "*** Updating indom ***" << Qt::endl;
    if (simple_now->indom()->changed())
	simple_now->updateIndom();
    else
	cerr << "Nothing to update!" << Qt::endl;

    simple_now->dump(cerr);
    simple_now->indom()->dump(cerr);

    cerr << Qt::endl << "*** Group 2 Fetch 3 ***" << Qt::endl;
    group2.fetch();
    simple_now->dump(cerr);

    cerr << Qt::endl << "*** Remove instance from PMDA ***" << Qt::endl;
    changeConf("sec,hour");

    cerr << Qt::endl << "*** Group 2 Fetch 4 ***" << Qt::endl;
    group2.fetch();
    simple_now->dump(cerr);

    cerr << Qt::endl << "*** Remove an instance ***" << Qt::endl;
    simple_now->removeInst(1);
    simple_now->dump(cerr);
    simple_now->indom()->dump(cerr);

    cerr << Qt::endl << "*** Updating indom ***" << Qt::endl;
    if (simple_now->indom()->changed())
	simple_now->updateIndom();
    else
	cerr << "Nothing to update!" << Qt::endl;

    simple_now->dump(cerr);
    simple_now->indom()->dump(cerr);

    cerr << Qt::endl << "*** Group 2 Fetch 5 ***" << Qt::endl;
    group2.fetch();
    simple_now->dump(cerr);
    
    cerr << Qt::endl << "*** Add another metric with the same indom ***" << Qt::endl;
    QmcMetric* simple_now2 = group2.addMetric("simple.now");
    simple_now2->dump(cerr);
    simple_now2->indom()->dump(cerr);

    cerr << Qt::endl << "*** Group 2 Fetch 6 ***" << Qt::endl;
    group2.fetch();
    simple_now->dump(cerr);
    simple_now2->dump(cerr);
       
    cerr << Qt::endl << "*** Add an instance to the PMDA ***" << Qt::endl;
    changeConf("sec,min,hour");

    cerr << Qt::endl << "*** Group 2 Fetch 7 ***" << Qt::endl;
    group2.fetch();
    simple_now->dump(cerr);
    simple_now2->dump(cerr);

    cerr << Qt::endl << "*** Updating indom ***" << Qt::endl;
    if (simple_now2->indom()->changed())
	simple_now2->updateIndom();
    else
	cerr << "Nothing to update!" << Qt::endl;

    simple_now2->dump(cerr);
    simple_now2->indom()->dump(cerr);

    cerr << Qt::endl << "*** Group 2 Fetch 8 ***" << Qt::endl;
    group2.fetch();
    simple_now->dump(cerr);
    simple_now2->dump(cerr);

    cerr << Qt::endl << "*** Exiting ***" << Qt::endl;
    return sts;
}
