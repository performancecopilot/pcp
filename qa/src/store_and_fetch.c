#include <ctype.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

/**
 * similar to pmstore -F, except that it only returns the new value
 * and does not stop on PM_ERR_BADSTORE
 */
int main(int argc, char **argv) {
    int ctx;
    char *new_value_str;
    const char *namelist[1];
    pmID pmidlist[1];
    pmResult *result;
    pmAtomValue atomValue;
    pmValue newValue;
    int ret;

    if (argc != 3) {
        printf("Usage: %s metric new_value\n", argv[0]);
        return 1;
    }

    namelist[0] = argv[1];
    new_value_str = argv[2];

    if ((ctx = pmNewContext(PM_CONTEXT_HOST, "local:")) < 0) {
        printf("Cannot connect to PMCD: %s\n", pmErrStr(ctx));
        return 1;
    }

    if ((ret = pmLookupName(1, namelist, pmidlist)) != 1) {
        printf("pmLookupName: %s\n", pmErrStr(ret));
        return 1;
    }

    if ((ret = pmFetch(1, pmidlist, &result)) < 0) {
        printf("pmFetch before store: %s\n", pmErrStr(ret));
        return 1;
    }

    if ((ret = __pmStringValue(new_value_str, &atomValue, PM_TYPE_STRING)) < 0) {
        printf("__pmStringValue: %s\n", pmErrStr(ret));
        return 1;
    }

    if ((ret = __pmStuffValue(&atomValue, &newValue, PM_TYPE_STRING)) < 0) {
        printf("__pmStuffValue: %s\n", pmErrStr(ret));
        return 1;
    }

    result->vset[0]->vlist[0] = newValue;

    if ((ret = pmStore(result)) != 0 && ret != PM_ERR_BADSTORE) {
        printf("pmStore: %s\n", pmErrStr(ret));
        return 1;
    }

    if ((ret = pmFetch(1, pmidlist, &result)) < 0) {
        printf("pmFetch after store: %s\n", pmErrStr(ret));
        return 1;
    }

    puts(result->vset[0]->vlist[0].value.pval->vbuf);
    return 0;
}
