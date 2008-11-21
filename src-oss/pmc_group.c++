//
// Test PMC_Group
// This illustrates the functionality needed for pmjd
// It creates three groups representing three clients
// Two groups are using live contexts, one group is using archives.
//

#include "Group.h"
#include "Source.h"
#include "Metric.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif

#define mesg(str)	msg(__LINE__, str)
#define checksts()	pmflush(); if (sts < 0) fail(__LINE__, sts);

//
// Dummy pmjd client class - should be replaced by real class
// which is installed with pcp_noship
//

class Client
{
private:

    PMC_Group*		_group;
    PMC_MetricList	_metrics;

    enum KeyWords { keyArch, keyContext, keyDesc, keyError, keyFetch,
		    keyHost, keyIndom, keyJump, keyList, keyMetric,
		    keyName, keyReal, keyString, keyText, keyUpdate,
		    keyWipe };

    static char const*	keywords[];
    static char const*	terminator;
    static char const	sep;
    static char const	cont;

public:

    ~Client()
	{ delete _group; }

    Client()
	{ _group = new PMC_Group; }

    PMC_Group const& group() const
    	{ return *_group; }

    // Represents a request for a context handle
    int context(int type, char const* source);

    // Represents a namespace lookup
    int name(int context, char const* metric);

    // Represents a help text lookup
    int text(int context, char const* metric);

    // Represents a list of specs
    int list(PMC_StrList const& list);

    // Represents a jump to a time and interval for archives
    int jump(int interval, int startSec, int startMSec);

    // Represents a fetch
    int fetch();

    // Represents a wipe
    int wipe();

    // Represents an update of dynamic indoms
    int update(PMC_IntList const& list);

    // Dump out some debug stuff
    void dump(ostream& os)
	{ _group->dump(os); }

    friend ostream& operator<<(ostream& os, struct timeval const& tv);
};

ostream&
operator<<(ostream& os, struct timeval const& tv)
{
    os << tv.tv_sec << Client::sep << tv.tv_usec;
    return os;
}

char const* Client::keywords[] = { "ARCH", "CONTEXT", "DESC", "ERROR",
				   "FETCH", "HOST", "INDOM", "JUMP", "LIST",
				   "METRIC", "NAME", "REAL", "STRING",
				   "TEXT", "UPDATE", "WIPE" };

char const* Client::terminator = ".\n";
char const Client::sep = ' ';
char const Client::cont = '-';

int
Client::context(int type, char const* source)
{
    uint_t	numContexts = _group->numContexts();
    int		sts = _group->use(type, source);

    if (sts >= 0) {
	sts = _group->whichIndex();
	cout << keywords[keyContext] << sep << sts;
	// We created a new context
	if (_group->numContexts() > numContexts) {
	    PMC_Context const& context = _group->context(sts);
	    cout << sep << context.numIndoms() << sep << context.numIDs()
		 << sep << context.numMetrics() << sep;
	    if (context.source().type() == PM_CONTEXT_HOST)
		cout << keywords[keyHost] << sep << context.source().host() 
		     << sep << context.source().timezone() << endl;
	    else
		cout << keywords[keyArch] << sep << context.source().host() 
		     << sep << context.source().source() << sep
		     << context.source().start() << sep
		     << context.source().end() << sep
		     << context.source().timezone() << endl;
	}
	cout << terminator;
    }
    else {
	cout << "ERROR " << sts << endl << terminator;
    }
    return sts;
}

int
Client::name(int context, char const* metric)
{
    int		sts = 0;
    PMC_StrList	list;
    uint_t	i;
    
    if (context < 0 || context >= _group->numContexts())
	sts = PM_ERR_NOCONTEXT;

    if (sts >= 0)
	sts = _group->use(context);

    if (sts >= 0)
	sts = _group->which()->traverse(metric, list);

    if (sts < 0)
	cout << keywords[keyError] << sep << sts << endl << terminator;
    else {
	cout << keywords[keyName] << sep << list.length() << endl;
	for (i = 0; i < list.length(); i++)
	    cout << cont << sep << list[i] << endl;
	cout << terminator;
    }

    return sts;
}

