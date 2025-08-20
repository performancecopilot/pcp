/*
 * Scan an archive index file from disk, no libpcp layers in the
 * way.
 *
 * Copyright (c) 2018,2025 Ken McDonell.  All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

/* from internal.h ... */
#ifdef HAVE_NETWORK_BYTEORDER
#define __ntohll(a)             /* noop */
#else
#define __ntohll(v) __htonll(v)
#endif

/* from e_index.c ... */
/*
 * On-Disk Temporal Index Record, Version 3
 */
typedef struct {
    __int32_t   sec[2];         /* __pmTimestamp */
    __int32_t   nsec;
    __int32_t   vol;
    __int32_t   off_meta[2];
    __int32_t   off_data[2];
} __pmTI_v3;

/*
 * On-Disk Temporal Index Record, Version 2
 */
typedef struct {
    __int32_t   sec;            /* pmTimeval */
    __int32_t   usec;
    __int32_t   vol;
    __int32_t   off_meta;
    __int32_t   off_data;
} __pmTI_v2;

/* from endian.c */
#ifdef __htonll
#undef __htonll
#endif
void
__htonll(char *p)
{
    char        c;
    int         i;

    for (i = 0; i < 4; i++) {
        c = p[i];
        p[i] = p[7-i];
        p[7-i] = c;
    }
}

void
usage(void)
{
    fprintf(stderr, "Usage: %s [options] in.index\n", pmGetProgname());
    fprintf(stderr, "Options:\n");
    fprintf(stderr, " -D debug         set debug options\n");
}

int
main(int argc, char *argv[])
{
    size_t      buflen;
    size_t      bytes;
    void        *buf ;
    int         in;
    int         c;
    int         sts;
    int         errflag = 0;
    int         nrec;
    __pmFILE    *f;
    __pmLogLabel        label;
    __pmLogTI   ti;

    pmSetProgname(argv[0]);
    setlinebuf(stdout);
    setlinebuf(stderr);

    while ((c = getopt(argc, argv, "D:lx")) != EOF) {
        switch (c) {

        case 'D':       /* debug options */
            sts = pmSetDebug(optarg);
            if (sts < 0) {
                fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
                    pmGetProgname(), optarg);
                errflag++;
            }
            break;

        case '?':
        default:
            errflag++;
            break;
        }
    }

    if (errflag || optind != argc-1) {
        usage();
        exit(1);
    }

    if ((in = open(argv[optind], O_RDONLY)) < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", argv[optind], strerror(errno));
        exit(1);
    }

    if ((f = __pmFopen(argv[optind], "r")) == NULL) {
        fprintf(stderr, "Failed to __pmFopen %s: %s\n", argv[optind], strerror(errno));
        exit(1);
    }
    memset((void *)&label, 0, sizeof(label));
    if ((sts = __pmLogLoadLabel(f, &label)) < 0) {
        fprintf(stderr, "error: %s does not start with label record, not a PCP archive file?\n", argv[optind]);
        exit(1);
    }
    printf("[0] archive label <V%d> @ ", label.magic & 0xff);
    __pmPrintTimestamp(stdout, &label.start);
    putchar('\n');

    switch (label.magic & 0xff) {
        case PM_LOG_VERS03:
            buflen = sizeof(__pmTI_v3);
            break;
        case PM_LOG_VERS02:
            buflen = sizeof(__pmTI_v2);
            break;
        default:
            fprintf(stderr, "botch: version %d not 2 or 3\n", label.magic & 0xff);
            exit(1);
    }
    buf = (void *)malloc(buflen);
    if (buf == NULL) {
        fprintf(stderr, "Arrgh: buf malloc %zd failed\n", buflen);
        exit(1);
    }

    for (nrec = 1; ; nrec++) {
        bytes = __pmFread(buf, 1, buflen, f);
        if (bytes != buflen) {
            if (__pmFeof(f))
                break;
            fprintf(stderr, "error: ti[%d] read %zd expecting %zd\n", nrec, bytes, buflen);
            exit(1);
        }

        /* from __pmLogLoadIndex() ... */
        if ((label.magic & 0xff) == PM_LOG_VERS03) {
            __pmTI_v3   *tip_v3 = (__pmTI_v3 *)buf;
            __pmLoadTimestamp(&tip_v3->sec[0], &ti.stamp);
            ti.vol = ntohl(tip_v3->vol);
            __ntohll((char *)&tip_v3->off_meta[0]);
            __ntohll((char *)&tip_v3->off_data[0]);
            memcpy((void *)&ti.off_meta, (void *)&tip_v3->off_meta[0], 2*sizeof(__int32_t));
            memcpy((void *)&ti.off_data, (void *)&tip_v3->off_data[0], 2*sizeof(__int32_t));
        }
        else {
            __pmTI_v2   *tip_v2 = (__pmTI_v2 *)buf;
            __pmLoadTimeval(&tip_v2->sec, &ti.stamp);
            ti.vol = ntohl(tip_v2->vol);
            ti.off_meta = ntohl(tip_v2->off_meta);
            ti.off_data = ntohl(tip_v2->off_data);
        }
        printf("[%d] ", nrec);
        if ((label.magic & 0xff) == PM_LOG_VERS03)
            __pmPrintTimestamp(stdout, &ti.stamp);
        else {
            struct tm   tmp;
            time_t      now;
            now = (time_t)ti.stamp.sec;
            pmLocaltime(&now, &tmp);
// check-time-formatting-ok
        printf("%02d:%02d:%02d.%06d", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, ti.stamp.nsec / 1000);

        }
        printf(" vol %d meta %lld data %lld\n", ti.vol,
                (long long)ti.off_meta, (long long)ti.off_data);

    }

    __pmFclose(f);
    __pmLogFreeLabel(&label);

    return 0;
}
