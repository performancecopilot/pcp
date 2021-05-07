/*
 * Copyright (C) 2021 Marko Myllynen <myllynen@redhat.com>
 * Copyright (c) 2021 Ken McDonell.  All Rights Reserved.
 *
 * Marko's pmFetchGroup instances demonstrator.
 * From https://github.com/performancecopilot/pcp/issues/1253
 */
#include <pcp/pmapi.h>


#define DRV "test"

void fail_if_fail(int sts, char *err) {
  if (sts < 0) {
    fprintf(stderr, "%s\n", err != NULL ? err : pmErrStr(sts));
    exit(1);
  }
}

int main(int argc, char **argv) {
  int i;
  int sts;
  char *err;
  pmAtomValue res[8];
  pmFG pmfg;

  sts = pmCreateFetchGroup(&pmfg, PM_CONTEXT_HOST, "local:");
  fail_if_fail(sts, NULL);
  sts = pmRegisterDerivedMetric(DRV, "kernel.all.uptime-proc.psinfo.start_time", &err);
  fail_if_fail(sts, err);
  for (i = 1; i < argc; i++) {
    sts = pmExtendFetchGroup_item(pmfg, DRV, argv[i], NULL, &res[i-1], PM_TYPE_DOUBLE, NULL);
    fail_if_fail(sts, NULL);
  }
  sts = pmFetchGroup(pmfg);
  fail_if_fail(sts, NULL);
  for (i = 1; i < argc; i++) {
    printf("%s:\t%f\n", argv[i], res[i-1].d);
  }
  sts = pmDestroyFetchGroup(pmfg);
  fail_if_fail(sts, NULL);
  return 0;
}