int
Client::text(int context, char const* metric)
{
    int		sts = 0;
    pmID	id;
    char*	buf;

    if (context < 0 || context >= _group->numContexts())
	sts = PM_ERR_NOCONTEXT;

    if (sts >= 0) 
	sts = _group->use(context);

    if (sts >= 0)
	sts = _group->which()->lookupDesc(metric, id);

    if (sts >= 0)
	sts = pmLookupText(id, PM_TEXT_HELP, &buf);

    if (sts >= 0) {
	cout << keywords[keyText] << sep << strlen(buf) << endl;
	cout << buf << endl;
	cout << terminator;
	free(buf);
    }
    else
	cout << keywords[keyError] << sep << sts << endl << terminator;

    _group->useDefault();

    return sts;
}

int
Client::list(PMC_StrList const& list)
{
    int		sts = 0;
    uint_t	i;
    uint_t	j;
    uint_t	k;
    uint_t	numContexts = _group->numContexts();
    uint_t	numMetrics = _metrics.length();
    
    _group->useDefault();

    for (i = 0; i < list.length(); i++)
	_metrics.append(_group->addMetric(list[i].ptr(), 0.0, PMC_false));

// TODO - reset, how will this work if we accept CONTEXTs separately?

    cout << keywords[keyList] << sep << _group->numContexts() << endl;

    for (i = 0; i < _group->numContexts(); i++) {

	PMC_Context const& context = _group->context(i);

	cout << keywords[keyContext] << sep << i << sep 
	     << context.numIndoms() << sep << context.numIDs() << sep 
	     << context.numMetrics();

	// Client has not seen this context before
	if (i >= numContexts) {
	    if (context.source().type() == PM_CONTEXT_HOST)
		cout << sep << keywords[keyHost] << sep 
		     << context.source().host() << sep
		     << context.source().timezone() << endl;
	    else
		cout << sep << keywords[keyArch] << sep 
		     << context.source().host() << sep
		     << context.source().source() << sep
		     << context.source().start() << sep
		     << context.source().end() << sep
		     << context.source().timezone() << endl;
	}
	else
	    cout << endl;

	// Dump all indoms in context, but only list instances in use
	for (j = 0; j < context.numIndoms(); j++) {

	    PMC_Indom const& indom = context.indom(j);

	    cout << keywords[keyIndom] << sep << j << sep << indom.refCount() 
		 << endl;
	    for (k = 0; k < indom.listLen(); k++) {
		if (!indom.nullInst(k) && indom.refInst(k))
		    cout << cont << sep << k << sep << indom.name(k) << endl;
	    }
	}

	// Dump all descriptors in use
	for (j = 0; j < context.numIDs(); j++) {

	    pmID id = context.id(j);

	    for (k = j; k < context.numDesc(); k++)
		if (context.desc(k).desc().pmid == id) {
		    PMC_Desc const& desc = context.desc(k);
		    cout << keywords[keyDesc] << sep << j << sep;
		    if (desc.desc().type == PM_TYPE_STRING)
			cout << keywords[keyString];
		    else
			cout << keywords[keyReal];
		    cout << sep << desc.units() << endl;
		    break;
		}
	}
    }

    // Dump new metrics listed by client
    for (i = numMetrics; i < _metrics.length(); i++) {

	PMC_Metric const& metric = *_metrics[i];

	cout << keywords[keyMetric] << sep << i << sep;

	if (metric.status() < 0) {
	    cout << metric.status() << endl;
	}
	else {
	    cout << metric.numValues() << sep << metric.name() << sep 
		 << metric.contextIndex()
		 << sep << metric.idIndex() << sep;
	    if (metric.indomIndex() == UINT_MAX)
		cout << "-1" << endl;
	    else
		cout << metric.indomIndex() << endl;
	    for (j = 0; j < metric.numInst(); j++)
		cout << cont << sep << metric.instIndex(j) << endl;
	}

    }

    cout << terminator;

    return sts;
}

int
Client::jump(int interval, int startSec, int startMSec)
{
    int sts;
    struct timeval start;

    start.tv_sec = startSec;
    start.tv_usec = startMSec;

    sts = _group->setArchiveMode(PM_MODE_INTERP, &start, interval);

    if (sts < 0)
	cout << keywords[keyError] << sts << endl << terminator;
    else
	cout << keywords[keyJump] << endl << terminator;

    return sts;
}

