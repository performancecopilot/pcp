#include <assert.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include "./domain.h"


static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_LOGFILE,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};
static pmdaOptions opts = {
    .short_options = "D:l:?",
    .long_options = longopts,
};

/*
 * callback for pmdaChildren()
 */
static int
trivial_children(char const * name, int traverse, char *** offspring, int ** status, pmdaExt * ext)
{
     /*
      * for the purpose of this test, we keep things very simple
      */

     if (strcmp(name, "trivial"))
          return PM_ERR_NAME;

     char const * metric;
     switch (traverse) {
     case 0:
          metric = "foo";
          break;
     case 1:
          metric = "trivial.foo";
          break;
     default:
	  fprintf(stderr, "botch: traverse=%d\n", traverse);
          exit(1);
     }

     /* offspring */
     char ** namebuf = malloc(sizeof(char *) + strlen(metric) + 1);
     assert(namebuf);
     namebuf[0] = (char *) (namebuf + 1);
     strcpy(namebuf[0], metric);
     *offspring = namebuf;

     /* status */
     int * statusbuf = malloc(sizeof(int));
     assert(statusbuf);
     statusbuf[0] = 0;  // a leaf
     *status = statusbuf;

     return 1; // one entry
}

static int
trivial_fetch(pmdaMetric * mdesc, unsigned int inst, pmAtomValue * atom)
{
     return PM_ERR_PMID;
}

int
main(int argc, char **argv)
{
    pmdaInterface desc;

    pmSetProgname(argv[0]);
    pmdaDaemon(&desc, PMDA_INTERFACE_4, pmGetProgname(), TRIVIAL, NULL, NULL);

    pmdaGetOptions(argc, argv, &opts, &desc);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    desc.version.four.children = trivial_children;
    pmdaSetFetchCallBack(&desc, trivial_fetch);
    pmdaInit(&desc, NULL, 0, NULL, 0);
    pmdaConnect(&desc);
    pmdaMain(&desc);
    exit(0);
}
