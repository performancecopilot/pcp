#include <assert.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "./domain.h"

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
          assert(0);
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
    __pmSetProgname(argv[0]);
    pmdaInterface desc;
    pmdaDaemon(&desc, PMDA_INTERFACE_4, pmProgname, TRIVIAL, NULL, NULL);
    desc.version.four.children = trivial_children;
    pmdaSetFetchCallBack(&desc, trivial_fetch);
    pmdaInit(&desc, NULL, 0, NULL, 0);
    pmdaConnect(&desc);
    pmdaMain(&desc);
    exit(0);
}