int
Client::fetch()
{
    int			sts;
    uint_t		i;
    uint_t		j;

    sts = _group->fetch();

    cout << keywords[keyFetch] << sep << _group->numContexts() << sep 
	 << _metrics.length() << endl;

    for (i = 0; i < _group->numContexts(); i++) {

	PMC_Context const& context = _group->context(i);

	cout << keywords[keyContext] << sep << i << sep 
	     << (long long)(context.timeDelta() * 1000.0) << sep
	     << context.timeStamp() << endl;
    }

    for (i = 0; i < _metrics.length(); i++) {

	PMC_Metric const& metric = *_metrics[i];

	cout << keywords[keyMetric] << sep << i << sep;

	if (metric.status() < 0) {
	    cout << metric.status() << endl;
	}
	else {
	    cout << metric.numValues();
	    if (metric.hasInstances() && metric.indom()->changed())
		cout << sep << keywords[keyUpdate];
	    cout << endl;
	    for (j = 0; j < metric.numValues(); j++) {
		cout << cont << sep;
		if (metric.error(j) < 0)
		    cout << '?' << endl;
		else {
		    if (metric.desc().desc().type == PM_TYPE_STRING)
			cout << metric.strValue(j);
		    else
			cout << metric.value(j);
		    cout << endl;
		}
	    }
	}

    }

    cout << terminator;

    return sts;
}

int
Client::wipe()
{
    _metrics.removeAll();

    delete _group;
    _group = new PMC_Group;

    cout << keywords[keyWipe] << endl << terminator;

    return 0;
}

int
Client::update(PMC_IntList const& list)
{
    int		sts = 0;
    PMC_IntList	contexts;
    PMC_IntList	indoms;
    PMC_IntList	metrics;
    uint_t	i;
    uint_t	j;
    uint_t	k;
    
    // Generate unique list of contexts for updated metrics
    for (i = 0; i < list.length(); i++) {
	int index = list[i];

	if (index < 0 || index >= _metrics.length()) {
	    sts = PM_ERR_PMID;
	    break;
	}

	uint_t ctx = _metrics[index]->contextIndex();

	for (j = 0; j < contexts.length(); j++)
	    if (contexts[j] == ctx)
		break;
	if (j == contexts.length())
	    contexts.append(ctx);
    }

    if (sts >= 0) {
	cout << keywords[keyUpdate] << sep << contexts.length() << endl;

	for (i = 0; i < contexts.length(); i++) {
	    uint_t cntx = contexts[i];
	    PMC_Context& context = _group->context(cntx);

	    // Generate unique list of updated metrics for this context
	    metrics.removeAll();
	    for (j = 0; j < list.length(); j++) {
		uint_t index = list[j];
		if (_metrics[index]->status() >= 0 &&
		    _metrics[index]->contextIndex() == cntx)
		    metrics.append(index);
	    }

	    // Generate unique list of updated indoms for this context
	    indoms.removeAll();

	    for (j = 0; j < metrics.length(); j++) {

		uint_t indm = _metrics[metrics[j]]->indomIndex();
		for (k = 0; k < indoms.length(); k++)
		    if (indoms[k] == indm)
			break;
		if (k == indoms.length())
		    indoms.append(indm);

		_metrics[metrics[j]]->updateIndom();
	    }

	    cout << keywords[keyContext] << sep << cntx << sep
		<< indoms.length() << sep << list.length() << endl;

	    for (j = 0; j < indoms.length(); j++) {
		PMC_Indom const& indom = context.indom(indoms[j]);

		cout << keywords[keyIndom] << sep << indoms[j] << sep 
		    << indom.refCount() << endl;
		for (k = 0; k < indom.listLen(); k++) {
		    if (!indom.nullInst(k) && indom.refInst(k))
			cout << cont << sep << k << sep << indom.name(k) << endl;
		}
	    }
	}

	for (i = 0; i < list.length(); i++) {
	    PMC_Metric const& metric = *_metrics[list[i]];

	    cout << keywords[keyMetric] << sep << list[i] << sep;

	    if (metric.status() < 0) {
		cout << metric.status() << endl;
	    }
	    else {
		cout << metric.numValues() << endl;
		for (j = 0; j < metric.numInst(); j++)
		    cout << cont << sep << metric.instIndex(j) << endl;
	    }
	}
    }
    else
	cout << keywords[keyError] << sep << sts << endl;

    cout << terminator;

    return sts;
}

