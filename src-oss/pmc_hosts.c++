//
// Test host matching algorithm with multiple groups
//

#include <iostream.h>
#include <errno.h>
#include "Group.h"
#include "Metric.h"
#include "Indom.h"

#define mesg(str)	msg(__LINE__, str)

void
msg(int line, char const* str)
{
    static int count = 1;

    cerr << endl << "*** " << count << ": Line " << line << " - " << str 
	 << " ***" << endl;
    count++;
}

void
quit(int err)
{
    pmflush();
    cerr << "Error: " << pmErrStr(err) << endl;
    exit(1);
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

    mesg("Create two groups");
    PMC_Group group1;
    pmflush();
    PMC_Group group2;
    pmflush();
    
    mesg("Create an archive context in both groups, but to different hosts");
    sts = group1.use(PM_CONTEXT_ARCHIVE, "oview-short");

    if (sts < 0)
	quit(sts);
	
    sts = group2.use(PM_CONTEXT_ARCHIVE, "../src-oss/rattle");
    
    if (sts < 0)
	quit(sts);

    mesg("Try to create a host context to snort in group1");

    // Should pass as it will get mapped to the archive
    sts = group1.use(PM_CONTEXT_HOST, "snort");

    if (sts < 0)
	quit(sts);

    mesg("Try to create a host context to snort in group2");

    // Should fail, this group has not seen an archive context for host snort
    sts = group2.use(PM_CONTEXT_HOST, "snort");

    if (sts >= 0) {
	mesg("Test failed: duplicate context created from another group");
	pmflush();
	exit(1);
    }

    pmflush();
    return 0;
}

