/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

#include "localconfig.h"

int
main()
{
    int			*ip;
    int			sts = 0;
    pmUnits		units;
    pmValueBlock	vb;
    __pmID_int		pmid;
    __pmInDom_int	indom;
    __pmPDUInfo		pduinfo;
    __pmCred		cred;

    printf("pmUnits: ");
    ip = (int *)&units;
    *ip = 0;
    units.dimSpace = 0x1;
    units.dimTime = 0x2;
    units.dimCount = 0x3;
    units.scaleSpace = 0x4;
    units.scaleTime = 0x5;
    units.scaleCount = 0x6;
    units.pad = 0x78;

    if (*ip == 0x12345678)
	printf("OK\n");
    else {
	printf("FAIL get 0x%x, expected 0x12345678\n", *ip);
	sts = 1;
    }

    printf("pmValueBlock (header): ");
    ip = (int *)&vb;
    *ip = 0;
    vb.vtype = 0x12;
    vb.vlen = 0x345678;

    if (*ip == 0x12345678)
	printf("OK\n");
    else {
	printf("FAIL get 0x%x, expected 0x12345678\n", *ip);
	sts = 1;
    }

    printf("__pmCred: ");
    ip = (int *)&cred;
    *ip = 0;
    cred.c_type = 0x12;
    cred.c_vala = 0x34;
    cred.c_valb = 0x56;
    cred.c_valc = 0x78;

    if (*ip == 0x12345678)
	printf("OK\n");
    else {
	printf("FAIL get 0x%x, expected 0x12345678\n", *ip);
	sts = 1;
    }

    printf("__pmID_int: ");
    ip = (int *)&pmid;
    *ip = 0;
    pmid.flag = 0x1;
    pmid.domain = (0x123 >> 2) & 0x1ff;
    pmid.cluster = (0x3456 >> 2) & 0xfff;
    pmid.item = 0x678 & 0x3ff;

    if (*ip == 0x92345678)
	printf("OK\n");
    else {
	printf("FAIL get 0x%x, expected 0x92345678\n", *ip);
	sts = 1;
    }

    printf("__pmInDom_int: ");
    ip = (int *)&indom;
    *ip = 0;
    indom.flag = 0x1;
    indom.domain = (0x123 >> 2) & 0x1ff;
    indom.serial = 0x345678 & 0x3fffff;

    if (*ip == 0x92345678)
	printf("OK\n");
    else {
	printf("FAIL get 0x%x, expected 0x92345678\n", *ip);
	sts = 1;
    }

    printf("__pmPDUInfo: ");
    ip = (int *)&pduinfo;
    *ip = 0;
    pduinfo.zero = 0x1;
    pduinfo.version = 0x12 & 0x7f;
    pduinfo.licensed = 0x34;
#if PCP_VER >= 3611
    pduinfo.features = 0x5678;
#else
    pduinfo.authorize = 0x5678;
#endif

    if (*ip == 0x92345678)
	printf("OK\n");
    else {
	printf("FAIL get 0x%x, expected 0x92345678\n", *ip);
	sts = 1;
    }

#ifdef TODO
    /*
     * write tests
     */
    pmUnits		units;
    pmValueBlock	vb;
    __pmID_int		pmid;
    __pmInDom_int	indom;
    __pmPDUInfo		pduinfo;
    __pmCred		cred;
#endif

    exit(sts);
}