void
store(char const* name, char const* inst)
{
    char buf[128];

    sprintf(buf, "pmstore %s %s > /dev/null\n", name, inst);
    cout << name << ' ' << inst << endl;
    system(buf);
    system("pminfo -f dynamic");
}

static int msgCount = 1;

void
msg(int line, char const* str)
{

    cout << endl << "*** " << msgCount << ": Line " << line << " - " << str 
	 << " ***" << endl;
    cerr << endl << "*** " << msgCount << ": Line " << line << " - " << str 
	 << " ***" << endl;
    msgCount++;
}

void
fail(int line, int err)
{
    cout << endl << "*** " << msgCount << ": Testing failed at line " << line
	 << " - " << pmErrStr(err) << " ***" << endl;
    cerr << endl << "*** " << msgCount << ": Testing failed at line " << line
	 << " - " << pmErrStr(err) << " ***" << endl;

    pmflush();

    exit(1);
    /*NOTREACHED*/
}

int
main(int argc, char* argv[])
{
    int		sts = 0;
    int		c;
    PMC_String	archive1 = "snort-disks";
    			// snort-disks timestamps
			// 1117075022.613953 ... 1117075050.309912
    PMC_String	archive2 = "vldb-disks";
    			// vldb-disks timestamps
			// 869629190.357184 ... 869629210.660548
    PMC_String	archive3 = "moomba.pmkstat";
    PMC_StrList	metrics;
    PMC_IntList	metricIds;

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

    if (sts || optind != argc) {
	pmprintf("Usage: %s\n", pmProgname);
	pmflush();
	exit(1);
        /*NOTREACHED*/
    }

    store("dynamic.control.del", "-1");

    //
    // Client 1 uses live contexts to the same host as pmjd
    //

    mesg("Client1: VERSION version");
    Client* client1 = new Client;

    mesg("Client1: LIST 2 hinv.ncpu kernel.all.load[1,15]");
    metrics.removeAll();
    metrics.append("hinv.ncpu");
    metrics.append("kernel.all.load[1,15]");
    sts = client1->list(metrics);

    checksts();

    mesg("Client1: FETCH");
    sts = client1->fetch();

    checksts();

    //
    // Client 2 uses live contexts to multiple hosts
    // Client 2 does some namespace and helptext lookups first
    //

    mesg("Client2: VERSION version");
    Client* client2 = new Client;


    mesg("Client2: CONTEXT HOST localhost");
    sts = client2->context(PM_CONTEXT_HOST, "localhost");

    checksts();

    int ctx = sts;

    mesg("Client2: NAME 0 dynamic");
    sts = client2->name(ctx, "dynamic");

    checksts();

    mesg("Client2: TEXT 0 dynamic.numinsts");
    client2->text(ctx, "dynamic.numinsts");

    checksts();

    //
    // Another fetch from Client 1
    //

    mesg("Client1: FETCH");
    sts = client1->fetch();

    checksts();

    //
    // Client 3 uses archives contexts
    //

    mesg("Client3: VERSION version");
    Client* client3 = new Client;

    mesg("Client3: CONTEXT ARCH archive1");
    sts = client3->context(PM_CONTEXT_ARCHIVE, archive1.ptr());

    checksts();

    mesg("Client3: CONTEXT ARCH archive2");
    sts = client3->context(PM_CONTEXT_ARCHIVE, archive2.ptr());

    checksts();

    //
    // Client 1 does a fetch
    //

    mesg("Client1: FETCH");
    sts = client1->fetch();

    checksts();
    
    //
    // Client 2 sets up its metrics
    //

    mesg("Client2: LIST 4 dynamic.numinsts dynamic.discrete dynamic.instant dynamic.counter");
    metrics.removeAll();
    metrics.append("dynamic.numinsts");
    metrics.append("dynamic.discrete");
    metrics.append("dynamic.instant");
    metrics.append("dynamic.counter");
    sts = client2->list(metrics);

    checksts();

    mesg("Client2: FETCH");
    sts = client2->fetch();

    checksts();

    //
    // Client 3 specifies some metrics using the archive host and source
    // names
    //

    mesg("Client3: LIST 4 snort:disk.dev.read[dks0d1,dks1d1,dks9d1] vldb-disks/irix.disk.dev.total[dks17d8,dks11d3,dks45d2] disk.dev.write[dks1d1,dks0d4] vldb.engr:irix.disk.dev.total[dks18d6,dks11d3]");

    metrics.removeAll();
    metrics.append("snort:disk.dev.read[dks0d1,dks1d1,dks9d1]");
    metrics.append("vldb-disks/irix.disk.dev.total[dks17d8,dks11d3,dks45d2]");
    metrics.append("disk.dev.write[dks1d1,dks0d4]");
    metrics.append("vldb.engr:irix.disk.dev.total[dks18d6,dks11d3]");

    sts = client3->list(metrics);

    checksts();

    mesg("Client3: JUMP 5000 869629200 0");
    sts = client3->jump(5000, 869629200, 0);

    checksts();

    mesg("Client3: FETCH");
    sts = client3->fetch();

    checksts();

    mesg("Client3: FETCH");
    sts = client3->fetch();

    checksts();

    //
    // Client 1 now wants to add some more metrics
    //

    mesg("Client1: LIST 2 hinv.ndisk disk.all.total");
    metrics.removeAll();
    metrics.append("hinv.ndisk");
    metrics.append("disk.all.total");
    sts = client1->list(metrics);

    checksts();

    mesg("Client1: FETCH");
    sts = client1->fetch();

    checksts();

    mesg("Client1: FETCH");
    sts = client1->fetch();

    checksts();

    //
    // Client 3 jumps into the next archive
    //

    mesg("Client3: JUMP 2000 1117075022 0 ");
    sts = client3->jump(2000, 1117075022, 0);

    checksts();

    mesg("Client3: FETCH");
    sts = client3->fetch();

    checksts();

    mesg("Client3: FETCH");
    sts = client3->fetch();

    checksts();

    mesg("Client3: FETCH");
    sts = client3->fetch();

    checksts();

    //
    // Client 1 wants to start afresh with some archive contexts
    //

    mesg("Client1: WIPE");
    sts = client1->wipe();

    checksts();

    mesg("Client1: FETCH");
    sts = client1->fetch();

    checksts();

    //
    // Client 2 gets a fetch in but the indom has changed
    //

    mesg("Dynamic Indom Changes");
    store("dynamic.control.add", "2");
    store("dynamic.control.add", "4");

    mesg("Client2: FETCH");
    sts = client2->fetch();

    checksts();

    mesg("Client2: UPDATE 3 1 2 3");
    metricIds.removeAll();
    metricIds.append(1);
    metricIds.append(2);
    metricIds.append(3);
    sts = client2->update(metricIds);

    checksts();

    mesg("Client2: FETCH");
    sts = client2->fetch();

    checksts();

    mesg("Client2: FETCH");
    sts = client2->fetch();

    checksts();

    //
    // Client 1 adds some archive metrics
    //

    mesg("Client1: CONTEXT ARCH moomba.pmkstat");
    sts = client1->context(PM_CONTEXT_ARCHIVE, archive3.ptr());

    checksts();

    mesg("Client1: LIST 2 kernel.all.idle localhost:kernel.all.cpu.user");
    metrics.removeAll();
    metrics.append("irix.kernel.all.cpu.idle");
    metrics.append("moomba:irix.kernel.all.cpu.user");
    sts = client1->list(metrics);

    checksts();

    mesg("Client1: JUMP 5000 885849650 0");
    sts = client1->jump(5000, 885849650, 0);

    checksts();

    mesg("Client1: FETCH");
    sts = client1->fetch();

    checksts();

    mesg("Client1: FETCH");
    sts = client1->fetch();

    checksts();

    // Client 3 gets a fetch in
    mesg("Client3: FETCH");
    sts = client3->fetch();

    checksts();


    mesg("Exiting");
    pmflush();
    return sts;
}
