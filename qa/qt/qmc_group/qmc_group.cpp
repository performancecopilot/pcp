//
// Test QmcGroup class
// It creates three groups representing three clients
// Two groups are using live contexts, one group is using archives.
//

#include <QTextStream>
#include <QStringList>
#include <qmc_context.h>
#include <qmc_group.h>
#include <qmc_source.h>
#include <qmc_metric.h>

QTextStream cerr(stderr);
QTextStream cout(stdout);

#define mesg(str)	msg(__LINE__, str)
#define checksts()	pmflush(); if (sts < 0) fail(__LINE__, sts);

class Client
{
private:

    QmcGroup*		_group;
    QList<QmcMetric*>	_metrics;

    enum KeyWords { keyArch, keyContext, keyDesc, keyError, keyFetch,
		    keyHost, keyIndom, keyJump, keyList, keyMetric,
		    keyName, keyReal, keyString, keyEvent,
		    keyText, keyUpdate, keyWipe };

    static char const*	keywords[];
    static char const*	terminator;
    static char const	sep;
    static char const	cont;

public:

    ~Client()
	{ delete _group; }

    Client()
	{ _group = new QmcGroup; }

    QmcGroup const& group() const
    	{ return *_group; }

    // Represents a request for a context handle
    int context(int type, char const* source);

    // Represents a namespace lookup
    int name(int context, char const* metric);

    // Represents a help text lookup
    int text(int context, char const* metric);

    // Represents a list of specs
    int list(QStringList const& list);

    // Represents a jump to a time and interval for archives
    int jump(int interval, int startSec, int startMSec);

    // Represents a fetch
    int fetch();

    // Represents a wipe
    int wipe();

    // Represents an update of dynamic indoms
    int update(QList<int> const& list);

    // Dump out some debug stuff
    void dump(QTextStream& os)
	{ _group->dump(os); }

    friend QTextStream& operator<<(QTextStream& os, struct timeval const& tv);
};

QTextStream&
operator<<(QTextStream& os, struct timeval const& tv)
{
    os << tv.tv_sec << Client::sep << tv.tv_usec;
    return os;
}

char const* Client::keywords[] = { "ARCH", "CONTEXT", "DESC", "ERROR",
				   "FETCH", "HOST", "INDOM", "JUMP", "LIST",
				   "METRIC", "NAME", "REAL", "STRING", "EVENT",
				   "TEXT", "UPDATE", "WIPE" };

char const* Client::terminator = ".\n";
char const Client::sep = ' ';
char const Client::cont = '-';

int
Client::context(int type, char const* source)
{
    uint32_t	numContexts = _group->numContexts();
    QString	qsource = source;
    int		sts = _group->use(type, qsource);

    if (sts >= 0) {
	sts = _group->contextIndex();
	cout << keywords[keyContext] << sep << sts;
	// We created a new context
	if (_group->numContexts() > numContexts) {
	    QmcContext *context = _group->context(sts);
	    cout << sep << context->numIndoms() << sep << context->numIDs()
		 << sep << context->numMetrics() << sep;
	    if (context->source().type() == PM_CONTEXT_HOST)
		cout << keywords[keyHost] << sep << context->source().host() 
		     << sep << context->source().timezone() << Qt::endl;
	    else
		cout << keywords[keyArch] << sep << context->source().host() 
		     << sep << context->source().source() << sep
		     << context->source().start() << sep
		     << context->source().end() << sep
		     << context->source().timezone() << Qt::endl;
	}
	cout << terminator;
    }
    else {
	cout << "ERROR " << sts << Qt::endl << terminator;
    }
    return sts;
}

