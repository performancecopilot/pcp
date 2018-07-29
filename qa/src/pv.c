/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <stdio.h>
#include <string.h>
#include <pcp/pmapi.h>

/*
 * exercise pmPrintValue() ... should produce very similar answers
 * when compiled both 32 and 64 bit ... see pv and pv64 targets in
 * the Makefile
 */

int
main(void)
{
    pmValue		v;
    int			l;
    unsigned int	ul;
    __int64_t		ll;
    __uint64_t		ull;
    float		f;
    float		*fp;
    double		d;
    struct {
	int	len;
	char	vbuf[sizeof(double)];
    } vb;
    char		*fmt_int64x;
    char		*p;

    v.inst = 1;

    l = -1 & (~0xffff);
    memcpy((void *)&v.value.lval, (void *)&l, sizeof(l));
    printf("PM_TYPE_32: ");
    pmPrintValue(stdout, PM_VAL_INSITU,  PM_TYPE_32, &v, 1);
    printf(" correct: %i (0x%x)\n", l, l);

    ul = 0x87654321;
    memcpy((void *)&v.value.lval, (void *)&ul, sizeof(ul));
    printf("PM_TYPE_U32: ");
    pmPrintValue(stdout, PM_VAL_INSITU,  PM_TYPE_U32, &v, 1);
    printf(" correct: %u (0x%x)\n", ul, ul);

    f = 123456.78;
    fp = (float *)&v.value.lval;
    *fp = f;
    printf("PM_TYPE_FLOAT: ");
    pmPrintValue(stdout, PM_VAL_INSITU,  PM_TYPE_FLOAT, &v, 9);
    printf(" correct: %.2f\n", (double)f);

    d = 123456.789012345;
    memcpy((void *)vb.vbuf, (void *)&d, sizeof(double));
    vb.len = sizeof(vb.len) + sizeof(double);
    v.value.pval = (pmValueBlock *)&vb;
    printf("PM_TYPE_DOUBLE: ");
    pmPrintValue(stdout, PM_VAL_SPTR,  PM_TYPE_DOUBLE, &v, 16);
    printf(" correct: %.9f\n", d);

    fmt_int64x = strdup("%" FMT_INT64);
    for (p = fmt_int64x; *p; p++)
	;
    p--;
    if (*p != 'd') {
	fprintf(stderr, "Botch: expect FMT_INT64 (%s) to end in 'd'\n", FMT_INT64);
	exit (1);
    }
    *p = 'x';

    ll = -1 & (~0xffff);
    memcpy((void *)vb.vbuf, (void *)&ll, sizeof(long long));
    vb.len = sizeof(vb.len) + sizeof(long long);
    v.value.pval = (pmValueBlock *)&vb;
    printf("PM_TYPE_64: ");
    pmPrintValue(stdout, PM_VAL_SPTR,  PM_TYPE_64,  &v, 1);
    printf(" correct: %" FMT_INT64 " (0x", ll);
    printf(fmt_int64x, ll);
    printf(")\n");

    ull = 0x8765432112340000LL;
    memcpy((void *)vb.vbuf, (void *)&ull, sizeof(unsigned long long));
    vb.len = sizeof(vb.len) + sizeof(unsigned long long);
    v.value.pval = (pmValueBlock *)&vb;
    printf("PM_TYPE_U64: ");
    pmPrintValue(stdout, PM_VAL_SPTR,  PM_TYPE_U64,  &v, 1);
    printf(" correct: %" FMT_UINT64 " (0x", ull);
    printf(fmt_int64x, ull);
    printf(")\n");

    exit(0);
}
