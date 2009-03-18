/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * main - test the code which adds PMNS to archives
 */

#include <ctype.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

void
parse_args(int argc, char **argv)
{
    extern char	*optarg;
    extern int	optind;
    int		errflag = 0;
    int		c;
    int		sts;
    static char	*usage = "[-v]";

#ifdef PCP_DEBUG
    static char	*debug = "[-D N]";
#else
    static char	*debug = "";
#endif

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {
#ifdef PCP_DEBUG

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;
#endif

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	printf("Usage: %s %s%s\n", pmProgname, debug, usage);
	exit(1);
    }


}

typedef struct {
  char *name;
  pmID pmid;
}Metric;

static Metric metric_list[] = {
  {"abc.def.ghi", 100},
  {"abc.def.jkl", 200},
  {"abc.mno", 300},
  {"pqr.rst.uvw", 400},
  {"pqr.xyz", 500}
};
static char *parents[] = {
  "abc", "pqr", "abc.def", "def", "abc.def.ghi"
};

const int num_metrics = sizeof(metric_list) / sizeof(Metric);
const int num_parents = sizeof(parents) / sizeof(char*);

int
main(int argc, char **argv)
{
  int sts = 0;
  int i = 0;
  __pmnsTree *pmns = NULL;

  parse_args(argc, argv);


  printf("\n*** Build up the PMNS from metrics... ***\n");
  printf("Number of metrics = %d\n", num_metrics);

  if ((sts = __pmNewPMNS(&pmns)) < 0) {
      fprintf(stderr, "%s: __pmNewPMNS: %s\n", pmProgname, pmErrStr(sts));
      exit(1);
  }

  for(i = 0; i < num_metrics; i++) {
      pmID pmid = metric_list[i].pmid;
      char *name = metric_list[i].name;
      sts = __pmAddPMNSNode(pmns, pmid, name);
      printf("Adding node: \"%s\"[%d]\n", name, pmid);
      if (sts < 0) {
	  fprintf(stderr, "%s: __pmAddPMNSNode: %s\n", 
		  pmProgname, pmErrStr(sts));
	  exit(1);
      }
  } 

  if ((sts = __pmFixPMNSHashTab(pmns, num_metrics, 1)) < 0) {
      fprintf(stderr, "%s: __pmFixPMNSHashTab: %s\n", 
	      pmProgname, pmErrStr(sts));
      exit(1);
  }
  __pmUsePMNS(pmns);


  printf("\n*** Check PMNS is ok ***\n");

  printf("\n--- Dump out PMNS ---\n");
  __pmDumpNameSpace(stdout, 1);

  printf("\n--- Test out pmLookupName ---\n");
  for(i = 0; i < num_metrics; i++) {
      pmID pmid = metric_list[i].pmid;
      char *name = metric_list[i].name;
      pmID thepmid = 0;

      if ((sts = pmLookupName(1, &name, &thepmid)) < 0) {
	  fprintf(stderr, "%s: _pmLookupName: %s\n", 
		  pmProgname, pmErrStr(sts));
      }
      if (thepmid == pmid) {
	  printf("%d matches for name lookup\n", pmid);
      }
      else {
	  printf("%d does not match with %d\n", pmid, thepmid);
      }
  }

  printf("\n--- Test out pmNameID for matches ---\n");
  for(i = 0; i < num_metrics; i++) {
      pmID pmid = metric_list[i].pmid;
      char *name = metric_list[i].name;
      char *thename = NULL;

      if ((sts = pmNameID(pmid, &thename)) < 0) {
	  fprintf(stderr, "%s: _pmNameID: %s\n", 
		  pmProgname, pmErrStr(sts));
      }
      if (strcmp(name, thename) == 0) {
	  printf("%s matches for id lookup\n", name);
      }
      else {
	  printf("%s does not match with %s\n", name, thename);
      }
  }

  printf("\n--- Test out pmGetChildren ---\n");
  for (i = 0; i < num_parents; i++) {
      char *parent = parents[i]; 
      int num_childs = 0;
      int j;
      char **offspring = NULL;

      printf("Children of %s:\n", parent);
      if ((sts = pmGetChildren(parent, &offspring)) < 0) {
	  fprintf(stderr, "%s: _pmGetChildren: %s\n", 
		  pmProgname, pmErrStr(sts));
      }
      num_childs = sts;
      for (j = 0; j < num_childs; j++) {
	printf("  %s\n", offspring[j]);
      }
  }/*for*/

  exit(0);
}
