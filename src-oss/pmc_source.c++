//
// Test PMC_Source
//

#include "Source.h"
#ifdef HAVE_IOSTREAM
#include <iostream>
using namespace std;
#else
#include <iostream.h>
#endif
#include <strings.h>

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

    fprintf(stderr,"*** Create an archive context ***\n");
    PMC_Source* src1 = PMC_Source::getSource(PM_CONTEXT_ARCHIVE, 
			"oview-short",
			PMC_false);

    if (src1->status() < 0) {
	pmprintf("%s: Error: Unable to create context to \"oview-short\": %s\n",
		 pmProgname, pmErrStr(src1->status()));
	pmflush();
	sts = 1;
    }

    fprintf(stderr,"\n*** Create an archive context using the host name ***\n");
    PMC_Source* src2 = PMC_Source::getSource(PM_CONTEXT_HOST,
			"snort", PMC_true);

    if (src2->status() < 0) {
	pmprintf("%s: Error: Unable to create context to \"%s\": %s\n",
		 pmProgname, buf, pmErrStr(src2->status()));
	pmflush();
	sts = 1;
    }

    if (src1 != src2) {
	pmprintf("%s: Error: Matching host to an archive failed: src1 = %s, src2 = %s\n",
		 pmProgname, src1->desc().ptr(), src2->desc().ptr());
	pmflush();
	PMC_Source::dumpList(cerr);
	sts = 1;
    }

    fprintf(stderr,"\n*** Create two live contexts to the local host ***\n");
    PMC_Source* src3 = PMC_Source::getSource(PM_CONTEXT_HOST, buf);
    PMC_Source* src4 = PMC_Source::getSource(PM_CONTEXT_HOST, buf);

    if (src3->status() < 0) {
	pmprintf("%s: Error: Unable to create context to \"%s\": %s\n",
		 pmProgname, buf, pmErrStr(src3->status()));
	pmflush();
    }

    if (src4->status() < 0) {
	pmprintf("%s: Error: Unable to create context to \"%s\": %s\n",
		 pmProgname, buf, pmErrStr(src4->status()));
	pmflush();
    }

    if (src3 != src4) {
	pmprintf("%s: Error: Identical host test failed: src3 = %s, src4 = %s\n",
		 pmProgname, src3->desc().ptr(), src4->desc().ptr());
	pmflush();
	PMC_Source::dumpList(cerr);
	sts = 1;
    }

    fprintf(stderr,"\n*** Create a local context ***\n");
    PMC_Source* src5 = PMC_Source::getSource(PM_CONTEXT_LOCAL, NULL);
    
    if (src5->status() < 0) {
	pmprintf("%s: Error: Unable to create context to localhost: %s\n",
		 pmProgname, pmErrStr(src5->status()));
	pmflush();
	sts = 1;
    }

    fprintf(stderr,"\n*** List all known sources ***\n");
    PMC_Source::dumpList(cout);

    pmflush();
    return sts;
}
