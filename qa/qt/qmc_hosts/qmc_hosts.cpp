//
// Test host matching algorithm with multiple groups
//

#include <errno.h>
#include <QTextStream>
#include <qmc_context.h>
#include <qmc_group.h>
#include <qmc_metric.h>
#include <qmc_indom.h>

QTextStream cerr(stderr);
QTextStream cout(stdout);

#define mesg(str)	msg(__LINE__, str)

void
msg(int line, char const* str)
{
    static int count = 1;

    cerr << Qt::endl << "*** " << count << ": Line " << line << " - " << str 
	 << " ***" << Qt::endl;
    count++;
}

void
quit(int err)
{
    pmflush();
    cerr << "Error: " << pmErrStr(err) << Qt::endl;
    exit(1);
}

int
main(int argc, char* argv[])
{
    int		sts = 0;
    int		c;
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

    mesg("Create two groups");
    QmcGroup group1;
    pmflush();
    QmcGroup group2;
    pmflush();
    
    mesg("Create an archive context in both groups, but to different hosts");
    source = "archives/oview-short";
    sts = group1.use(PM_CONTEXT_ARCHIVE, source);

    if (sts < 0)
	quit(sts);
	
    source = "archives/rattle";
    sts = group2.use(PM_CONTEXT_ARCHIVE, source);
    
    if (sts < 0)
	quit(sts);

    mesg("Try to create a host context to snort in group1");

    // Should pass as it will get mapped to the archive
    source = "snort";
    sts = group1.use(PM_CONTEXT_HOST, source);

    if (sts < 0)
	quit(sts);

    mesg("Try to create a host context to snort in group2");

    // Should fail, this group has not seen an archive context for host snort
    source = "snort";
    sts = group2.use(PM_CONTEXT_HOST, source);

    if (sts >= 0) {
	mesg("Test failed: duplicate context created from another group");
	pmflush();
	exit(1);
    }

    pmflush();
    return 0;
}

