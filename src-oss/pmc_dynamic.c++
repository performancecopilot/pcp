//
// Test libpcp_pmc dynamic indom support
//

#include <iostream.h>
#include <errno.h>
#include "Group.h"
#include "Metric.h"
#include "Indom.h"

#define ADD_INST	"dynamic.control.add"
#define DEL_INST	"dynamic.control.del"
#define mesg(str)	msg(__LINE__, str)

void
store(char const* name, char const* inst)
{
    char buf[128];

    sprintf(buf, "pmstore %s %s > /dev/null\n", name, inst);
    cout << name << ' ' << inst << endl;
    system(buf);
}

void
dump(PMC_Metric const* num, PMC_Metric const* discrete, 
     PMC_Metric const* instant, PMC_Metric const* counter)
{
    uint_t	i;

    cout << "dynamic.numinsts = " << num->value(0) << endl << endl;
    for (i = 0; i < discrete->numInst(); i++) {
	cout << '[' << discrete->instName(i) << "] = ";
	if (discrete->error(i) < 0) {
	    cout << pmErrStr(discrete->error(i)) << endl;
	}
	else {
	    cout << '\"' << discrete->strValue(i) << "\" = " 
		 << instant->value(i) << " (";
	    if (counter->error(i) < 0)
		cout << pmErrStr(counter->error(i)) << ")" << endl;
	    else
		cout << counter->currValue(i) << ")" << endl;
	}
    }
    cout << endl;
    discrete->indom()->dump(cout);
}

void
update(PMC_Metric* discrete, PMC_Metric* instant, 
       PMC_Metric* counter)
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

    cout << endl << "*** " << count << ": Line " << line << " - " << str 
	 << " ***" << endl;
    cerr << endl << "*** " << count << ": Line " << line << " - " << str 
	 << " ***" << endl;
    count++;
}

int
main(int argc, char* argv[])
{
    int		sts = 0;
    int		c;
    char	buf[MAXHOSTNAMELEN];

    pmProgname = basename(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {
	case 'D':
	    sts = __pmParseDebug(optarg);
            if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			 pmProgname, optarg);
                sts = 1;
            }
            else {
                pmDebug |= sts;
		sts = 0;
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

    (void)gethostname(buf, MAXHOSTNAMELEN);
    buf[MAXHOSTNAMELEN-1] = '\0';

    mesg("Create two fetch groups");
    PMC_Group group1;
    pmflush();
    PMC_Group group2;
    pmflush();

    mesg("Add number of instances to both groups");
    PMC_Metric* numinsts1 = group1.addMetric("dynamic.numinsts");
    pmflush();
    if (numinsts1->status() < 0)
	exit(1);
    else
	numinsts1->dump(cout);

    PMC_Metric* numinsts2 = group2.addMetric("dynamic.numinsts");
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
    PMC_Metric* discrete1 = group1.addMetric("dynamic.discrete", 0.0, PMC_true);
    pmflush();
    if (discrete1->status() < 0)
	exit(1);
    else
	discrete1->dump(cout);

    PMC_Metric* instant1 = group1.addMetric("dynamic.instant", 0.0, PMC_true);
    pmflush();
    if (instant1->status() < 0)
	exit(1);
    else
	instant1->dump(cout);

    PMC_Metric* counter1 = group1.addMetric("dynamic.counter", 0.0, PMC_true);
    pmflush();
    if (counter1->status() < 0)
	exit(1);
    else
	counter1->dump(cout);

    PMC_Metric* discrete2 = group2.addMetric("dynamic.discrete");
    pmflush();
    if (discrete2->status() < 0)
	exit(1);
    else
	discrete2->dump(cout);

    PMC_Metric* instant2 = group2.addMetric("dynamic.instant");
    pmflush();
    if (instant2->status() < 0)
	exit(1);
    else
	instant2->dump(cout);

    PMC_Metric* counter2 = group2.addMetric("dynamic.counter");
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

