//
// Test Qt Metrics Class dynamic indom support
//

#include <errno.h>
#include <QTextStream>
#include <qmc_context.h>
#include <qmc_group.h>
#include <qmc_metric.h>
#include <qmc_indom.h>

#define ADD_INST	"dynamic.control.add"
#define DEL_INST	"dynamic.control.del"
#define mesg(str)	msg(__LINE__, str)

QTextStream cerr(stderr);
QTextStream cout(stdout);

void
store(char const* name, char const* inst)
{
    char buf[128];

    sprintf(buf, "pmstore %s %s > /dev/null\n", name, inst);
    cout << name << ' ' << inst << Qt::endl;
    if (system(buf) < 0) {
	pmprintf("%s: cannot run system(%s)\n", pmGetProgname(), buf);
	pmflush();
	exit(1);
    }
}

void
dump(QmcMetric const* num, QmcMetric const* discrete, 
     QmcMetric const* instant, QmcMetric const* counter)
{
    cout << "dynamic.numinsts = " << num->value(0) << Qt::endl << Qt::endl;
    for (int i = 0; i < discrete->numInst(); i++) {
	cout << '[' << discrete->instName(i) << "] = ";
	if (discrete->error(i) < 0) {
	    cout << pmErrStr(discrete->error(i)) << Qt::endl;
	}
	else {
	    cout << '\"' << discrete->stringValue(i) << "\" = " 
		 << instant->value(i) << " (";
	    if (counter->error(i) < 0)
		cout << pmErrStr(counter->error(i)) << ")" << Qt::endl;
	    else
		cout << counter->currentValue(i) << ")" << Qt::endl;
	}
    }
    cout << Qt::endl;
    discrete->indom()->dump(cout);
}

void
update(QmcMetric* discrete, QmcMetric* instant, 
       QmcMetric* counter)
{
    if (discrete->indom()->changed())
	discrete->updateIndom();
    if (instant->indom()->changed())
	instant->updateIndom();
    if (counter->indom()->changed())
	counter->updateIndom();
}

void
msg(int line, char const* str)
{
    static int count = 1;

    cout << Qt::endl << "*** " << count << ": Line " << line << " - " << str 
	 << " ***" << Qt::endl;
    cerr << Qt::endl << "*** " << count << ": Line " << line << " - " << str 
	 << " ***" << Qt::endl;
    count++;
}

int
main(int argc, char* argv[])
{
    int		sts = 0;
    int		c;
    char	buf[MAXHOSTNAMELEN];

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

    mesg("Create two fetch groups");
    QmcGroup group1;
    pmflush();
    QmcGroup group2;
    pmflush();

    mesg("Add number of instances to both groups");
    QmcMetric* numinsts1 = group1.addMetric("dynamic.numinsts");
    pmflush();
    if (numinsts1->status() < 0)
	exit(1);
    else
	numinsts1->dump(cout);

    QmcMetric* numinsts2 = group2.addMetric("dynamic.numinsts");
    pmflush();
    if (numinsts2->status() < 0)
	exit(1);
    else
	numinsts2->dump(cout);

    mesg("Fetch both groups");
    group1.fetch();
    numinsts1->dump(cout);
    group2.fetch();
    numinsts2->dump(cout);

    mesg("Add dynamic metrics to both groups");
    QmcMetric* discrete1 = group1.addMetric("dynamic.discrete", 0.0, true);
    pmflush();
    if (discrete1->status() < 0)
	exit(1);
    else
	discrete1->dump(cout);

    QmcMetric* instant1 = group1.addMetric("dynamic.instant", 0.0, true);
    pmflush();
    if (instant1->status() < 0)
	exit(1);
    else
	instant1->dump(cout);

    QmcMetric* counter1 = group1.addMetric("dynamic.counter", 0.0, true);
    pmflush();
    if (counter1->status() < 0)
	exit(1);
    else
	counter1->dump(cout);

    QmcMetric* discrete2 = group2.addMetric("dynamic.discrete");
    pmflush();
    if (discrete2->status() < 0)
	exit(1);
    else
	discrete2->dump(cout);

    QmcMetric* instant2 = group2.addMetric("dynamic.instant");
    pmflush();
    if (instant2->status() < 0)
	exit(1);
    else
	instant2->dump(cout);

    QmcMetric* counter2 = group2.addMetric("dynamic.counter");
    pmflush();
    if (counter2->status() < 0)
	exit(1);
    else
	counter2->dump(cout);

    mesg("Fetch both groups");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);
    group2.fetch();
    dump(numinsts2, discrete2, instant2, counter2);

    mesg("Add an instance");
    store(ADD_INST, "1");

    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Update indom for first group");
    update(discrete1, instant1, counter1);

    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Add another instance");
    store(ADD_INST, "5");
    
    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Update indom for first group");
    update(discrete1, instant1, counter1);

    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Delete first instance");
    store(DEL_INST, "1");
    
    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Update indom for first group");
    update(discrete1, instant1, counter1);

    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Fetch second group");
    group2.fetch();
    dump(numinsts2, discrete2, instant2, counter2);

    mesg("Update indom for second group");
    update(discrete2, instant2, counter2);

    mesg("Fetch second group");
    group2.fetch();
    dump(numinsts2, discrete2, instant2, counter2);

    mesg("Fetch second group");
    group2.fetch();
    dump(numinsts2, discrete2, instant2, counter2);

    mesg("Delete second instance, add new instance");
    store(DEL_INST, "5");
    store(ADD_INST, "3");
    
    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Update indom for first group");
    update(discrete1, instant1, counter1);

    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Fetch second group");
    group2.fetch();
    dump(numinsts2, discrete2, instant2, counter2);

    mesg("Update indom for second group");
    update(discrete2, instant2, counter2);

    mesg("Fetch second group");
    group2.fetch();
    dump(numinsts2, discrete2, instant2, counter2);

    mesg("Fetch second group");
    group2.fetch();
    dump(numinsts2, discrete2, instant2, counter2);

    mesg("Delete third instance");
    store(DEL_INST, "3");
    
    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Update indom for first group");
    update(discrete1, instant1, counter1);

    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Fetch second group");
    group2.fetch();
    dump(numinsts2, discrete2, instant2, counter2);

    mesg("Update indom for second group");
    update(discrete2, instant2, counter2);

    mesg("Fetch second group");
    group2.fetch();
    dump(numinsts2, discrete2, instant2, counter2);

    mesg("Add second instance again");
    store(ADD_INST, "5");
    
    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Update indom for first group");
    update(discrete1, instant1, counter1);

    mesg("Fetch first group");
    group1.fetch();
    dump(numinsts1, discrete1, instant1, counter1);

    mesg("Fetch second group");
    group2.fetch();
    dump(numinsts2, discrete2, instant2, counter2);

    mesg("Update indom for second group");
    update(discrete2, instant2, counter2);

    mesg("Fetch second group");
    group2.fetch();
    dump(numinsts2, discrete2, instant2, counter2);

    return 0;
}

