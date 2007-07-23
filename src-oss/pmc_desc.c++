//
// Test PMC_Desc
//

#include <iostream.h>
#include "Source.h"
#include "Desc.h"

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
    
    fprintf(stderr,"*** Compare metric descriptor with pminfo output ***\n");
    PMC_Source *src = PMC_Source::getSource(PM_CONTEXT_HOST, buf);
    
    if (src->status() < 0) {
	pmprintf("%s: Error: Unable to create context to \"%s\": \n",
		 pmProgname, buf, pmErrStr(src->status()));
	pmflush();
	return 1;
    }

    __pmID_int hinv_ncpu_s;

#ifdef IRIX6_5
    /* IRIX hinv.ncpu PMID: 1.18.2 */
    hinv_ncpu_s.pad = 0;
    hinv_ncpu_s.domain = 1;
    hinv_ncpu_s.cluster = 18;
    hinv_ncpu_s.item = 2;
#else
    /* Linux hinv.ncpu PMID: 60.0.32 */
    hinv_ncpu_s.pad = 0;
    hinv_ncpu_s.domain = 60;
    hinv_ncpu_s.cluster = 0;
    hinv_ncpu_s.item = 32;
#endif

    pmID hinv_ncpu = *((pmID*)&hinv_ncpu_s);
    PMC_Desc hinv_ncpu_pmc(hinv_ncpu);
    pmDesc hinv_ncpu_desc = hinv_ncpu_pmc.desc();

    if (hinv_ncpu_pmc.status() < 0) {
	pmprintf("\n%s: Error: hinv.ncpu: %s\n",
		 pmProgname, pmErrStr(hinv_ncpu_pmc.status()));
	pmflush();
	sts = 1;
    }

    printf("hinv.ncpu\n");
    __pmPrintDesc(stdout, &hinv_ncpu_desc);
    fflush(stdout);
    fflush(stderr);
    system("pminfo -d hinv.ncpu");
    fflush(stdout);
    fflush(stderr);

    fprintf(stderr, "\n*** Fetch a bad descriptor ***\n");
    __pmID_int bad_s = { 0, 42, 42, 42 };
    pmID bad = *((pmID*)&bad_s);
    PMC_Desc bad_pmc(bad);
    
    if (bad_pmc.status() < 0) {
	pmprintf("%s: Error: Bogus metric: %s\n",
		 pmProgname, pmErrStr(bad_pmc.status()));
	pmflush();
    }
    else
	sts = 1;

    return sts;
}

