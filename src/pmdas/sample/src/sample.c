/*
 * Copyright (c) 2014-2015 Red Hat.
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include <limits.h>
#include <sys/stat.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include "percontext.h"
#include "events.h"
#include "domain.h"
#ifdef HAVE_SYSINFO
/*
 * On Solaris, need <sys/systeminfo.h> and sysinfo() is different.
 * Other platforms need <sys/sysinfo.h>
 */
#ifdef IS_SOLARIS
#include <sys/systeminfo.h>
#define MAX_SYSNAME	257
#else
#include <sys/sysinfo.h>
#endif
#else
static struct sysinfo {
    char	dummy[64];
} si = { {
'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 
'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 
'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 
'W', 'X', 'Y', 'Z', '0', '1', '2', '3', '4', '5', '6', '7', '9', '[', ']', '.' 
} };
#endif
#ifndef roundup
#define roundup(x,y) ((((x) + ((y) - 1)) / (y)) * (y))
#endif

static int need_mirage;	/* only do mirage glop is someone asks for it */
static int need_dynamic;/* only do dynamic glop is someone asks for it */

/* from pmda.c: simulate PMDA busy */
extern int	limbo(void);

/*
 * all metrics supported in this PMD - one table entry for each
 */

static pmDesc	desctab[] = {
/* control */
    { PMDA_PMID(0,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* daemon_pid or sample.dupnames.daemon_pid or sample.dupnames.pid_daemon */
    { PMDA_PMID(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* seconds */
    { PMDA_PMID(0,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) },
/* milliseconds */
    { PMDA_PMID(0,3), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) },
/* load */
    { PMDA_PMID(0,4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* colour */
    { PMDA_PMID(0,5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* bin or dupnames.two.bin or dupnames.three.bin */
    { PMDA_PMID(0,6), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* drift */
    { PMDA_PMID(0,7), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* step */
    { PMDA_PMID(0,8), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* noinst */
    { PMDA_PMID(0,9), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* long.one */
    { PMDA_PMID(0,10), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* long.ten */
    { PMDA_PMID(0,11), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* long.hundred */
    { PMDA_PMID(0,12), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* long.million */
    { PMDA_PMID(0,13), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* long.write_me */
    { PMDA_PMID(0,14), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* float.one */
    { PMDA_PMID(0,15), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* float.ten or dupnames.two.float.ten */
    { PMDA_PMID(0,16), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* float.hundred */
    { PMDA_PMID(0,17), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* float.million */
    { PMDA_PMID(0,18), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* float.write_me */
    { PMDA_PMID(0,19), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* longlong.one */
    { PMDA_PMID(0,20), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* longlong.ten */
    { PMDA_PMID(0,21), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* longlong.hundred */
    { PMDA_PMID(0,22), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* longlong.million */
    { PMDA_PMID(0,23), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* longlong.write_me */
    { PMDA_PMID(0,24), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* double.one */
    { PMDA_PMID(0,25), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* double.ten or dupnames.two.double.ten */
    { PMDA_PMID(0,26), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* double.hundred */
    { PMDA_PMID(0,27), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* double.million */
    { PMDA_PMID(0,28), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* double.write_me */
    { PMDA_PMID(0,29), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* string.null */
    { PMDA_PMID(0,30), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* string.hullo */
    { PMDA_PMID(0,31), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* string.write_me */
    { PMDA_PMID(0,32), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* aggregate.null */
    { PMDA_PMID(0,33), PM_TYPE_AGGREGATE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* aggregate.hullo */
    { PMDA_PMID(0,34), PM_TYPE_AGGREGATE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* aggregate.write_me */
    { PMDA_PMID(0,35), PM_TYPE_AGGREGATE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* write_me or dupnames.two.write_me or dupnames.three.write_me */
    { PMDA_PMID(0,36), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,-1,1,0,PM_TIME_SEC,PM_COUNT_ONE) },
/* mirage */
    { PMDA_PMID(0,37), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(1,-1,0,PM_SPACE_KBYTE,PM_TIME_SEC,0) },
/* mirage-longlong */
    { PMDA_PMID(0,38), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(1,-1,0,PM_SPACE_BYTE,PM_TIME_MSEC,0) },
/* sysinfo */
    { PMDA_PMID(0,39), PM_TYPE_AGGREGATE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* pdu */
    { PMDA_PMID(0,40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* recv-pdu */
    { PMDA_PMID(0,41), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* xmit-pdu */
    { PMDA_PMID(0,42), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* percontext.pdu */
    { PMDA_PMID(0,43), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* percontext.recv-pdu */
    { PMDA_PMID(0,44), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* percontext.xmit-pdu */
    { PMDA_PMID(0,45), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* lights or dupnames.two.lights */
    { PMDA_PMID(0,46), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* magnitude */
    { PMDA_PMID(0,47), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* bucket - alias for bin, but different PMID */
    { PMDA_PMID(0,48), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* needprofile - need explicit instance profile */
    { PMDA_PMID(0,49), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* part_bin - bin, minus an instance or two */
    { PMDA_PMID(0,50), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* bogus_bin - bin, plus an instance or two */
    { PMDA_PMID(0,51), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* hordes.one */
    { PMDA_PMID(0,52), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* hordes.two */
    { PMDA_PMID(0,53), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* bad.unknown */
    { PMDA_PMID(0,54), 0, 0, 0, PMDA_PMUNITS(0,0,0,0,0,0) },
/* bad.nosupport */
    { PMDA_PMID(0,55), PM_TYPE_NOSUPPORT, PM_INDOM_NULL, 0, PMDA_PMUNITS(0,0,0,0,0,0) },
/* not_ready */
    { PMDA_PMID(0,56), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* wrap.long */
    { PMDA_PMID(0,57), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,0,0,0,0) },
/* wrap.ulong */
    { PMDA_PMID(0,58), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,0,0,0,0) },
/* wrap.longlong */
    { PMDA_PMID(0,59), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,0,0,0,0) },
/* wrap.ulonglong */
    { PMDA_PMID(0,60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,0,0,0,0) },
/* dodgey.control */
    { PMDA_PMID(0,61), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) },
/* dodgey.value */
    { PMDA_PMID(0,62), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* step_counter */
    { PMDA_PMID(0,63), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* rapid */
    { PMDA_PMID(0,64), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* scale_step.bytes_up */
    { PMDA_PMID(0,65), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(1,-1,0,PM_SPACE_BYTE,PM_TIME_SEC,0) },
/* scale_step.bytes_down */
    { PMDA_PMID(0,66), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) },
/* scale_step.count_up */
    { PMDA_PMID(0,67), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,-1,1,0,PM_TIME_SEC,PM_COUNT_ONE) },
/* scale_step.count_down */
    { PMDA_PMID(0,68), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* scale_step.time_up_secs */
    { PMDA_PMID(0,69), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) },
/* scale_step.time_up_nanosecs */
    { PMDA_PMID(0,70), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) },
/* scale_step.none_up */
    { PMDA_PMID(0,71), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* const_rate.value */
    { PMDA_PMID(0,72), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* const_rate.gradient */
    { PMDA_PMID(0,73), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* error_code */
    { PMDA_PMID(0,74), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* error_check */
    { PMDA_PMID(0,75), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* dynamic.counter */
    { PMDA_PMID(0,76), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* dynamic.discrete */
    { PMDA_PMID(0,77), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* dynamic.instant */
    { PMDA_PMID(0,78), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* many.count */
    { PMDA_PMID(0,79), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,0) },
/* many.int */
    { PMDA_PMID(0,80), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,0) },
/* byte_ctr */
    { PMDA_PMID(0,81), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) },
/* byte_rate */
    { PMDA_PMID(0,82), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(1,-1,0,PM_SPACE_BYTE,PM_TIME_SEC,0) },
/* kbyte_ctr */
    { PMDA_PMID(0,83), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) },
/* kbyte_rate */
    { PMDA_PMID(0,84), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(1,-1,0,PM_SPACE_KBYTE,PM_TIME_SEC,0) },
/* byte_rate_per_hour */
    { PMDA_PMID(0,85), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(1,-1,0,PM_SPACE_BYTE,PM_TIME_HOUR,0) },
/* dynamic.meta.metric - pmDesc here is a fake, use magic */
    { PMDA_PMID(0,86), 0, 0, 0, PMDA_PMUNITS(0,0,0,0,0,0) },
/* dynamic.meta.pmdesc.type */
    { PMDA_PMID(0,87), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* dynamic.meta.pmdesc.indom */
    { PMDA_PMID(0,88), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* dynamic.meta.pmdesc.sem */
    { PMDA_PMID(0,89), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* dynamic.meta.pmdesc.units */
    { PMDA_PMID(0,90), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* datasize */
    { PMDA_PMID(0,91), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) },
/* darkness */
    { PMDA_PMID(0,92), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* ulong.one */
    { PMDA_PMID(0,93), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* ulong.ten */
    { PMDA_PMID(0,94), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* ulong.hundred */
    { PMDA_PMID(0,95), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* ulong.million */
    { PMDA_PMID(0,96), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* ulong.write_me */
    { PMDA_PMID(0,97), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* ulonglong.one */
    { PMDA_PMID(0,98), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* ulonglong.ten */
    { PMDA_PMID(0,99), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* ulonglong.hundred */
    { PMDA_PMID(0,100), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* ulonglong.million */
    { PMDA_PMID(0,101), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* ulonglong.write_me */
    { PMDA_PMID(0,102), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* long.bin */
    { PMDA_PMID(0,103), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* long.bin_ctr */
    { PMDA_PMID(0,104), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) },
/* ulong.bin */
    { PMDA_PMID(0,105), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* ulong.bin_ctr */
    { PMDA_PMID(0,106), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) },
/* float.bin */
    { PMDA_PMID(0,107), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* float.bin_ctr */
    { PMDA_PMID(0,108), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) },
/* longlong.bin */
    { PMDA_PMID(0,109), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* longlong.bin_ctr */
    { PMDA_PMID(0,110), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) },
/* ulonglong.bin */
    { PMDA_PMID(0,111), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* ulonglong.bin_ctr */
    { PMDA_PMID(0,112), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) },
/* double.bin */
    { PMDA_PMID(0,113), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* double.bin_ctr */
    { PMDA_PMID(0,114), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) },
/* sample.ulong.count.base */
    { PMDA_PMID(0,115), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(-1,0,1,PM_SPACE_MBYTE,0,PM_COUNT_ONE) },
/* sample.ulong.count.deca */
    { PMDA_PMID(0,116), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(-1,0,1,PM_SPACE_MBYTE,0,PM_COUNT_ONE+1) },
/* sample.ulong.count.hecto */
    { PMDA_PMID(0,117), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(-1,0,1,PM_SPACE_MBYTE,0,PM_COUNT_ONE+2) },
/* sample.ulong.count.kilo */
    { PMDA_PMID(0,118), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(-1,0,1,PM_SPACE_MBYTE,0,PM_COUNT_ONE+3) },
/* sample.ulong.count.mega */
    { PMDA_PMID(0,119), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(-1,0,1,PM_SPACE_MBYTE,0,PM_COUNT_ONE+6) },
/* scramble.version */
    { PMDA_PMID(0,120), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* scramble.bin */
    { PMDA_PMID(0,121), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* percontext.control.ctx */
    { PMDA_PMID(0,122), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* percontext.control.active */
    { PMDA_PMID(0,123), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* percontext.control.start */
    { PMDA_PMID(0,124), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* percontext.control.end */
    { PMDA_PMID(0,125), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/* event.reset */
    { PMDA_PMID(0,126), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.type */
    { PMDA_PMID(0,127), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.param_32 */
    { PMDA_PMID(0,128), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.param_u32 */
    { PMDA_PMID(0,129), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.param_64 */
    { PMDA_PMID(0,130), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.param_u64 */
    { PMDA_PMID(0,131), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.param_float */
    { PMDA_PMID(0,132), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.param_double */
    { PMDA_PMID(0,133), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.param_string */
    { PMDA_PMID(0,134), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.param_aggregate */
    { PMDA_PMID(0,135), PM_TYPE_AGGREGATE, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.records */
    { PMDA_PMID(0,136), PM_TYPE_EVENT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.no_indom_records */
    { PMDA_PMID(0,137), PM_TYPE_EVENT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* bad.novalues */
    { PMDA_PMID(0,138), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.highres_records */
    { PMDA_PMID(0,139), PM_TYPE_HIGHRES_EVENT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/* event.reset */
    { PMDA_PMID(0,140), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },

/*
 * dynamic PMNS ones
 * secret.bar
 */
    { PMDA_PMID(0,1000), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) },
/*  secret.foo.one */
    { PMDA_PMID(0,1001), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/*  secret.foo.two */
    { PMDA_PMID(0,1002), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/*  secret.foo.bar.three */
    { PMDA_PMID(0,1003), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/*  secret.foo.bar.four */
    { PMDA_PMID(0,1004), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/*  secret.foo.bar.grunt.five */
    { PMDA_PMID(0,1005), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/*  secret.foo.bar.grunt.snort.six */
    { PMDA_PMID(0,1006), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },
/*  secret.foo.bar.grunt.snort.seven */
    { PMDA_PMID(0,1007), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },

/* bigid */
    { PMDA_PMID(0,1023), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) },

/* End-of-List */
    { PM_ID_NULL, 0, 0, 0, { 0, 0, 0, 0, 0, 0 } }
};
static int	direct_map = 1;
static int	ndesc = sizeof(desctab)/sizeof(desctab[0]);

static pmDesc magic = 
    { PMDA_PMID(0,86), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(1,-1,0,PM_SPACE_BYTE,PM_TIME_SEC,0) };

static pmdaInstid _colour[] = {
    { 0, "red" }, { 1, "green" }, { 2, "blue" }
};

static pmdaInstid _bin[] = {
    { 100, "bin-100" }, { 200, "bin-200" }, { 300, "bin-300" },
    { 400, "bin-400" }, { 500, "bin-500" }, { 600, "bin-600" },
    { 700, "bin-700" }, { 800, "bin-800" }, { 900, "bin-900" }
};

static pmdaInstid _scramble[] = {
    { 100, "bin-100" }, { 200, "bin-200" }, { 300, "bin-300" },
    { 400, "bin-400" }, { 500, "bin-500" }, { 600, "bin-600" },
    { 700, "bin-700" }, { 800, "bin-800" }, { 900, "bin-900" }
};

static long scramble_ver = 0;

static pmdaInstid _family[] = {
    { 0, "colleen" }, { 1, "terry" }, { 2, "emma" }, { 3, "cathy" }, { 4, "fat bald bastard" }
};

static pmdaInstid _dodgey[] = {
    { 1, NULL}, { 2, NULL }, { 3, NULL }, { 4, NULL }, { 5, NULL }
};

static pmdaInstid _hordes[] = {
    {  0, "0" }, {  1, "1" }, {  2, "2" }, {  3, "3" }, {  4, "4" },
    {  5, "5" }, {  6, "6" }, {  7, "7" }, {  8, "8" }, {  9, "9" },
    { 10, "10" }, { 11, "11" }, { 12, "12" }, { 13, "13" }, { 14, "14" },
    { 15, "15" }, { 16, "16" }, { 17, "17" }, { 18, "18" }, { 19, "19" },
    { 20, "20" }, { 21, "21" }, { 22, "22" }, { 23, "23" }, { 24, "24" },
    { 25, "25" }, { 26, "26" }, { 27, "27" }, { 28, "28" }, { 29, "29" },
    { 30, "30" }, { 31, "31" }, { 32, "32" }, { 33, "33" }, { 34, "34" },
    { 35, "35" }, { 36, "36" }, { 37, "37" }, { 38, "38" }, { 39, "39" },
    { 40, "40" }, { 41, "41" }, { 42, "42" }, { 43, "43" }, { 44, "44" },
    { 45, "45" }, { 46, "46" }, { 47, "47" }, { 48, "48" }, { 49, "49" },
    { 50, "50" }, { 51, "51" }, { 52, "52" }, { 53, "53" }, { 54, "54" },
    { 55, "55" }, { 56, "56" }, { 57, "57" }, { 58, "58" }, { 59, "59" },
    { 60, "60" }, { 61, "61" }, { 62, "62" }, { 63, "63" }, { 64, "64" },
    { 65, "65" }, { 66, "66" }, { 67, "67" }, { 68, "68" }, { 69, "69" },
    { 70, "70" }, { 71, "71" }, { 72, "72" }, { 73, "73" }, { 74, "74" },
    { 75, "75" }, { 76, "76" }, { 77, "77" }, { 78, "78" }, { 79, "79" },
    { 80, "80" }, { 81, "81" }, { 82, "82" }, { 83, "83" }, { 84, "84" },
    { 85, "85" }, { 86, "86" }, { 87, "87" }, { 88, "88" }, { 89, "89" },
    { 90, "90" }, { 91, "91" }, { 92, "92" }, { 93, "93" }, { 94, "94" },
    { 95, "95" }, { 96, "96" }, { 97, "97" }, { 98, "98" }, { 99, "99" },
    {100, "100" }, {101, "101" }, {102, "102" }, {103, "103" }, {104, "104" },
    {105, "105" }, {106, "106" }, {107, "107" }, {108, "108" }, {109, "109" },
    {110, "110" }, {111, "111" }, {112, "112" }, {113, "113" }, {114, "114" },
    {115, "115" }, {116, "116" }, {117, "117" }, {118, "118" }, {119, "119" },
    {120, "120" }, {121, "121" }, {122, "122" }, {123, "123" }, {124, "124" },
    {125, "125" }, {126, "126" }, {127, "127" }, {128, "128" }, {129, "129" },
    {130, "130" }, {131, "131" }, {132, "132" }, {133, "133" }, {134, "134" },
    {135, "135" }, {136, "136" }, {137, "137" }, {138, "138" }, {139, "139" },
    {140, "140" }, {141, "141" }, {142, "142" }, {143, "143" }, {144, "144" },
    {145, "145" }, {146, "146" }, {147, "147" }, {148, "148" }, {149, "149" },
    {150, "150" }, {151, "151" }, {152, "152" }, {153, "153" }, {154, "154" },
    {155, "155" }, {156, "156" }, {157, "157" }, {158, "158" }, {159, "159" },
    {160, "160" }, {161, "161" }, {162, "162" }, {163, "163" }, {164, "164" },
    {165, "165" }, {166, "166" }, {167, "167" }, {168, "168" }, {169, "169" },
    {170, "170" }, {171, "171" }, {172, "172" }, {173, "173" }, {174, "174" },
    {175, "175" }, {176, "176" }, {177, "177" }, {178, "178" }, {179, "179" },
    {180, "180" }, {181, "181" }, {182, "182" }, {183, "183" }, {184, "184" },
    {185, "185" }, {186, "186" }, {187, "187" }, {188, "188" }, {189, "189" },
    {190, "190" }, {191, "191" }, {192, "192" }, {193, "193" }, {194, "194" },
    {195, "195" }, {196, "196" }, {197, "197" }, {198, "198" }, {199, "199" },
    {200, "200" }, {201, "201" }, {202, "202" }, {203, "203" }, {204, "204" },
    {205, "205" }, {206, "206" }, {207, "207" }, {208, "208" }, {209, "209" },
    {210, "210" }, {211, "211" }, {212, "212" }, {213, "213" }, {214, "214" },
    {215, "215" }, {216, "216" }, {217, "217" }, {218, "218" }, {219, "219" },
    {220, "220" }, {221, "221" }, {222, "222" }, {223, "223" }, {224, "224" },
    {225, "225" }, {226, "226" }, {227, "227" }, {228, "228" }, {229, "229" },
    {230, "230" }, {231, "231" }, {232, "232" }, {233, "233" }, {234, "234" },
    {235, "235" }, {236, "236" }, {237, "237" }, {238, "238" }, {239, "239" },
    {240, "240" }, {241, "241" }, {242, "242" }, {243, "243" }, {244, "244" },
    {245, "245" }, {246, "246" }, {247, "247" }, {248, "248" }, {249, "249" },
    {250, "250" }, {251, "251" }, {252, "252" }, {253, "253" }, {254, "254" },
    {255, "255" }, {256, "256" }, {257, "257" }, {258, "258" }, {259, "259" },
    {260, "260" }, {261, "261" }, {262, "262" }, {263, "263" }, {264, "264" },
    {265, "265" }, {266, "266" }, {267, "267" }, {268, "268" }, {269, "269" },
    {270, "270" }, {271, "271" }, {272, "272" }, {273, "273" }, {274, "274" },
    {275, "275" }, {276, "276" }, {277, "277" }, {278, "278" }, {279, "279" },
    {280, "280" }, {281, "281" }, {282, "282" }, {283, "283" }, {284, "284" },
    {285, "285" }, {286, "286" }, {287, "287" }, {288, "288" }, {289, "289" },
    {290, "290" }, {291, "291" }, {292, "292" }, {293, "293" }, {294, "294" },
    {295, "295" }, {296, "296" }, {297, "297" }, {298, "298" }, {299, "299" },
    {300, "300" }, {301, "301" }, {302, "302" }, {303, "303" }, {304, "304" },
    {305, "305" }, {306, "306" }, {307, "307" }, {308, "308" }, {309, "309" },
    {310, "310" }, {311, "311" }, {312, "312" }, {313, "313" }, {314, "314" },
    {315, "315" }, {316, "316" }, {317, "317" }, {318, "318" }, {319, "319" },
    {320, "320" }, {321, "321" }, {322, "322" }, {323, "323" }, {324, "324" },
    {325, "325" }, {326, "326" }, {327, "327" }, {328, "328" }, {329, "329" },
    {330, "330" }, {331, "331" }, {332, "332" }, {333, "333" }, {334, "334" },
    {335, "335" }, {336, "336" }, {337, "337" }, {338, "338" }, {339, "339" },
    {340, "340" }, {341, "341" }, {342, "342" }, {343, "343" }, {344, "344" },
    {345, "345" }, {346, "346" }, {347, "347" }, {348, "348" }, {349, "349" },
    {350, "350" }, {351, "351" }, {352, "352" }, {353, "353" }, {354, "354" },
    {355, "355" }, {356, "356" }, {357, "357" }, {358, "358" }, {359, "359" },
    {360, "360" }, {361, "361" }, {362, "362" }, {363, "363" }, {364, "364" },
    {365, "365" }, {366, "366" }, {367, "367" }, {368, "368" }, {369, "369" },
    {370, "370" }, {371, "371" }, {372, "372" }, {373, "373" }, {374, "374" },
    {375, "375" }, {376, "376" }, {377, "377" }, {378, "378" }, {379, "379" },
    {380, "380" }, {381, "381" }, {382, "382" }, {383, "383" }, {384, "384" },
    {385, "385" }, {386, "386" }, {387, "387" }, {388, "388" }, {389, "389" },
    {390, "390" }, {391, "391" }, {392, "392" }, {393, "393" }, {394, "394" },
    {395, "395" }, {396, "396" }, {397, "397" }, {398, "398" }, {399, "399" },
    {400, "400" }, {401, "401" }, {402, "402" }, {403, "403" }, {404, "404" },
    {405, "405" }, {406, "406" }, {407, "407" }, {408, "408" }, {409, "409" },
    {410, "410" }, {411, "411" }, {412, "412" }, {413, "413" }, {414, "414" },
    {415, "415" }, {416, "416" }, {417, "417" }, {418, "418" }, {419, "419" },
    {420, "420" }, {421, "421" }, {422, "422" }, {423, "423" }, {424, "424" },
    {425, "425" }, {426, "426" }, {427, "427" }, {428, "428" }, {429, "429" },
    {430, "430" }, {431, "431" }, {432, "432" }, {433, "433" }, {434, "434" },
    {435, "435" }, {436, "436" }, {437, "437" }, {438, "438" }, {439, "439" },
    {440, "440" }, {441, "441" }, {442, "442" }, {443, "443" }, {444, "444" },
    {445, "445" }, {446, "446" }, {447, "447" }, {448, "448" }, {449, "449" },
    {450, "450" }, {451, "451" }, {452, "452" }, {453, "453" }, {454, "454" },
    {455, "455" }, {456, "456" }, {457, "457" }, {458, "458" }, {459, "459" },
    {460, "460" }, {461, "461" }, {462, "462" }, {463, "463" }, {464, "464" },
    {465, "465" }, {466, "466" }, {467, "467" }, {468, "468" }, {469, "469" },
    {470, "470" }, {471, "471" }, {472, "472" }, {473, "473" }, {474, "474" },
    {475, "475" }, {476, "476" }, {477, "477" }, {478, "478" }, {479, "479" },
    {480, "480" }, {481, "481" }, {482, "482" }, {483, "483" }, {484, "484" },
    {485, "485" }, {486, "486" }, {487, "487" }, {488, "488" }, {489, "489" },
    {490, "490" }, {491, "491" }, {492, "492" }, {493, "493" }, {494, "494" },
    {495, "495" }, {496, "496" }, {497, "497" }, {498, "498" }, {499, "499" }
};

static pmdaInstid _events[] = {
    { 0, "fungus" }, { 1, "bogus" }
};

/* all domains supported in this PMDA - one entry each */
static pmdaIndom indomtab[] = {
#define COLOUR_INDOM	0
    { 0, 3, _colour },
#define BIN_INDOM	1
    { 0, 9, _bin },
#define MIRAGE_INDOM	2
    { 0, 0, NULL },
#define FAMILY_INDOM	3
    { 0, 5, _family },
#define HORDES_INDOM	4
    { 0, 500, _hordes },
#define DODGEY_INDOM	5
    { 0, 5, _dodgey },
#define DYNAMIC_INDOM	6
    { 0, 0, NULL },
#define MANY_INDOM	7
    { 0, 5, NULL },
#define SCRAMBLE_INDOM	8
    { 0, 9, _scramble },
#define EVENT_INDOM	9
    { 0, 2, _events },

    { PM_INDOM_NULL, 0, 0 }
};

static struct timeval	_then;		/* time we started */
static time_t		_start;		/* ditto */
static __pmProfile	*_profile;	/* last received profile */
static int		_x;
static pmdaIndom	*_idp;
static int		_singular = -1;	/* =0 for singular values */
static int		_ordinal = -1;	/* >=0 for non-singular values */
static int		_control;	/* the control variable */
static int		_mypid;
static int		_drift = 200;	/* starting value for drift */
static int		_sign = -1;	/* up/down for drift */
static int		_step = 20;	/* magnitude of step */
static int		_write_me = 2;	/* constant, but modifiable */
static __int32_t	_long = 13;	/* long.write_me */
static __uint32_t	_ulong = 13;	/* ulong.write_me */
static __int64_t	_longlong = 13;	/* longlong.write_me */
static __uint64_t	_ulonglong = 13;/* ulonglong.write_me */
static float		_float = 13;	/* float.write_me */
static double		_double = 13;	/* double.write_me */
static char		*_string;	/* string.write_me */
static pmValueBlock	*_aggr33;	/* aggregate.null */
static pmValueBlock	*_aggr34;	/* aggregate.hullo */
static pmValueBlock	*_aggr35;	/* aggregate.write_me */
static long		_col46;		/* lights */
static int		_n46;		/* sample count for lights */
static long		_mag47;		/* magnitude */
static int		_n47;		/* sample count for magnitude */
static __uint32_t	_rapid;		/* counts @ 8x10^8 per fetch */
static int		_dyn_max = -1;
static int		*_dyn_ctr;
static int		many_count = 5;

static pmValueBlock	*sivb=NULL;

static __int32_t	_wrap = 0;	/* wrap.long */
static __uint32_t	_u_wrap = 0;	/* wrap.ulong */
static __int64_t	_ll_wrap = 0;	/* wrap.longlong */
static __uint64_t	_ull_wrap = 0;	/* wrap.ulonglong */

static int		_error_code = 0;/* return this! */

static int		dodgey = 5;	/* dodgey.control */
static int		tmp_dodgey = 5;
static int		new_dodgey = 0;

static double		scale_step_bytes_up = 1;
static double		scale_step_bytes_down = 1;
static double		scale_step_count_up = 1;
static double		scale_step_count_down = 1;
static double		scale_step_time_up_secs = 1;
static double		scale_step_time_up_nanosecs = 1;
static double		scale_step_none_up = 1;
static int		scale_step_number[7] = {0,0,0,0,0,0,0};

static __uint32_t	const_rate_gradient = 0;
static __uint32_t	const_rate_value = 10485760;
static struct timeval	const_rate_timestamp = {0,0};

/* this needs to be visible in pmda.c */
int			not_ready = 0;	/* sleep interval in seconds */
int			sample_done = 0;/* pending request to terminate, see sample_store() */

int			_isDSO = 1;	/* =0 I am a daemon */

/*
 * dynamic PMNS metrics ... nothing to do with redo_dynamic() and dynamic
 * InDoms
 */
static struct {
    char	*name;
    pmID	pmid;
    int		mark;
} dynamic_ones[] = {
    { "secret.foo.bar.max.redirect", PMDA_PMID(0,0) },
    { "secret.bar", PMDA_PMID(0,1000) },
    { "secret.foo.one", PMDA_PMID(0,1001) },
    { "secret.foo.two", PMDA_PMID(0,1002) },
    { "secret.foo.bar.three", PMDA_PMID(0,1003) },
    { "secret.foo.bar.four", PMDA_PMID(0,1004) },
    { "secret.foo.bar.grunt.five", PMDA_PMID(0,1005) },
    { "secret.foo.bar.grunt.snort.six", PMDA_PMID(0,1006) },
    { "secret.foo.bar.grunt.snort.huff.puff.seven", PMDA_PMID(0,1007) }
};
static int	numdyn = sizeof(dynamic_ones)/sizeof(dynamic_ones[0]);

static int
redo_dynamic(void)
{
    int			err;
    int			i;
    int			sep = __pmPathSeparator();
    static struct stat	lastsbuf;
    struct stat		statbuf;
    pmdaIndom		*idp = &indomtab[DYNAMIC_INDOM];
    char		mypath[MAXPATHLEN];

    snprintf(mypath, sizeof(mypath), "%s%c" "sample" "%c" "dynamic.indom",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);

    if (stat(mypath, &statbuf) == 0) {
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
	if (statbuf.st_mtime != lastsbuf.st_mtime)
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
	if ((statbuf.st_mtimespec.tv_sec != lastsbuf.st_mtimespec.tv_sec) ||
	    (statbuf.st_mtimespec.tv_nsec != lastsbuf.st_mtimespec.tv_nsec))
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
	if ((statbuf.st_mtim.tv_sec != lastsbuf.st_mtim.tv_sec) ||
	    (statbuf.st_mtim.tv_nsec != lastsbuf.st_mtim.tv_nsec))
#else
!bozo!
#endif
								    {
	    FILE	*fspec;
	    int		newinst;
	    char	newname[100];	/* hack, secret max */
	    int		numinst;

	    lastsbuf = statbuf;
	    if ((fspec = fopen(mypath, "r")) != NULL) {
		for (i = 0; i < idp->it_numinst; i++) {
		    free(idp->it_set[i].i_name);
		}
		for (i = 0; i <= _dyn_max; i++) {
		    _dyn_ctr[i] = -_dyn_ctr[i];
		}
		free(idp->it_set);
		idp->it_numinst = 0;
		idp->it_set = NULL;
		numinst = 0;
		for ( ; ; ) {
		    if (fscanf(fspec, "%d %s", &newinst, newname) != 2)
			break;
		    numinst++;
		    if ((idp->it_set = (pmdaInstid *)realloc(idp->it_set, numinst * sizeof(pmdaInstid))) == NULL) {
			err = -oserror();
			fclose(fspec);
			return err;
		    }
		    idp->it_set[numinst-1].i_inst = newinst;
		    if ((idp->it_set[numinst-1].i_name = strdup(newname)) == NULL) {
			err = -oserror();
			free(idp->it_set);
			idp->it_set = NULL;
			fclose(fspec);
			return err;
		    }
		    if (newinst > _dyn_max) {
			if ((_dyn_ctr = (int *)realloc(_dyn_ctr, (newinst+1)*sizeof(_dyn_ctr[0]))) == NULL) {
			    err = -oserror();
			    free(idp->it_set);
			    idp->it_set = NULL;
			    fclose(fspec);
			    return err;
			}
			for (i = _dyn_max+1; i <= newinst; i++)
			    _dyn_ctr[i] = 0;
			_dyn_max = newinst;
		    }
		    _dyn_ctr[newinst] = -_dyn_ctr[newinst];
		}
		fclose(fspec);
		idp->it_numinst = numinst;

		for (i = 0; i <= _dyn_max; i++) {
		    if (_dyn_ctr[i] < 0)
			_dyn_ctr[i] = 0;
		}

#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0) {
		    fprintf(stderr, "redo instance domain for dynamic: numinst: %d\n", idp->it_numinst);
		    for (i = 0; i < idp->it_numinst; i++) {
			fprintf(stderr, " %d \"%s\"", idp->it_set[i].i_inst, idp->it_set[i].i_name);
		    }
		    fputc('\n', stderr);
		}
#endif
	    }
	}
    }
    else {
	/* control file is not present, empty indom if not already so */
	if (idp->it_set != NULL) {
	    for (i = 0; i < idp->it_numinst; i++) {
		free(idp->it_set[i].i_name);
	    }
	    free(idp->it_set);
	    idp->it_set = NULL;
	    idp->it_numinst = 0;
	    for (i = 0; i <= _dyn_max; i++) {
		_dyn_ctr[i] = 0;
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "redo instance domain for dynamic: numinst: 0 (no control file)\n");
#endif
	}
    }

    for (i = 0; i < idp->it_numinst; i++) {
	_dyn_ctr[idp->it_set[i].i_inst]++;
    }

    return 0;
}

#define MANY_MAX_LEN 10

static int
redo_many(void)
{
    pmdaIndom   	*idp;
    int			a;
    static char		*tags=NULL;
    char		*tag;

    /* sanity check, range clip */

    if (many_count<0) many_count=0;
    if (many_count>999999) many_count=999999;

    idp = &indomtab[MANY_INDOM];

    /* realloc instances buffer */

    idp->it_set = realloc(idp->it_set, many_count*sizeof(pmdaInstid));
    if (!idp->it_set) {
	idp->it_numinst=0;
	many_count=0;
	return -oserror();
    }

    /* realloc string buffer */

    tags = realloc(tags, many_count*MANY_MAX_LEN);
    if (!idp->it_set) {
	idp->it_numinst=0;
	many_count=0;
	return -oserror();
    }

    /* set number of instances */

    idp->it_numinst=many_count;

    /* generate instances */

    tag=tags;
    for (a=0;a<many_count;a++) {
	idp->it_set[a].i_inst=a;
	idp->it_set[a].i_name=tag;
	tag+=sprintf(tag,"i-%d",a)+1;
    }

    return 0;
}

static int
redo_mirage(void)
{
    static time_t	doit = 0;
    time_t		now;
    int			i;
    int			j;
    static int		newinst = 0;
    pmdaIndom		*idp;

    now = time(NULL);
    if (now < doit)
	return 0;

    idp = &indomtab[MIRAGE_INDOM];
    if (idp->it_set == NULL) {
	/* first time */
	if ((idp->it_set = (pmdaInstid *)malloc(sizeof(pmdaInstid))) == NULL)
	    return -oserror();
	if ((idp->it_set[0].i_name = (char *)malloc(5)) == NULL) {
	    idp->it_set = NULL;
	    return -oserror();
	}
	idp->it_numinst = 1;
	idp->it_set[0].i_inst = 0;
	sprintf(idp->it_set[0].i_name, "m-%02d", 0);
    }
    else {
	int	numinst;
	int	cull;

	numinst = 1;
	cull = idp->it_numinst > 12 ? idp->it_numinst/2 : idp->it_numinst;
	for (i = 1; i < idp->it_numinst; i++) {
	    if (lrand48() % 1000 < 1000 / cull) {
		/* delete this one */
		free(idp->it_set[i].i_name);
		continue;
	    }
	    idp->it_set[numinst++] = idp->it_set[i];
	}
	if (numinst != idp->it_numinst) {
	    if ((idp->it_set = (pmdaInstid *)realloc(idp->it_set, numinst * sizeof(pmdaInstid))) == NULL) {
		idp->it_set = NULL;
		idp->it_numinst = 0;
		return -oserror();
	    }
	    idp->it_numinst = numinst;
	}
	for (i = 0; i < 2; i++) {
	    if (lrand48() % 1000 < 500) {
		/* add a new one */
		numinst++;
		if ((idp->it_set = (pmdaInstid *)realloc(idp->it_set, numinst * sizeof(pmdaInstid))) == NULL) {
		    idp->it_set = NULL;
		    idp->it_numinst = 0;
		    return -oserror();
		}
		if ((idp->it_set[numinst-1].i_name = (char *)malloc(5)) == NULL) {
		    idp->it_set = NULL;
		    return -oserror();
		}
		for ( ; ; ) {
		    newinst = (newinst + 1) % 50;
		    for (j = 0; j < idp->it_numinst; j++) {
			if (idp->it_set[j].i_inst == newinst)
			    break;
		    }
		    if (j == idp->it_numinst)
			break;
		}
		idp->it_numinst = numinst;
		idp->it_set[numinst-1].i_inst = newinst;
		sprintf(idp->it_set[numinst-1].i_name, "m-%02d", newinst);
	    }
	}
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	fprintf(stderr, "redo instance domain for mirage: numinst: %d\n", idp->it_numinst);
	for (i = 0; i < idp->it_numinst; i++) {
	    fprintf(stderr, " %d \"%s\"", idp->it_set[i].i_inst, idp->it_set[i].i_name);
	}
	fputc('\n', stderr);
    }
#endif

    doit = now + 10;	/* no more than once every 10 seconds */
    return 0;
}

static void
redo_dodgey(void)
{
    int		j;
    int		k;

    if (dodgey <= 5) {
	tmp_dodgey = dodgey;
	new_dodgey = 0;
	/* re-build full instance table */
	for (j = 0; j < 5; j++) {
	    _dodgey[j].i_inst = j+1;
	    _dodgey[j].i_name[1] = '0' + j+1;
	}
	indomtab[DODGEY_INDOM].it_numinst = 5;
    }
    else {
	j = (int)(lrand48() % 1000);
	if (j < 33)
	    tmp_dodgey = PM_ERR_NOAGENT;
	else if (j < 66)
	    tmp_dodgey = PM_ERR_AGAIN;
	else if (j < 99)
	    tmp_dodgey = PM_ERR_APPVERSION;
	else {
	    /*
	     * create partial instance table, instances appear
	     * at random with prob = 0.5
	     */
	    k = 0;
	    for (j = 0; j < 5; j++) {
		if (lrand48() % 100 < 49) {
		    _dodgey[k].i_inst = j+1;
		    _dodgey[k].i_name[1] = '0' + j+1;
		    k++;
		}
	    }
	    tmp_dodgey = indomtab[DODGEY_INDOM].it_numinst = k;
	}
	/* fetches before re-setting */
	new_dodgey = (int)(lrand48() % dodgey);
    }
}

/*
 * count the number of instances in an instance domain
 */
static int
cntinst(pmInDom indom)
{
    pmdaIndom	*idp;

    if (indom == PM_INDOM_NULL)
	return 1;
    for (idp = indomtab; idp->it_indom != PM_INDOM_NULL; idp++) {
	if (idp->it_indom == indom)
	    return idp->it_numinst;
    }
    __pmNotifyErr(LOG_WARNING, "cntinst: unknown pmInDom 0x%x", indom);
    return 0;
}

/*
 * commence a new round of instance selection
 * flag == 1 for prefetch instance counting
 * flag == 0 for iteration over instances to retrieve values
 */
static void
startinst(pmInDom indom, int flag)
{
    _ordinal = _singular = -1;
    if (indom == PM_INDOM_NULL) {
	/* singular value */
	_singular = 0;
	return;
    }
    for (_idp = indomtab; _idp->it_indom != PM_INDOM_NULL; _idp++) {
	if (_idp->it_indom == indom) {
	    /* multiple values are possible */
	    _ordinal = 0;
	    if (flag == 1 && _idp == &indomtab[SCRAMBLE_INDOM]) {
		/*
		 * indomtab[BIN_INDOM].it_set[] is the same size as
		 * indomtab[SCRAMBLE_INDOM].it_set[] (maxnuminst
		 * entries)
		 */
		int	i;
		int	k = 0;
		int	maxnuminst = indomtab[BIN_INDOM].it_numinst;
		srand48((scramble_ver << 10) + 13);
		scramble_ver++;
		for (i = 0; i < maxnuminst; i++)
		    indomtab[SCRAMBLE_INDOM].it_set[i].i_inst = PM_IN_NULL;
		for (i = 0; i < maxnuminst; i++) {
		    /* skip 1/3 of instances */
		    if ((lrand48() % 100) < 33) continue;
		    /* order of instances is random */
		    for ( ; ; ) {
			k = lrand48() % maxnuminst;
			if (indomtab[SCRAMBLE_INDOM].it_set[k].i_inst != PM_IN_NULL)
			    continue;
			indomtab[SCRAMBLE_INDOM].it_set[k].i_inst = indomtab[BIN_INDOM].it_set[i].i_inst;
			indomtab[SCRAMBLE_INDOM].it_set[k].i_name = indomtab[BIN_INDOM].it_set[i].i_name;
			break;
		    }
		}
		/* pack to remove skipped instances */
		k = 0;
		for (i = 0; i < maxnuminst; i++) {
		    if (indomtab[SCRAMBLE_INDOM].it_set[i].i_inst == PM_IN_NULL)
			continue;
		    if (k < i) {
			indomtab[SCRAMBLE_INDOM].it_set[k].i_inst = indomtab[SCRAMBLE_INDOM].it_set[i].i_inst;
			indomtab[SCRAMBLE_INDOM].it_set[k].i_name = indomtab[SCRAMBLE_INDOM].it_set[i].i_name;
		    }
		    k++;
		}
		indomtab[SCRAMBLE_INDOM].it_numinst = k;
	    }
	    break;
	}
    }
}

/*
 * find next selected instance, if any
 *
 * EXCEPTION PCP 2.1.1: make use of __pmProfile much smarter, particularly when state for
 *	this indom is PM_PROFILE_EXCLUDE, then only need to consider inst
 *      values in the profile - this is a performance enhancement, and
 *      the simple method is functionally complete, particularly for
 *	stable (non-varying) instance domains
 */
static int
nextinst(int *inst)
{
    int		j;

    if (_singular == 0) {
	/* PM_INDOM_NULL ... just the one value */
	*inst = 0;
	_singular = -1;
	return 1;
    }
    if (_ordinal >= 0) {
	/* scan for next value in the profile */
	for (j = _ordinal; j < _idp->it_numinst; j++) {
	    if (__pmInProfile(_idp->it_indom, _profile, _idp->it_set[j].i_inst)) {
		*inst = _idp->it_set[j].i_inst;
		_ordinal = j+1;
		return 1;
	    }
	}
	_ordinal = -1;
    }
    return 0;
}

/*
 * this routine is called at initialization to patch up any parts of the
 * desctab that cannot be statically initialized, and to optionally
 * modify our Performance Metrics Domain Id (dom)
 */
static void
init_tables(int dom)
{
    int			i, allocsz;
    __pmInDom_int	b_indom;
    __pmInDom_int	*indomp;
    __pmID_int		*pmidp;
    pmDesc		*dp;

    /* serial numbering is arbitrary, but must be unique in this PMD */
    b_indom.flag = 0;
    b_indom.domain = dom;
    b_indom.serial = 1;
    indomp = (__pmInDom_int *)&indomtab[COLOUR_INDOM].it_indom;
    *indomp = b_indom;
    b_indom.serial++;
    indomp = (__pmInDom_int *)&indomtab[BIN_INDOM].it_indom;
    *indomp = b_indom;
    b_indom.serial++;
    indomp = (__pmInDom_int *)&indomtab[MIRAGE_INDOM].it_indom;
    *indomp = b_indom;
    b_indom.serial++;
    indomp = (__pmInDom_int *)&indomtab[FAMILY_INDOM].it_indom;
    *indomp = b_indom;
    b_indom.serial++;
    indomp = (__pmInDom_int *)&indomtab[HORDES_INDOM].it_indom;
    *indomp = b_indom;
    b_indom.serial++;
    indomp = (__pmInDom_int *)&indomtab[DODGEY_INDOM].it_indom;
    *indomp = b_indom;
    b_indom.serial++;
    indomp = (__pmInDom_int *)&indomtab[DYNAMIC_INDOM].it_indom;
    *indomp = b_indom;
    b_indom.serial++;
    indomp = (__pmInDom_int *)&indomtab[MANY_INDOM].it_indom;
    *indomp = b_indom;
    b_indom.serial++;
    indomp = (__pmInDom_int *)&indomtab[SCRAMBLE_INDOM].it_indom;
    *indomp = b_indom;
    b_indom.serial++;
    indomp = (__pmInDom_int *)&indomtab[EVENT_INDOM].it_indom;
    *indomp = b_indom;

    /* rewrite indom in desctab[] */
    for (dp = desctab; dp->pmid != PM_ID_NULL; dp++) {
	switch (dp->pmid) {
	    case PMDA_PMID(0,5):	/* colour */
	    case PMDA_PMID(0,92):	/* darkness */
		dp->indom = indomtab[COLOUR_INDOM].it_indom;
		break;
	    case PMDA_PMID(0,6):	/* bin or dupnames.two.bin or dupnames.three.bin */
	    case PMDA_PMID(0,48):	/* bucket */
	    case PMDA_PMID(0,50):	/* part_bin */
	    case PMDA_PMID(0,51):	/* bogus_bin */
	    case PMDA_PMID(0,103):	/* long.bin */
	    case PMDA_PMID(0,104):	/* long.bin_ctr */
	    case PMDA_PMID(0,105):	/* ulong.bin */
	    case PMDA_PMID(0,106):	/* ulong.bin_ctr */
	    case PMDA_PMID(0,107):	/* float.bin */
	    case PMDA_PMID(0,108):	/* float.bin_ctr */
	    case PMDA_PMID(0,109):	/* longlong.bin */
	    case PMDA_PMID(0,110):	/* longlong.bin_ctr */
	    case PMDA_PMID(0,111):	/* ulonglong.bin */
	    case PMDA_PMID(0,112):	/* ulonglong.bin_ctr */
	    case PMDA_PMID(0,113):	/* double.bin */
	    case PMDA_PMID(0,114):	/* double.bin_ctr */
		dp->indom = indomtab[BIN_INDOM].it_indom;
		break;
	    case PMDA_PMID(0,37):	/* mirage */
		dp->indom = indomtab[MIRAGE_INDOM].it_indom;
		break;
	    case PMDA_PMID(0,38):	/* mirage-longlong */
		dp->indom = indomtab[MIRAGE_INDOM].it_indom;
		break;
	    case PMDA_PMID(0,49):	/* needprofile */
		dp->indom = indomtab[FAMILY_INDOM].it_indom;
		break;
	    case PMDA_PMID(0,52):	/* hordes.one */
	    case PMDA_PMID(0,53):	/* hordes.two */
		dp->indom = indomtab[HORDES_INDOM].it_indom;
		break;
	    case PMDA_PMID(0,62):	/* dodgey.value */
		dp->indom = indomtab[DODGEY_INDOM].it_indom;
		break;
    	    case PMDA_PMID(0,76):	/* dynamic.counter */
    	    case PMDA_PMID(0,77): 	/* dynamic.discrete */
    	    case PMDA_PMID(0,78):	/* dynamic.instant */
		dp->indom = indomtab[DYNAMIC_INDOM].it_indom;
		break;
	    case PMDA_PMID(0,80):	/* many.int */
		dp->indom = indomtab[MANY_INDOM].it_indom;
		break;
	    case PMDA_PMID(0,121):	/* scramble.bin */
		dp->indom = indomtab[SCRAMBLE_INDOM].it_indom;
		break;
	    case PMDA_PMID(0,136):		/* event.records */
	    case PMDA_PMID(0,139):		/* event.highres_records */
		dp->indom = indomtab[EVENT_INDOM].it_indom;
		break;
	}
    }

    /* merge performance domain id part into PMIDs in pmDesc table */
    for (i = 0; desctab[i].pmid != PM_ID_NULL; i++) {
	pmidp = (__pmID_int *)&desctab[i].pmid;
	pmidp->domain = dom;
	if (direct_map && pmidp->item != i) {
	    direct_map = 0;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		__pmNotifyErr(LOG_WARNING, "sample_init: direct map disabled @ desctab[%d]", i);
	    }
#endif
	}
    }
    ndesc--;
    pmidp = (__pmID_int *)&magic.pmid;
    pmidp->domain = dom;

    /* local hacks */
    allocsz = roundup(sizeof("13"), 8);
    _string = (char *)calloc(1, allocsz);
    strncpy(_string, "13", sizeof("13"));
    allocsz = roundup(PM_VAL_HDR_SIZE, 8);
    _aggr33 = (pmValueBlock *)malloc(allocsz);
    _aggr33->vlen = PM_VAL_HDR_SIZE + 0;
    _aggr33->vtype = PM_TYPE_AGGREGATE;
    allocsz = roundup(PM_VAL_HDR_SIZE + strlen("hullo world!"), 8);
    _aggr34 = (pmValueBlock *)malloc(allocsz);
    _aggr34->vlen = PM_VAL_HDR_SIZE + strlen("hullo world!");
    _aggr34->vtype = PM_TYPE_AGGREGATE;
    memcpy(_aggr34->vbuf, "hullo world!", strlen("hullo world!"));
    allocsz = roundup(PM_VAL_HDR_SIZE + strlen("13"), 8);
    _aggr35 = (pmValueBlock *)malloc(allocsz);
    _aggr35->vlen = PM_VAL_HDR_SIZE + strlen("13");
    _aggr35->vtype = PM_TYPE_AGGREGATE;
    memcpy(_aggr35->vbuf, "13", strlen("13"));

    (void)redo_many();
}

static int
sample_profile(__pmProfile *prof, pmdaExt *ep)
{
    sample_inc_recv(ep->e_context);
    _profile = prof;	
    return 0;
}

static int
sample_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *ep)
{
    int		i;
    __pmInResult *res;
    pmdaIndom	*idp;
    int		err = 0;

    sample_inc_recv(ep->e_context);
    sample_inc_xmit(ep->e_context);

    if (not_ready > 0) {
	return limbo();
    }

    if (need_mirage && (i = redo_mirage()) < 0)
	return i;
    if (need_dynamic && (i = redo_dynamic()) < 0)
	return i;

    /*
     * check this is an instance domain we know about -- code below
     * assumes this test is complete
     */
    for (idp = indomtab; idp->it_indom != PM_INDOM_NULL; idp++) {
	if (idp->it_indom == indom)
	    break;
    }
    if (idp->it_indom == PM_INDOM_NULL)
	return PM_ERR_INDOM;

    if ((res = (__pmInResult *)malloc(sizeof(*res))) == NULL)
        return -oserror();
    res->indom = indom;

    if (name == NULL && inst == PM_IN_NULL)
	res->numinst = cntinst(indom);
    else
	res->numinst = 1;

    if (inst == PM_IN_NULL) {
	if ((res->instlist = (int *)malloc(res->numinst * sizeof(res->instlist[0]))) == NULL) {
	    free(res);
	    return -oserror();
	}
    }
    else
	res->instlist = NULL;

    if (name == NULL) {
	if ((res->namelist = (char **)malloc(res->numinst * sizeof(res->namelist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -oserror();
	}
	for (i = 0; i < res->numinst; i++)
	    res->namelist[0] = NULL;
    }
    else
	res->namelist = NULL;

    if (name == NULL && inst == PM_IN_NULL) {
	/* return inst and name for everything */
	for (i = 0; i < res->numinst; i++) {
	    res->instlist[i] = idp->it_set[i].i_inst;
	    if ((res->namelist[i] = strdup(idp->it_set[i].i_name)) == NULL) {
		__pmFreeInResult(res);
		return -oserror();
	    }
	}
    }
    else if (name == NULL) {
	/* given an inst, return the name */
	for (i = 0; i < idp->it_numinst; i++) {
	    if (inst == idp->it_set[i].i_inst) {
		if ((res->namelist[0] = strdup(idp->it_set[i].i_name)) == NULL) {
		    __pmFreeInResult(res);
		    return -oserror();
		}
		break;
	    }
	}
	if (i == idp->it_numinst)
	    err = 1;
    }
    else if (inst == PM_IN_NULL) {
	/* given a name, return an inst */
	char	*p;
	long		len;
	for (p = name; *p; p++) {
	    if (*p == ' ')
		break;
	}
	len = p - name;
	for (i = 0; i < idp->it_numinst; i++) {
	    if (strncmp(name, idp->it_set[i].i_name, len) == 0 &&
		strlen(idp->it_set[i].i_name) >= len &&
		(idp->it_set[i].i_name[len] == '\0' || idp->it_set[i].i_name[len] == ' ')) {
		res->instlist[0] = idp->it_set[i].i_inst;
		break;
	    }
	}
	if (i == idp->it_numinst)
	    err = 1;
    }
    else
	err = 1;
    if (err == 1) {
	/* bogus arguments or instance id/name */
	__pmFreeInResult(res);
	return PM_ERR_INST;
    }

    *result = res;
    return 0;
}

static int
sample_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    int		i;
    const char	*p;

    /* skip the sample. or sampledso. part */
    for (p = name; *p != '.' && *p; p++)
	;
    if (*p == '.') p++;
    
    for (i = 0; i < numdyn; i++) {
	if (strcmp(p, dynamic_ones[i].name) == 0) {
	    *pmid = dynamic_ones[i].pmid;
	    return 0;
	}
    }

    return PM_ERR_NAME;
}

static int
sample_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    size_t	len = 0;
    int		nmatch = 0;
    int		i;
    char	*pfx;
    char	*p;
    char	**list;

    if (_isDSO)
	pfx = "sampledso.";
    else
	pfx = "sample.";

    for (i = 0; i < numdyn; i++) {
	if (dynamic_ones[i].pmid == pmid) {
	    nmatch++;
	    len += strlen(pfx)+strlen(dynamic_ones[i].name)+1;
	}
    }

    if (nmatch == 0)
	return PM_ERR_PMID;

    len += nmatch*sizeof(char *);	/* pointers to names */

    if ((list = (char **)malloc(len)) == NULL)
	return -oserror();

    p = (char *)&list[nmatch];
    nmatch = 0;
    for (i = 0; i < numdyn; i++) {
	if (dynamic_ones[i].pmid == pmid) {
	    list[nmatch++] = p;
	    strcpy(p, pfx);
	    p += strlen(pfx);
	    strcpy(p, dynamic_ones[i].name);
	    p += strlen(dynamic_ones[i].name);
	    *p++ = '\0';
	}
    }
    *nameset = list;

    return nmatch;
}

static int
sample_children(const char *name, int traverse, char ***offspring, int **status, pmdaExt *pmda)
{
    int		i;
    int		j;
    int		nmatch;
    int		pfxlen;
    int		namelen;
    const char	*p;
    char	*q;
    char	*qend = NULL;
    char	**chn = NULL;
    int		*sts = NULL;
    size_t	len = 0;
    size_t	tlen = 0;

    /* skip the sample. or sampledso. part */
    for (p = name; *p != '.' && *p; p++)
	;
    pfxlen = p - name;
    if (*p == '.') p++;
    namelen = strlen(p);

    nmatch = 0;
    for (i = 0; i < numdyn; i++) {
	q = dynamic_ones[i].name;
	if (strncmp(p, q, namelen) != 0) {
	    /* no prefix match */
	    dynamic_ones[i].mark = 0;
	    continue;
	}
	if (traverse == 0 && q[namelen] != '.') {
	    /* cannot be a child of name */
	    dynamic_ones[i].mark = 0;
	    continue;
	}
	if (traverse == 1 && q[namelen] != '.' && q[namelen] != '\0') {
	    /* cannot be name itself, not a child of name */
	    dynamic_ones[i].mark = 0;
	    continue;
	}
	if (traverse == 0) {
	    qend = &q[namelen+1];
	    while (*qend && *qend != '.')
		qend++;
	    tlen = qend - &q[namelen+1];
	    for (j = 0; j < nmatch; j++) {
		if (strncmp(&q[namelen+1], chn[j], tlen) == 0) {
		    /* already seen this child ... skip it */
		    break;
		}
	    }
	}
	else {
	    /* traversal ... need this one */
	    j = nmatch;
	}
	if (j == nmatch) {
	    nmatch++;
	    if ((chn = (char **)realloc(chn, nmatch*sizeof(chn[0]))) == NULL) {
		j = -oserror();
		goto fail;
	    }
	    if ((sts = (int *)realloc(sts, nmatch*sizeof(sts[0]))) == NULL) { 
		j = -oserror();
		goto fail;
	    }
	    if (traverse == 0) {
		/*
		 * descendents only ... just want the next component of
		 * PMNS name
		 */
		if ((chn[nmatch-1] = (char *)malloc(tlen+1)) == NULL) {
		    j = -oserror();
		    goto fail;
		}
		strncpy(chn[nmatch-1], &q[namelen+1], tlen);
		chn[nmatch-1][tlen] = '\0';
		if (*qend == '.')
		    sts[nmatch-1] = PMNS_NONLEAF_STATUS;
		else
		    sts[nmatch-1] = PMNS_LEAF_STATUS;
	    }
	    else {
		/*
		 * traversal ... want the whole name including the prefix
		 * part
		 */
		tlen = pfxlen + strlen(dynamic_ones[i].name) + 2;
		if ((chn[nmatch-1] = malloc(tlen)) == NULL) {
		    j = -oserror();
		    goto fail;
		}
		strncpy(chn[nmatch-1], name, pfxlen);
		chn[nmatch-1][pfxlen] = '.';
		chn[nmatch-1][pfxlen+1] = '\0';
		strcat(chn[nmatch-1], dynamic_ones[i].name);
		sts[nmatch-1] = PMNS_LEAF_STATUS;
	    }
	    len += tlen + 1;
	}
    }
    if (nmatch == 0) {
	*offspring = NULL;
	*status = NULL;
    }
    else {
	if ((chn = (char **)realloc(chn, nmatch*sizeof(chn[0])+len)) == NULL) {
	    j = -oserror();
	    goto fail;
	}
	q = (char *)&chn[nmatch];
	for (j = 0; j < nmatch; j++) {
	    strcpy(q, chn[j]);
	    free(chn[j]);
	    chn[j] = q;
	    q += strlen(chn[j])+1;
	}
	*offspring = chn;
	*status = sts;
    }
    return nmatch;

fail:
    /*
     * come here with j as negative error code, and some allocation failure for
     * sts[] or chn[] or chn[nmatch-1][]
     */
     if (sts != NULL) free(sts);
     if (chn != NULL) {
	for (i = 0; i < nmatch-1; i++) {
	    if (chn[i] != NULL) free(chn[i]);
	}
	free(chn);
     }
     return j;
}

static int
sample_attribute(int ctx, int attr, const char *value, int length, pmdaExt *pmda)
{
    /*
     * We have no special security or other requirements, so we're just
     * going to log any connection attribute messages we happen to get
     * from pmcd (handy for demo and testing purposes).
     */
    return pmdaAttribute(ctx, attr, value, length, pmda);
}

/*
 * high precision counter
 */
typedef union {
    __uint32_t	half[2];
    __uint64_t	full;
} pmHPC_t;

#ifdef HAVE_NETWORK_BYTEORDER
#define PM_HPC_TOP	0
#define PM_HPC_BOTTOM	1
#else
#define PM_HPC_TOP	1
#define PM_HPC_BOTTOM	0
#endif

void
_pmHPCincr(pmHPC_t *ctr, __uint32_t val)
{
    if (val < ctr->half[PM_HPC_BOTTOM])
	/* assume single overflow */
	ctr->half[PM_HPC_TOP]++;
    ctr->half[PM_HPC_BOTTOM] = val;
}

static pmHPC_t	rapid_ctr;

static int
sample_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *ep)
{
    int		i;		/* over pmidlist[] */
    int		j;		/* over vset->vlist[] */
    int		sts;
    int		need;
    int		inst;
    int		numval;
    static pmResult	*res;
    static int		maxnpmids;
    static int		nbyte;
    __uint32_t		*ulp;
    unsigned long	ul;
    struct timeval	now;
    pmValueSet	*vset;
    pmDesc	*dp;
    __pmID_int	*pmidp;
    pmAtomValue	atom;
    int		type;

    sample_inc_recv(ep->e_context);
    sample_inc_xmit(ep->e_context);

    if (not_ready > 0) {
	return limbo();
    }

    if (numpmid > maxnpmids) {
	if (res != NULL)
	    free(res);
	/* (numpmid - 1) because there's room for one valueSet in a pmResult */
	need = (int)sizeof(pmResult) + (numpmid - 1) * (int)sizeof(pmValueSet *);
	if ((res = (pmResult *)malloc(need)) == NULL)
	    return -oserror();
	maxnpmids = numpmid;
    }
    res->timestamp.tv_sec = 0;
    res->timestamp.tv_usec = 0;
    res->numpmid = numpmid;

    if (need_mirage && (j = redo_mirage()) < 0)
	return j;
    if (need_dynamic && (j = redo_dynamic()) < 0)
	return j;

    if (new_dodgey < 0)
	redo_dodgey();

    for (i = 0; i < numpmid; i++) {
	pmidp = (__pmID_int *)&pmidlist[i];

	if (direct_map) {
	    j = pmidp->item;
	    if (j < ndesc && desctab[j].pmid == pmidlist[i]) {
		dp = &desctab[j];
		goto doit;
	    }
	}
	for (dp = desctab; dp->pmid != PM_ID_NULL; dp++) {
	    if (dp->pmid == pmidlist[i])
		break;
	}
doit:

	if (dp->pmid != PM_ID_NULL) {
	    /* the special cases */
	    if (pmidp->cluster == 0 && pmidp->item == 86) {
		dp = &magic;
		numval = 1;
	    }
	    else if (pmidp->cluster == 0 && pmidp->item == 54)
		numval = PM_ERR_PMID;
	    else if (pmidp->cluster == 0 && pmidp->item == 92)	/* darkness */
		numval = 0;
	    else if (pmidp->cluster == 0 && pmidp->item == 138)	/* bad.novalues */
		numval = 0;
	    else if (pmidp->cluster == 0 &&
	             (pmidp->item == 127 ||	/* event.type */
		      pmidp->item == 128 ||	/* event.param_32 */
		      pmidp->item == 129 ||	/* event.param_u32 */
		      pmidp->item == 130 ||	/* event.param_64 */
		      pmidp->item == 131 ||	/* event.param_u64 */
		      pmidp->item == 132 ||	/* event.param_float */
		      pmidp->item == 133 ||	/* event.param_double */
		      pmidp->item == 134 ||	/* event.param_string */
		      pmidp->item == 135))	/* event.param_aggregate */
		numval = 0;
	    else if (dp->type == PM_TYPE_NOSUPPORT)
		numval = PM_ERR_APPVERSION;
	    else if (dp->indom != PM_INDOM_NULL) {
		/* count instances in the profile */
		numval = 0;
		/* special case(s) */
		if (pmidp->cluster == 0 && pmidp->item == 49) {
		    int		kp;
		    /* needprofile - explict instances required */

		    numval = PM_ERR_PROFILE;
		    for (kp = 0; kp < _profile->profile_len; kp++) {
			if (_profile->profile[kp].indom != dp->indom)
			    continue;
			if (_profile->profile[kp].state == PM_PROFILE_EXCLUDE &&
			    _profile->profile[kp].instances_len != 0)
				numval = 0;
			break;
		    }
		}
		else if (pmidp->cluster == 0 && (pmidp->item == 76 || pmidp->item == 77 || pmidp->item == 78)) {
		    /*
		     * if $(PCP_VAR_DIR)/pmdas/sample/dynamic.indom is not present,
		     * then numinst will be zero after the redo_dynamic() call
		     * in sample_init(), which makes zero loops through the
		     * fetch loop, so cannot set need_dynamic there ...
		     * do it here if not already turned on
		     */
		    if (need_dynamic == 0) {
			need_dynamic = 1;
			if ((j = redo_dynamic()) < 0)
			    return j;
		    }
		}
		if (numval == 0) {
		    /* count instances in indom */
		    startinst(dp->indom, 1);
		    while (nextinst(&inst)) {
			/* special case ... not all here for part_bin */
			if (pmidp->cluster == 0 && pmidp->item == 50 && (inst % 200) == 0)
			    continue;
			numval++;
		    }
		}
	    }
	    else {
		/* special case(s) for singular instance domains */
		if (pmidp->cluster == 0 && pmidp->item == 9) {
		    /* surprise! no value available */
		    numval = 0;
		}
		else
		    numval = 1;
	    }
	}
	else
	    numval = 0;

	/* Must use individual malloc()s because of pmFreeResult() */
	if (numval >= 1)
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet) + 
					    (numval - 1)*sizeof(pmValue));
	else
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet) - 
					    sizeof(pmValue));
	if (vset == NULL) {
	    if (i) {
		res->numpmid = i;
		__pmFreeResultValues(res);
	    }
	    return -oserror();
	}
	vset->pmid = pmidlist[i];
	vset->numval = numval;
	vset->valfmt = PM_VAL_INSITU;
	if (vset->numval <= 0)
	    continue;

	if (dp->indom == PM_INDOM_NULL)
	    inst = PM_IN_NULL;
	else {
	    startinst(dp->indom, 0);
	    nextinst(&inst);
	}
	type = dp->type;
	j = 0;
	do {
	    if (pmidp->cluster == 0 && pmidp->item == 50 && inst % 200 == 0)
		goto skip;
	    if (pmidp->cluster == 0 && pmidp->item == 51 && inst % 200 == 0)
		inst += 50;
	    if (j == numval) {
		/* more instances than expected! */
		numval++;
		res->vset[i] = vset = (pmValueSet *)realloc(vset,
			    sizeof(pmValueSet) + (numval - 1)*sizeof(pmValue));
		if (vset == NULL) {
		    if (i) {
			res->numpmid = i;
			__pmFreeResultValues(res);
		    }
		    return -oserror();
		}
	    }
	    vset->vlist[j].inst = inst;
	    /*
	     * we mostly have cluster 0, metric already found in desctab[]
	     * so no checking needed
	     */
	    if (pmidp->cluster == 0) {
		switch (pmidp->item) {
		    case 0:		/* control */
			atom.l = _control;
			break;
		    case 1:		/* daemon_pid or sample.dupnames.daemon_pid or sample.dupnames.pid_daemon */
			if (_mypid == 0) _mypid = (int)getpid();
			atom.ul = _mypid;
			break;
		    case 2:		/* seconds or sample.dupnames.seconds */
			atom.ul = time(NULL) - _start;
			break;
		    case 3:		/* milliseconds */
			__pmtimevalNow(&now);
			atom.d = 1000 * __pmtimevalSub(&now, &_then);
			break;
		    case 4:		/* load */
			atom.l = 42;
			break;
		    case 5:		/* colour */
			switch (inst) {
			    case 0:		/* "red" */
				_x = (_x + 1) % 100;
				atom.l = _x + 100;
				break;
			    case 1:		/* "green" */
				_x = (_x + 1) % 100;
				atom.l = _x + 200;
				break;
			    case 2:		/* "blue" */
				_x = (_x + 1) % 100;
				atom.l = _x + 300;
				break;
			}
			break;
		    case 6:		/* bin or dupnames.two.bin or dupnames.three.bin */
		    case 48:
		    case 50:
		    case 51:
		    case 103:		/* long.bin & long.bin_ctr */
		    case 104:
		    case 121:		/* scramble.bin */
			/* the value is the instance identifier (sic) */
			atom.l = inst;
			break;
			/* and ditto for all the other type variants of "bin" */
		    case 105:		/* ulong.bin & ulong.bin_ctr */
		    case 106:
			atom.ul = inst;
			break;
		    case 107:		/* float.bin & float.bin_ctr */
		    case 108:
			atom.f = inst;
			break;
		    case 109:		/* longlong.bin & longlong.bin_ctr */
		    case 110:
			atom.ll = inst;
			break;
		    case 111:		/* ulonglong.bin & ulonglong.bin_ctr */
		    case 112:
			atom.ull = inst;
			break;
		    case 113:		/* double.bin & double.bin_ctr */
		    case 114:
			atom.d = inst;
			break;
		    case 7:		/* drift */
			_drift = _drift + _sign * (int)(lrand48() % 50);
			if (_drift < 0) _drift = 0;
			atom.l = _drift;
			if ((lrand48() % 100) < 20) {
			    if (_sign == 1)
				_sign = -1;
			    else
				_sign = 1;
			}
			break;
		    case 63:		/* step_counter */
		    case 8:		/* step every 30 seconds */
			atom.l = (1 + (time(NULL) - _start) / 30) * _step;
			break;
		    case 40:
			/* total pdu count for all contexts */
			atom.ll = (__int64_t)sample_get_recv(CTX_ALL) + (__int64_t)sample_get_xmit(CTX_ALL);
			break;
		    case 41:
			/* recv pdu count for all contexts */
			atom.l = sample_get_recv(CTX_ALL);
			break;
		    case 42:
			/* xmit pdu count for all contexts */
			atom.l = sample_get_xmit(CTX_ALL);
			break;
		    case 43:
		    case 44:
		    case 45:
		    case 122:
		    case 123:
		    case 124:
		    case 125:
			/* percontext.pdu */
			/* percontext.recv-pdu */
			/* percontext.xmit-pdu */
			/* percontext.control.ctx */
			/* percontext.control.active */
			/* percontext.control.start */
			/* percontext.control.end */
			atom.l = sample_ctx_fetch(ep->e_context, pmidp->item);
			break;
		    case 37:
			/* mirage */
			_x = (_x + 1) % 100;
			atom.l = (inst + 1) * 100 - _x;
			need_mirage = 1;
			break;
		    case 36:
			/* write_me */
			atom.l = _write_me;
			break;
		    case 39:
			/* sysinfo */
			if (!sivb) {
			    /* malloc and init the pmValueBlock for
			     * sysinfo first type around */

			    int size = sizeof(pmValueBlock) - sizeof(int);

#ifdef IS_SOLARIS
			    size += MAX_SYSNAME;
#else
			    size += sizeof (struct sysinfo);
#endif

			    if ((sivb = calloc(1, size)) == NULL ) 
				return PM_ERR_GENERIC;

			    sivb->vlen = size;
			    sivb->vtype = PM_TYPE_AGGREGATE;
			}

#ifdef HAVE_SYSINFO
#ifdef IS_SOLARIS
			sysinfo(SI_SYSNAME, sivb->vbuf, MAX_SYSNAME);
#else
			sysinfo((struct sysinfo *)sivb->vbuf);
#endif
#else
			strncpy((char *)sivb->vbuf, si.dummy, sizeof(struct sysinfo));
#endif
			atom.vbp = sivb;

			/*
			 * pv:782029 The actual type must be PM_TYPE_AGGREGATE, 
			 *           but we have to tell pmStuffValue it's a
			 *           PM_TYPE_AGGREGATE_STATIC
			 */
			type = PM_TYPE_AGGREGATE_STATIC;
			break;
		    case 46:		/* lights or dupnames.two.lights */
			if (_n46 == 0) {
			    _col46 = lrand48() % 3;
			    _n46 = 1 + (int)(lrand48() % 10);
			}
			_n46--;
			switch (_col46) {
			    case 0:
				atom.cp = "red";
				break;
			    case 1:
				atom.cp = "yellow";
				break;
			    case 2:
				atom.cp = "green";
				break;
			}
			break;
		    case 47:
			if (_n47 == 0) {
			    _mag47 = 1 << (1 + (int)(lrand48() % 6));
			    _n47 = 1 + (int)(lrand48() % 5);
			}
			_n47--;
			atom.l = (__int32_t)_mag47;
			break;
		    case 38:
			/* mirage-longlong */
			_x = (_x + 1) % 100;
			atom.ll = (inst + 1) * 100 - _x;
			atom.ll *= 1000000;
			need_mirage = 1;
			break;
		    case 49:
			/* need profile */
			switch (inst) {
			    case 0:		/* "colleen" */
				atom.f = 3.05;
				break;
			    case 1:		/* "terry" */
				atom.f = 12.05;
				break;
			    case 2:		/* "emma" */
			    case 3:		/* "cathy" */
				atom.f = 11.09;
				break;
			    case 4:		/* "alexi" */
				atom.f = 5.26;
				break;
			}
			break;
		    case 10:		/* long.* group */
			atom.l = 1;
			break;
		    case 11:
			atom.l = 10;
			break;
		    case 12:
			atom.l = 100;
			break;
		    case 13:
			atom.l = 1000000;
			break;
		    case 14:
			atom.l = (__int32_t)_long;
			break;
		    case 20:		/* longlong.* group */
#if !defined(HAVE_CONST_LONGLONG)
			atom.ll = 1;
#else
			atom.ll = 1LL;
#endif
			break;
		    case 21:
#if !defined(HAVE_CONST_LONGLONG)
			atom.ll = 10;
#else
			atom.ll = 10LL;
#endif
			break;
		    case 22:
#if !defined(HAVE_CONST_LONGLONG)
			atom.ll = 100;
#else
			atom.ll = 100LL;
#endif
			break;
		    case 23:
#if !defined(HAVE_CONST_LONGLONG)
			atom.ll = 1000000;
#else
			atom.ll = 1000000LL;
#endif
			break;
		    case 24:
			atom.ll = _longlong;
			break;
		    case 15:		/* float.* group */
			atom.f = 1;
			break;
		    case 16:		/* float.ten or dupnames.two.float.ten */
			atom.f = 10;
			break;
		    case 17:
			atom.f = 100;
			break;
		    case 18:
			atom.f = 1000000;
			break;
		    case 19:
			atom.f = _float;
			break;
		    case 25:		/* double.* group */
			atom.d = 1;
			break;
		    case 26:		/* double.ten or dupnames.two.double.ten */
			atom.d = 10;
			break;
		    case 27:
			atom.d = 100;
			break;
		    case 28:
			atom.d = 1000000;
			break;
		    case 29:
			atom.d = _double;
			break;
		    case 30:
			atom.cp = "";
			break;
		    case 31:
			atom.cp = "hullo world!";
			break;
		    case 32:
			atom.cp = _string;
			break;
		    case 33:
			atom.vbp = _aggr33;
			break;
		    case 34:
			atom.vbp = _aggr34;
			break;
		    case 35:
			atom.vbp = _aggr35;
			break;
		    case 52:
			atom.l = inst;
			break;
		    case 53:
			atom.l = 499 - inst;
			break;
		    case 56:
			atom.l = not_ready;
			break;
		    case 57:
			_wrap += INT_MAX / 2 - 1;
			atom.l = _wrap;
			break;
		    case 58:
			_u_wrap += UINT_MAX / 2 - 1;
			atom.ul = _u_wrap;
			break;
		    case 59:
			_ll_wrap += LONGLONG_MAX / 2 - 1;
			atom.ll = _ll_wrap;
			break;
		    case 60:
			_ull_wrap += ULONGLONG_MAX / 2 - 1;
			atom.ull = _ull_wrap;
			break;
		    case 61:
			atom.l = dodgey;
			break;
		    case 62:
			if (dodgey > 5 && j == 0)
			    new_dodgey--;
			if (tmp_dodgey <= 0) {
			    j = tmp_dodgey;
			    goto done;
			}
			else if (tmp_dodgey <= 5) {
			    if (inst > tmp_dodgey)
				goto skip;
			}
			atom.l = (int)(lrand48() % 101);
			break;
		    case 64:
			_rapid += 80000000;
			_pmHPCincr(&rapid_ctr, _rapid);
			atom.ul = (__uint32_t)(rapid_ctr.full * 10);
			break;
		    case 65: /* scale_step.bytes_up */
			atom.d = scale_step_bytes_up;
			if (++scale_step_number[0] % 5 == 0) {
			    if (scale_step_bytes_up < 1024.0*1024.0*1024.0*1024.0)
				scale_step_bytes_up *= 2;
			    else
				scale_step_bytes_up = 1;
			}
			break;
		    case 66: /* scale_step.bytes_down */
			atom.d = scale_step_bytes_down;
			if (++scale_step_number[1] % 5 == 0) {
			    if (scale_step_bytes_down > 1)
				scale_step_bytes_down /= 2;
			    else
				scale_step_bytes_down = 1024.0*1024.0*1024.0*1024.0;
			}
			break;
		    case 67: /* scale_step.count_up */
			atom.d = scale_step_count_up;
			if (++scale_step_number[2] % 5 == 0) {
			    if (scale_step_count_up < 1.0e12)
				scale_step_count_up *= 10;
			    else
				scale_step_count_up = 1;
			}
			break;
		    case 68: /* scale_step.count_down */
			atom.d = scale_step_count_down;
			if (++scale_step_number[3] % 5 == 0) {
			    if (scale_step_count_down > 1)
				scale_step_count_down /= 10;
			    else
				scale_step_count_down = 1.0e12;
			}
			break;
		    case 69: /* scale_step.time_up_secs */
			atom.d = scale_step_time_up_secs;
			if (++scale_step_number[4] % 5 == 0) {
			    if (scale_step_time_up_secs < 60*60*24)
				scale_step_time_up_secs *= 10;
			    else
				scale_step_time_up_secs = 1;
			}
			break;
		    case 70: /* scale_step.time_up_nanosecs */
			atom.d = scale_step_time_up_nanosecs;
			if (++scale_step_number[5] % 5 == 0) {
			    if (scale_step_time_up_nanosecs < 1e9*60*60*24)
				scale_step_time_up_nanosecs *= 10;
			    else
				scale_step_time_up_nanosecs = 1;
			}
			break;
		    case 71: /* scale_step.none_up */
			atom.d = scale_step_none_up;
			if (++scale_step_number[6] % 5 == 0) {
			    if (scale_step_none_up < 10000000)
				scale_step_none_up *= 10;
			    else
				scale_step_none_up = 1;
			}
			break;
		    case 72: /* const_rate.value */
			__pmtimevalNow(&now);
			atom.ul = const_rate_value + const_rate_gradient * __pmtimevalSub(&now, &const_rate_timestamp);
			const_rate_timestamp = now;
			const_rate_value = atom.ul;
			break;
		    case 73: /* const_rate.gradient */
			atom.ul = const_rate_gradient;
			break;
		    case 74: /* error_code */
			atom.l = _error_code;
			break;
		    case 75: /* error_check */
			if (_error_code < 0)
			    return _error_code;
			atom.l = 0;
			break;
		    case 76:	/* dynamic.counter */
		    case 77: 	/* dynamic.discrete */
		    case 78:	/* dynamic.instant */
			if (inst > _dyn_max) {
			    /* bad instance! */
			    goto done;
			}
			atom.l = _dyn_ctr[inst];
			break;
		    case 79:	/* many.count */
			atom.l=many_count;
			break;
		    case 80:	/* many.int */
			atom.l = inst;
			break;
		    case 81:	/* byte_ctr */
			nbyte += lrand48() % 1024;
			atom.l = nbyte;
			break;
		    case 82:	/* byte_rate */
			atom.l = (int)(lrand48() % 1024);
			break;
		    case 83:	/* kbyte_ctr */
			nbyte += lrand48() % 1024;
			atom.l = nbyte;
			break;
		    case 84:	/* kbyte_rate */
			atom.l = (int)(lrand48() % 1024);
			break;
		    case 85:	/* byte_rate_per_hour */
			atom.l = (int)(lrand48() % 1024);
			break;
		    case 86:	/* dynamic.meta.metric */
			switch (magic.type) {
			    case PM_TYPE_32:
				atom.l = 42;
				break;
			    case PM_TYPE_U32:
				atom.ul = 42;
				break;
			    case PM_TYPE_64:
				atom.ll = 42;
				break;
			    case PM_TYPE_U64:
				atom.ull = 42;
				break;
			    case PM_TYPE_FLOAT:
				atom.f = 42;
				break;
			    case PM_TYPE_DOUBLE:
				atom.d = 42;
				break;
			    default:
				/* do nothing in other cases ... return garbage */
				break;
			}
			break;
		    case 87:	/* dynamic.meta.pmdesc.type */
			atom.ul = magic.type;
			break;
		    case 88:	/* dynamic.meta.pmdesc.indom */
			atom.ul = magic.indom;
			break;
		    case 89:	/* dynamic.meta.pmdesc.sem */
			atom.ul = magic.sem;
			break;
		    case 90:	/* dynamic.meta.pmdesc.units */
			ulp = (__uint32_t *)&magic.units;
			atom.ul = *ulp;
			break;
		    case 91:	/* datasize */
			__pmProcessDataSize(&ul);
			atom.ul = ul;
			break;
		    /* no case 92 for darkeness, handled above */
		    case 93:		/* ulong.* group */
			atom.ul = 1;
			break;
		    case 94:
			atom.ul = 10;
			break;
		    case 95:
			atom.ul = 100;
			break;
		    case 96:
			atom.ul = 1000000;
			break;
		    case 97:
			atom.ul = (__int32_t)_ulong;
			break;
		    case 98:		/* ulonglong.* group */
#if !defined(HAVE_CONST_LONGLONG)
			atom.ull = 1;
#else
			atom.ull = 1ULL;
#endif
			break;
		    case 99:
#if !defined(HAVE_CONST_LONGLONG)
			atom.ull = 10;
#else
			atom.ull = 10ULL;
#endif
			break;
		    case 100:
#if !defined(HAVE_CONST_LONGLONG)
			atom.ull = 100;
#else
			atom.ull = 100ULL;
#endif
			break;
		    case 101:
#if !defined(HAVE_CONST_LONGLONG)
			atom.ull = 1000000;
#else
			atom.ull = 1000000ULL;
#endif
			break;
		    case 102:
			atom.ull = _ulonglong;
			break;
		    case 115:	/* ulong.count.base */
			atom.ul = 42000000;
			break;
		    case 116:	/* ulong.count.deca */
			atom.ul = 4200000;
			break;
		    case 117:	/* ulong.count.hecto */
			atom.ul = 420000;
			break;
		    case 118:	/* ulong.count.kilo */
			atom.ul = 42000;
			break;
		    case 119:	/* ulong.count.mega */
			atom.ul = 42;
			break;
		    case 120:	/* scramble.version */
			atom.ll = scramble_ver;
			break;
		    case 126:	/* event.reset */
			atom.l = event_get_fetch_count();
			break;
		    case 136:	/* event.records */
		    case 137:	/* event.no_indom_records */
			if ((sts = sample_fetch_events(&atom.vbp, inst)) < 0)
			    return sts;
			break;
		    case 139:	/* event.highres_records */
			if ((sts = sample_fetch_highres_events(&atom.vbp, inst)) < 0)
			    return sts;
			break;
		    case 140:	/* event.reset_highres */
			atom.l = event_get_highres_fetch_count();
			break;
		    case 1000:	/* secret.bar */
			atom.cp = "foo";
			break;
		    case 1001:	/* secret.foo.one */
			atom.l = 1;
			break;
		    case 1002:	/* secret.foo.two */
			atom.l = 2;
			break;
		    case 1003:	/* secret.foo.bar.three */
			atom.l = 3;
			break;
		    case 1004:	/* secret.foo.bar.four */
			atom.l = 4;
			break;
		    case 1005:	/* secret.foo.bar.grunt.five */
			atom.l = 5;
			break;
		    case 1006:	/* secret.foo.bar.grunt.snort.six */
			atom.l = 6;
			break;
		    case 1007:	/* secret.foo.bar.grunt.snort.seven */
			atom.l = 7;
			break;
		    case 1023: /* bigid */
			atom.l = 4194303;
			break;
		}
	    }
	    if ((sts = __pmStuffValue(&atom, &vset->vlist[j], type)) < 0) {
		__pmFreeResultValues(res);
		return sts;
	    }
	    vset->valfmt = sts;
	    j++;	/* next element in vlist[] for next instance */

skip:
	    ;
	} while (dp->indom != PM_INDOM_NULL && nextinst(&inst));
done:
	vset->numval = j;
    }
    *resp = res;
    return PMDA_FETCH_STATIC;
}

static int
sample_desc(pmID pmid, pmDesc *desc, pmdaExt *ep)
{
    int		i;
    __pmID_int	*pmidp = (__pmID_int *)&pmid;

    sample_inc_recv(ep->e_context);
    sample_inc_xmit(ep->e_context);

    if (not_ready > 0) {
	return limbo();
    }

    if (direct_map) {
	i = pmidp->item;
	if (i < ndesc && desctab[i].pmid == pmid)
	    goto doit;
    }
    for (i = 0; desctab[i].pmid != PM_ID_NULL; i++) {
	if (desctab[i].pmid == pmid) {
doit:
	    /* the special cases */
	    if (pmidp->item == 54)
		return PM_ERR_PMID;
	    else if (pmidp->item == 75 && _error_code < 0)
		/* error_check and error_code armed */
		return _error_code;
	    else if (pmidp->item == 86)
		*desc = magic;
	    else
		*desc = desctab[i];
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

static int
sample_text(int ident, int type, char **buffer, pmdaExt *ep)
{
    int sts;

    sample_inc_recv(ep->e_context);
    sample_inc_xmit(ep->e_context);

    if (not_ready > 0) {
	return limbo();
    }

    if (ident & PM_TEXT_PMID) {
	__pmID_int	*pmidp = (__pmID_int *)&ident;
	int		i;

	if (direct_map) {
	    i = pmidp->item;
	    if (i < ndesc && desctab[i].pmid == (pmID)ident)
		goto doit;
	}
	for (i = 0; desctab[i].pmid != PM_ID_NULL; i++) {
	    if (desctab[i].pmid == (pmID)ident) {
doit:
		/* the special cases */
		if (pmidp->item == 75 && _error_code < 0)
		    /* error_check and error_code armed */
		    return _error_code;
		break;
	    }
	}
    }

    sts = pmdaText(ident, type, buffer, ep);

    return sts;
}

static int
sample_store(pmResult *result, pmdaExt *ep)
{
    int		i;
    pmValueSet	*vsp;
    pmDesc	*dp;
    __pmID_int	*pmidp;
    int		sts = 0;
    __int32_t	*lp;
    pmAtomValue	av;

    sample_inc_recv(ep->e_context);
    sample_inc_xmit(ep->e_context);

    if (not_ready > 0) {
	return limbo();
    }

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	for (dp = desctab; dp->pmid != PM_ID_NULL; dp++) {
	    if (dp->pmid == vsp->pmid)
		break;
	}
	if (dp->pmid == PM_ID_NULL) {
	    /* not one of our metrics */
	    sts = PM_ERR_PMID;
	    break;
	}
	pmidp = (__pmID_int *)&vsp->pmid;

	if (pmidp->cluster != 0) {
	    sts = PM_ERR_PMID;
	    break;
	}

	/*
	 * for this PMD, the metrics that support modification
	 * via pmStore() mostly demand a single value, encoded in
	 * the result structure as PM_VAL_INSITU format for 32-bit
	 * ints, else a single value, encoded in the result structure
	 * as NOT PM_VAL_INSITU for 64-bit ints.
	 */
	switch (pmidp->item) {

	    case 0:	/* control */
	    case 7:	/* drift */
	    case 8:	/* step */
	    case 14:	/* long.write_me */
	    case 36:	/* write_me or dupnames.two.write_me or dupnames.three.write_me */
	    case 41:	/* recv_pdu */
	    case 42:	/* xmit_pdu */
	    case 56:	/* not_ready */
	    case 61:	/* dodgey.control */
	    case 72:    /* const_rate.value */
	    case 73:    /* const_rate.gradient */
	    case 74:    /* error_code */
	    case 79:    /* many.count */
	    case 87:	/* dynamic.meta.pmdesc.type */
	    case 88:	/* dynamic.meta.pmdesc.indom */
	    case 89:	/* dynamic.meta.pmdesc.sem */
	    case 90:	/* dynamic.meta.pmdesc.units */
	    case 97:	/* ulong.write_me */
	    case 126:	/* event.reset */
	    case 140:	/* event.reset_highres */
		if (vsp->numval != 1 || vsp->valfmt != PM_VAL_INSITU)
		    sts = PM_ERR_BADSTORE;
		break;

	    case 24:	/* longlong.write_me */
	    case 29:	/* double.write_me */
	    case 32:	/* string.write_me */
	    case 35:	/* aggregate.write_me */
	    case 102:	/* ulonglong.write_me */
	    case 120:	/* scramble.ver */
		if (vsp->numval != 1 || vsp->valfmt == PM_VAL_INSITU)
		    sts = PM_ERR_BADSTORE;
		break;

	    case 5:	/* colour */
	    case 37:	/* mirage */
		/*
		 * number of values/instances does not matter ... reset
		 * counter _x for a pmStore on _any_ instance
		 */
		if (vsp->valfmt != PM_VAL_INSITU)
		    sts = PM_ERR_BADSTORE;
		break;
	    case 38:	/* mirage_longlong */
		/*
		 * number of values/instances does not matter ... reset
		 * counter _x for a pmStore on _any_ instance
		 */
		if (vsp->valfmt == PM_VAL_INSITU)
		    sts = PM_ERR_BADSTORE;
		break;

	    case 19:	/* float.write_me */
		if (vsp->numval != 1)
		    sts = PM_ERR_BADSTORE;
		/* accommodate both old and new encoding styles for floats */
		break;

	    case 40:	/* pdu */
		/* value is ignored, so valfmt does not matter */
		if (vsp->numval != 1)
		    sts = PM_ERR_BADSTORE;
		break;

	    default:
		sts = PM_ERR_PERMISSION;
		break;

	}
	if (sts != 0)
	    break;

	if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0], dp->type, &av, dp->type)) < 0)
	    break;

	/*
	 * we only have cluster 0, metric already found in desctab[],
	 * so no checking needed nor outer case on pmidp->cluster
	 */
	switch (pmidp->item) {
	    case 0:	/* control */
		_control = av.l;
		switch (_control) {
		    case -1:
			/* terminate, if we are not a DSO implementation */
			sample_done = 1;
			break;
		    default:
			pmDebug = _control;
			break;
		}
		break;
	    /*
	     * note: all 3 metrics below share the same underlying
	     * global counter, _x
	     */
	    case 5:	/* colour */
	    case 37:	/* mirage */
		_x = av.l;
		break;
	    case 38:	/* mirage_longlong */
		_x = av.ll;
		break;
	    case 7:	/* drift */
		_drift = av.l;
		break;
	    case 8:	/* step */
		_step = av.l;
		break;
	    case 14:	/* long.write_me */
		_long = av.l;
		break;
	    case 24:	/* longlong.write_me */
		_longlong = av.ll;
		break;
	    case 19:	/* float.write_me */
		_float = av.f;
		break;
	    case 40:	/* pdu */
		/*
		 * for the pdu group, the value is ignored, and the only
		 * operation is to reset the counter(s)
		 */
		sample_clr_recv(CTX_ALL);
		sample_clr_xmit(CTX_ALL);
		break;
	    case 41:
		sample_clr_recv(CTX_ALL);
		break;
	    case 42:
		sample_clr_xmit(CTX_ALL);
		break;
	    case 36:
		_write_me = av.l;
		break;
	    case 29:	/* double.write_me */
		_double = av.d;
		break;
	    case 32:	/* string.write_me */
		free(_string);
		_string = av.cp;
		break;
	    case 35:	/* aggregate.write_me */
		free(_aggr35);
		_aggr35 = av.vbp;
		break;
	    case 56:	/* not_ready */
		not_ready = av.l;
		break;
	    case 61:	/* dodgey.control */
		dodgey = av.l;
		redo_dodgey();
		break;
	    case 72:	/* const_rate.value */
		const_rate_value = av.ul;
		break;
	    case 73:	/* const_rate.gradient */
		const_rate_gradient = av.ul;
		break;
	    case 74:	/* error_code */
		_error_code = av.l;
		break;
	    case 79:	/* many.count */
		many_count = av.l;
		/* change the size of the many instance domain */
		_error_code = redo_many();
		break;
	    case 87:	/* dynamic.meta.pmdesc.type */
		magic.type = av.l;
		break;
	    case 88:	/* dynamic.meta.pmdesc.indom */
		magic.indom = av.l;
		break;
	    case 89:	/* dynamic.meta.pmdesc.sem */
		magic.sem = av.l;
		break;
	    case 90:	/* dynamic.meta.pmdesc.units */
		lp = (__int32_t *)&magic.units;
		*lp = av.l;
		break;
	    case 97:	/* ulong.write_me */
		_ulong = av.ul;
		break;
	    case 102:	/* ulonglong.write_me */
		_ulonglong = av.ull;
		break;
	    case 120:	/* scramble.version */
		scramble_ver = 0;
		for (i = 0; i < indomtab[BIN_INDOM].it_numinst; i++) {
		    indomtab[SCRAMBLE_INDOM].it_set[i].i_inst = indomtab[BIN_INDOM].it_set[i].i_inst;
		    indomtab[SCRAMBLE_INDOM].it_set[i].i_name = indomtab[BIN_INDOM].it_set[i].i_name;
		}
		indomtab[SCRAMBLE_INDOM].it_numinst = indomtab[BIN_INDOM].it_numinst;
		break;
	    case 126:	/* event.reset */
		event_set_fetch_count(av.l);
		break;
	    case 140:	/* event.reset_highres */
		event_set_highres_fetch_count(av.l);
		break;
	    default:
		sts = PM_ERR_PERMISSION;
		break;
	}
    }

    return sts;
}

void
__PMDA_INIT_CALL
sample_init(pmdaInterface *dp)
{
    char	helppath[MAXPATHLEN];
    int		i;

    if (_isDSO) {
	int sep = __pmPathSeparator();
	snprintf(helppath, sizeof(helppath), "%s%c" "sample" "%c" "dsohelp",
			pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_LATEST, "sample DSO", helppath);
    }
    else {
	__pmProcessDataSize(NULL);
    }

    if (dp->status != 0)
	return;
    dp->comm.flags |= PDU_FLAG_AUTH;

    dp->version.any.fetch = sample_fetch;
    dp->version.any.desc = sample_desc;
    dp->version.any.instance = sample_instance;
    dp->version.any.text = sample_text;
    dp->version.any.store = sample_store;
    dp->version.any.profile = sample_profile;
    dp->version.four.pmid = sample_pmid;
    dp->version.four.name = sample_name;
    dp->version.four.children = sample_children;
    dp->version.six.attribute = sample_attribute;
    pmdaSetEndContextCallBack(dp, sample_ctx_end);

    pmdaInit(dp, NULL, 0, NULL, 0);	/* don't use indomtab or metrictab */

    __pmtimevalNow(&_then);
    _start = time(NULL);
    init_tables(dp->domain);
    init_events(dp->domain);
    redo_mirage();
    redo_dynamic();

    /* initialization of domain in PMIDs for dynamic PMNS entries */
    for (i = 0; i < numdyn; i++) {
	((__pmID_int *)&dynamic_ones[i].pmid)->domain = dp->domain;
    }
    /*
     * Max Matveev wanted this sort of redirection, so first entry is
     * actually a redirect to PMID 2.4.1 (pmcd.agent.status)
     */
    ((__pmID_int *)&dynamic_ones[0].pmid)->domain = 2;
    ((__pmID_int *)&dynamic_ones[0].pmid)->cluster = 4;
    ((__pmID_int *)&dynamic_ones[0].pmid)->item = 1;

    /*
     * for gcc/egcs, statically initializing these cased the strings
     * to be read-only, causing SEGV in redo_dynamic ... so do the
     * initialization dynamically here.
     */
    _dodgey[0].i_name = strdup("d1");
    _dodgey[1].i_name = strdup("d2");
    _dodgey[2].i_name = strdup("d3");
    _dodgey[3].i_name = strdup("d4");
    _dodgey[4].i_name = strdup("d5");
}
