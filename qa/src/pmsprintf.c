/*
 * Copyright (c) 2017 Red Hat.
 */

#include <pcp/pmapi.h>

int
main(int argc, char *argv[])
{
    char	str[16];
    int		sts;
    int		i = 0;

    memset(str, '~', sizeof(str));
    sts = pmsprintf(str, 16, "[%d] %s", i++, "test");
    printf("[%d] pmsprintf strlen=%d str='%s'\n", i, sts, str);

    memset(str, '~', sizeof(str));
    sts = pmsprintf(str, 3, "[%d] %s", i++, "test");
    printf("[%d] pmsprintf strlen=%d str='%s'\n", i, sts, str);

    memset(str, '~', sizeof(str));
    sts = pmsprintf(str, 0, "[%d] %s", i++, "test");
    printf("[%d] pmsprintf strlen=%d str='%.*s'\n", i, sts, 16, str);

    memset(str, '~', sizeof(str));
    sts = pmsprintf(str, 1, "[%d] %s", i++, "test");
    printf("[%d] pmsprintf strlen=%d str='%s'\n", i, sts, str);

    memset(str, '~', sizeof(str));
    sts = pmsprintf(str, 2, "[%d] %s", i++, "test");
    printf("[%d] pmsprintf strlen=%d str='%s'\n", i, sts, str);

    exit(0);
}