int
Client::name(int context, char const* metric)
{
    int		sts = 0;
    QStringList	list;
    
    if (context < 0 || context >= (int)_group->numContexts())
	sts = PM_ERR_NOCONTEXT;

    if (sts >= 0)
	sts = _group->use(context);

    if (sts >= 0)
	sts = _group->context()->traverse(metric, list);

    if (sts < 0)
	cout << keywords[keyError] << sep << sts << Qt::endl << terminator;
    else {
	cout << keywords[keyName] << sep << list.size() << Qt::endl;
	for (int i = 0; i < list.size(); i++)
	    cout << cont << sep << list[i] << Qt::endl;
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

    if (context < 0 || context >= (int)_group->numContexts())
	sts = PM_ERR_NOCONTEXT;

    if (sts >= 0) 
	sts = _group->use(context);

    if (sts >= 0)
	sts = _group->context()->lookupPMID(metric, id);

    if (sts >= 0)
	sts = pmLookupText(id, PM_TEXT_HELP, &buf);

    if (sts >= 0) {
	cout << keywords[keyText] << sep << strlen(buf) << Qt::endl;
	cout << buf << Qt::endl;
	cout << terminator;
	free(buf);
    }
    else
	cout << keywords[keyError] << sep << sts << Qt::endl << terminator;

    _group->useDefault();

    return sts;
}

int
Client::list(QStringList const& list)
{
    int		l, sts = 0;
    uint32_t	i;
    uint32_t	j;
    uint32_t	k;
    uint32_t	numContexts = _group->numContexts();
    uint32_t	numMetrics = _metrics.size();
    
    _group->useDefault();

    for (l = 0; l < list.size(); l++)
	_metrics.append(_group->addMetric(list[l].toLatin1().constData(), 0.0, false));

    cout << keywords[keyList] << sep << _group->numContexts() << Qt::endl;

    for (i = 0; i < _group->numContexts(); i++) {

	QmcContext const *context = _group->context(i);

	cout << keywords[keyContext] << sep << i << sep 
	     << context->numIndoms() << sep << context->numIDs() << sep 
	     << context->numMetrics();

	// Client has not seen this context before
	if (i >= numContexts) {
	    if (context->source().type() == PM_CONTEXT_HOST)
		cout << sep << keywords[keyHost] << sep 
		     << context->source().host() << sep
		     << context->source().timezone() << Qt::endl;
	    else
		cout << sep << keywords[keyArch] << sep 
		     << context->source().host() << sep
		     << context->source().source() << sep
		     << context->source().start() << sep
		     << context->source().end() << sep
		     << context->source().timezone() << Qt::endl;
	}
	else
	    cout << Qt::endl;

	// Dump all indoms in context, but only list instances in use
	for (j = 0; j < context->numIndoms(); j++) {

	    QmcIndom const& indom = context->indom(j);

	    cout << keywords[keyIndom] << sep << j << sep << indom.refCount() 
		 << Qt::endl;
	    for (k = 0; k < (uint32_t)indom.listLen(); k++) {
		if (!indom.nullInst(k) && indom.refInst(k))
		    cout << cont << sep << k << sep << indom.name(k) << Qt::endl;
	    }
	}

	// Dump all descriptors in use
	for (j = 0; j < context->numIDs(); j++) {

	    pmID id = context->id(j);
	    QmcDesc const& desc = context->desc(id);

	    cout << keywords[keyDesc] << sep << j << sep;
	    if (desc.desc().type == PM_TYPE_STRING)
		cout << keywords[keyString];
	    else if (desc.desc().type == PM_TYPE_EVENT)
		cout << keywords[keyEvent];
	    else
		cout << keywords[keyReal];
	    cout << sep << desc.units() << Qt::endl;
	}
    }

    // Dump new metrics listed by client
    for (i = numMetrics; i < (uint32_t)_metrics.size(); i++) {

	QmcMetric const& metric = *_metrics[i];

	cout << keywords[keyMetric] << sep << i << sep;

	if (metric.status() < 0) {
	    cout << metric.status() << Qt::endl;
	}
	else {
	    cout << metric.numValues() << sep << metric.name() << sep 
		 << metric.contextIndex()
		 << sep << metric.idIndex() << sep;
	    if (metric.indomIndex() == UINT_MAX)
		cout << "-1" << Qt::endl;
	    else
		cout << metric.indomIndex() << Qt::endl;
	    for (j = 0; j < (uint32_t)metric.numInst(); j++)
		cout << cont << sep << metric.instIndex(j) << Qt::endl;
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
	cout << keywords[keyError] << sts << Qt::endl << terminator;
    else
	cout << keywords[keyJump] << Qt::endl << terminator;

    return sts;
}

int
Client::fetch()
{
    int		sts, i, j;

    sts = _group->fetch();

    cout << keywords[keyFetch] << sep << _group->numContexts() << sep 
	 << _metrics.size() << Qt::endl;

    for (i = 0; i < (int)_group->numContexts(); i++) {

	QmcContext const *context = _group->context(i);

	cout << keywords[keyContext] << sep << i << sep 
	     << (long long)(context->timeDelta() * 1000.0) << sep
	     << context->timeStamp() << Qt::endl;
    }

    for (i = 0; i < _metrics.size(); i++) {

	QmcMetric const& metric = *_metrics[i];

	cout << keywords[keyMetric] << sep << i << sep;

	if (metric.status() < 0) {
	    cout << metric.status() << Qt::endl;
	}
	else {
	    cout << metric.numValues();
	    if (metric.hasInstances() && metric.indom()->changed())
		cout << sep << keywords[keyUpdate];
	    cout << Qt::endl;
	    for (j = 0; j < metric.numValues(); j++) {
		cout << cont << sep;
		if (metric.error(j) < 0)
		    cout << '?' << Qt::endl;
		else {
		    int type = metric.desc().desc().type;
		    if (QmcMetric::real(type))
			cout << metric.value(j);
		    else if (QmcMetric::event(type))
			metric.dump(cout, j);
		    else
			cout << metric.stringValue(j);
		    cout << Qt::endl;
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
    _metrics.clear();

    delete _group;
    _group = new QmcGroup;

    cout << keywords[keyWipe] << Qt::endl << terminator;

    return 0;
}

int
Client::update(QList<int> const& list)
{
    int		sts = 0;
    QList<int>	contexts;
    QList<int>	indoms;
    QList<int>	metrics;
    int		i, j, k;
    
    // Generate unique list of contexts for updated metrics
    for (i = 0; i < list.size(); i++) {
	int index = list[i];

	if (index < 0 || index >= _metrics.size()) {
	    sts = PM_ERR_PMID;
	    break;
	}

	int ctx = _metrics[index]->contextIndex();

	for (j = 0; j < contexts.size(); j++)
	    if (contexts[j] == ctx)
		break;
	if (j == contexts.size())
	    contexts.append(ctx);
    }

    if (sts >= 0) {
	cout << keywords[keyUpdate] << sep << contexts.size() << Qt::endl;

	for (i = 0; i < contexts.size(); i++) {
	    uint32_t cntx = contexts[i];
	    QmcContext *context = _group->context(cntx);

	    // Generate unique list of updated metrics for this context
	    metrics.clear();
	    for (j = 0; j < list.size(); j++) {
		uint32_t index = list[j];
		if (_metrics[index]->status() >= 0 &&
		    _metrics[index]->contextIndex() == cntx)
		    metrics.append(index);
	    }

	    // Generate unique list of updated indoms for this context
	    indoms.clear();

	    for (j = 0; j < metrics.size(); j++) {

		int indom = _metrics[metrics[j]]->indomIndex();
		for (k = 0; k < indoms.size(); k++)
		    if (indoms[k] == indom)
			break;
		if (k == indoms.size())
		    indoms.append(indom);

		_metrics[metrics[j]]->updateIndom();
	    }

	    cout << keywords[keyContext] << sep << cntx << sep
		<< indoms.size() << sep << list.size() << Qt::endl;

	    for (j = 0; j < indoms.size(); j++) {
		QmcIndom const& indom = context->indom(indoms[j]);

		cout << keywords[keyIndom] << sep << indoms[j] << sep 
		    << indom.refCount() << Qt::endl;
		for (k = 0; k < indom.listLen(); k++) {
		    if (!indom.nullInst(k) && indom.refInst(k))
			cout << cont << sep << k << sep << indom.name(k) << Qt::endl;
		}
	    }
	}

	for (i = 0; i < list.size(); i++) {
	    QmcMetric const& metric = *_metrics[list[i]];

	    cout << keywords[keyMetric] << sep << list[i] << sep;

	    if (metric.status() < 0) {
		cout << metric.status() << Qt::endl;
	    }
	    else {
		cout << metric.numValues() << Qt::endl;
		for (j = 0; j < metric.numInst(); j++)
		    cout << cont << sep << metric.instIndex(j) << Qt::endl;
	    }
	}
    }
    else
	cout << keywords[keyError] << sep << sts << Qt::endl;

    cout << terminator;

    return sts;
}

void
store(char const* name, char const* inst)
{
    char buf[128];

    sprintf(buf, "pmstore %s %s > /dev/null\n", name, inst);
    cout << name << ' ' << inst << Qt::endl;
    if (system(buf) < 0) {
	pmprintf("%s: system(%s) failed\n", pmGetProgname(), buf);
	pmflush();
	exit(1);
    }
    if (system("pminfo -f dynamic") < 0) {
	pmprintf("%s: system(pminfo) failed\n", pmGetProgname());
	pmflush();
	exit(1);
    }
}

static int msgCount = 1;

void
msg(int line, char const* str)
{
    cout << Qt::endl << "*** " << msgCount << ": Line " << line << " - " << str 
	 << " ***" << Qt::endl;
    cerr << Qt::endl << "*** " << msgCount << ": Line " << line << " - " << str 
	 << " ***" << Qt::endl;
    msgCount++;
}

void
fail(int line, int err)
{
    cout << Qt::endl << "*** " << msgCount << ": Testing failed at line " << line
	 << " - " << pmErrStr(err) << " ***" << Qt::endl;
    cerr << Qt::endl << "*** " << msgCount << ": Testing failed at line " << line
	 << " - " << pmErrStr(err) << " ***" << Qt::endl;

    pmflush();

    exit(1);
    /*NOTREACHED*/
}

int
main(int argc, char* argv[])
{
    int		sts = 0;
    int		c;
    const char	*string;
    QString	archive1 = "archives/snort-disks";
    			// snort-disks timestamps
			// 1117075022.613953 ... 1117075050.309912
    QString	archive2 = "archives/vldb-disks";
    			// vldb-disks timestamps
			// 869629190.357184 ... 869629210.660548
    QString	archive3 = "archives/moomba.pmkstat";
    QStringList	metrics;
    QList<int>	metricIds;

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

    if (sts || optind != argc) {
	pmprintf("Usage: %s\n", pmGetProgname());
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
    metrics.clear();
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
    string = archive1.toLatin1().constData();
    sts = client3->context(PM_CONTEXT_ARCHIVE, string);

    checksts();

    mesg("Client3: CONTEXT ARCH archive2");
    string = archive2.toLatin1().constData();
    sts = client3->context(PM_CONTEXT_ARCHIVE, string);

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
    metrics.clear();
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

    mesg("Client3: LIST 4 snort:disk.dev.read[dks0d1,dks1d1,dks9d1] archives/vldb-disks/disk.dev.total[dks17d8,dks11d3,dks45d2] disk.dev.write[dks1d1,dks0d4] vldb.engr:disk.dev.total[dks18d6,dks11d3]");

    metrics.clear();
    metrics.append("snort:disk.dev.read[dks0d1,dks1d1,dks9d1]");
    metrics.append("archives/vldb-disks/disk.dev.total[dks17d8,dks11d3,dks45d2]");
    metrics.append("disk.dev.write[dks1d1,dks0d4]");
    metrics.append("vldb.engr:disk.dev.total[dks18d6,dks11d3]");

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
    metrics.clear();
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
    metricIds.clear();
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
    string = archive3.toLatin1().constData();
    sts = client1->context(PM_CONTEXT_ARCHIVE, string);

    checksts();

    mesg("Client1: LIST 2 kernel.all.idle localhost:kernel.all.cpu.user");
    metrics.clear();
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
