//
// Test QmcContext class
//

#include <QTextStream>
#include <qmc_context.h>

QTextStream cerr(stderr);
QTextStream cout(stdout);

int
main(int argc, char* argv[])
{
    int		fail = 0, sts = 0;
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
                fail = 1;
            }
            break;
	case '?':
	default:
	    sts = 1;
	    break;
	}
    }

    if (fail) {
	pmprintf("Usage: %s\n", pmGetProgname());
	pmflush();
	exit(1);
    }

    (void)gethostname(buf, MAXHOSTNAMELEN);
    buf[MAXHOSTNAMELEN-1] = '\0';

    fprintf(stderr, "*** Simple connection ***\n");
    source = QString("oview-short");

    QmcSource *src1 = QmcSource::getSource(PM_CONTEXT_ARCHIVE, source, false);
    if (src1->status() < 0) {
	pmprintf("%s: Error: Unable to create context to \"%s\": %s\n",
		pmGetProgname(), (const char *)source.toLatin1(),
		pmErrStr(src1->status()));
	pmflush();
	fail = 1;
    }

    QmcContext context1(src1);
    if (context1.handle() < 0) {
	pmflush();
	fail = 1;
    }

    context1.dump(cout);

    pmID pmid;
    uint32_t indomIndex;

    fprintf(stderr, "\n*** Cacheing of descriptors and indoms ***\n");
    QmcDesc *desc;
    QmcIndom *indom = NULL;

    sts = context1.lookupInDom("hinv.ncpu", indomIndex);
    if (sts < 0) {
	pmprintf("%s: Error: hinv.ncpu: %s\n",
		 pmGetProgname(), pmErrStr(sts));
	pmflush();
	fail = 1;
    }
    else {
	sts = context1.lookupPMID("hinv.ncpu", pmid);
	if (sts < 0) {
	    pmprintf("%s: Error: hinv.ncpu PMID: %s\n",
		 pmGetProgname(), pmErrStr(sts));
	    pmflush();
	    fail = 1;
	}
	desc = &context1.desc(pmid);
	if (desc->status() < 0) {
	    pmprintf("%s: Error: hinv.ncpu descriptor: %s\n",
		     pmGetProgname(), pmErrStr(desc->status()));
	    pmflush();
	    fail = 1;
	}
	else if (indomIndex < UINT_MAX) {
	    pmprintf("%s: Error: hinv.ncpu indom is not NULL\n",
		     pmGetProgname());
	    pmflush();
	    fail = 1;
	}
    }

    sts = context1.lookupInDom("hinv.cputype", indomIndex);
    if (sts < 0) {
	pmprintf("%s: Error: hinv.cputype: %s\n",
		 pmGetProgname(), pmErrStr(sts));
	pmflush();
	fail = 1;
    }
    else {
	sts = context1.lookupPMID("hinv.cputype", pmid);
	if (sts < 0) {
	    pmprintf("%s: Error: hinv.cputype PMID: %s\n",
		pmGetProgname(), pmErrStr(sts));
	    pmflush();
	    fail = 1;
	}
	desc = &context1.desc(pmid);
	indom = &context1.indom(indomIndex);
	if (desc->status() < 0) {
	    pmprintf("%s: Error: hinv.cputype descriptor: %s\n",
		     pmGetProgname(), pmErrStr(desc->status()));
	    pmflush();
	    fail = 1;
	}
	else if (indom->status() < 0) {
	    pmprintf("%s: Error: hinv.cputype indom: %s\n",
		     pmGetProgname(), pmErrStr(indom->status()));
	    pmflush();
	    fail = 1;
	}
    }

    QmcIndom *indom2;

    sts = context1.lookupInDom("hinv.map.cpu", indomIndex);
    if (sts < 0) {
	pmprintf("%s: Error: hinv.map.cpu: %s\n",
		 pmGetProgname(), pmErrStr(sts));
	pmflush();
	fail = 1;
    }
    else {
	sts = context1.lookupPMID("hinv.map.cpu", pmid);
	if (sts < 0) {
	    pmprintf("%s: Error: hinv.map.cpu PMID: %s\n",
		pmGetProgname(), pmErrStr(sts));
	    pmflush();
	    fail = 1;
	}

	desc = &context1.desc(pmid);
	indom2 = &context1.indom(indomIndex);

	if (desc->status() < 0) {
	    pmprintf("%s: Error: hinv.map.cpu descriptor: %s\n",
		     pmGetProgname(), pmErrStr(desc->status()));
	    pmflush();
	    fail = 1;
	}
	else if (indom2->status() < 0) {
	    pmprintf("%s: Error: hinv.map.cpu indom: %s\n",
		     pmGetProgname(), pmErrStr(indom2->status()));
	    pmflush();
	    fail = 1;
	}
	else if (indom != indom2) {
	    pmprintf("%s: Error: hinv.cputype and hinv.map.cpu indoms are not the same\n",
		     pmGetProgname());
	    pmflush();
	    fail = 1;
	}
    }

    sts = context1.lookupInDom("hinv.ncpu", indomIndex);
    if (sts < 0) {
	pmprintf("%s: Error: hinv.ncpu: %s\n",
		 pmGetProgname(), pmErrStr(sts));
	pmflush();
	fail = 1;
    }
    else {
	sts = context1.lookupPMID("hinv.ncpu", pmid);
	if (sts < 0) {
	    pmprintf("%s: Error: hinv.ncpu PMID: %s\n",
		pmGetProgname(), pmErrStr(sts));
	    pmflush();
	    fail = 1;
	}

	desc = &context1.desc(pmid);

	if (desc->status() < 0) {
	    pmprintf("%s: Error: hinv.ncpu descriptor: %s\n",
		     pmGetProgname(), pmErrStr(desc->status()));
	    pmflush();
	    fail = 1;
	}
	else if (indomIndex < UINT_MAX) {
	    pmprintf("%s: Error: hinv.ncpu indom is not NULL\n",
		     pmGetProgname());
	    pmflush();
	    fail = 1;
	}
    }

    context1.dump(cout);

    fprintf(stderr, "\n*** Bad Context ***\n");
    source = QString("no-such-host");
    QmcSource *src2 = QmcSource::getSource(PM_CONTEXT_HOST, source);

    if (src2->status() >= 0) {
	pmprintf("%s: Error: Able to create context to \"%s\": %s\n",
		pmGetProgname(), (const char *)source.toLatin1(),
		pmErrStr(src1->status()));
	pmflush();
	fail = 1;
    }

    QmcContext context2(src2);

    if (context2.handle() >= 0) {
	pmprintf("%s: Error: Created a valid context to an invalid host\n",
		 pmGetProgname());
	fail = 1;
    }

    pmflush();

    context2.dump(cout);

    fprintf(stderr, "\n*** Exiting ***\n");
    
    pmflush();
    return fail;
}
