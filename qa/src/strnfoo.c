/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2025 Ken McDonell.  All Rights Reserved.
 *
 * Exerciser and difference highlighter for strncpy(), pmstrncpy(), strncat()
 * and pmstrncat();
 */

#include <pcp/pmapi.h>
#include <ctype.h>
#include <string.h>

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,	/* -D */
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "[options] length string"
};

#define DST_SIZE 50

void
dump(char *dst, size_t dlen, char *src, size_t slen)
{
    int		i;
    int		first_null = -1;
    int		last_null = -1;
    int		first_unfill = -1;
    int		last_unfill = -1;
    int		first_overrun = -1;
    int		last_overrun = -1;
    char	*p;

    printf("src=\"%s\" [%ld]\n", src, (long)slen);
    printf("dst");
    if ((p = strstr(dst, src)) != NULL) {
	if (strlen(src) == 1)
	    printf(": src match [%d]", (int)(p - dst));
	else
	    printf(": src match [%d]...[%d]", (int)(p - dst), (int)((p - dst)+strlen(src)-1));
    }
    else
	printf(": src not found dst=\"%s\"", dst);

    for (i = 0; i < dlen-1; i++) {
	if (i < DST_SIZE && dst[i] == '\0') {
	    if (first_null == -1) {
		first_null = last_null = i;
	    }
	    else
		last_null = i;
	}
	else if (i < DST_SIZE && dst[i] == '\1') {
	    if (first_unfill == -1) {
		first_unfill = last_unfill = i;
	    }
	    else
		last_unfill = i;
	}
	else if (i >= DST_SIZE && dst[i] != '\2') {
	    if (first_overrun == -1) {
		first_overrun = last_overrun = i;
	    }
	    else
		last_overrun = i;
	}
    }

    if (first_null != -1) {
	printf(": null [%d]", first_null);
	if (last_null > first_null)
	    printf("...[%d]", last_null);
    }
    else
	printf(": no null");

    if (first_unfill != -1) {
	printf(": unfill [%d]", first_unfill);
	if (last_unfill > first_unfill)
	    printf("...[%d]", last_unfill);
    }
    else
	printf(": no unfill");

    if (first_overrun != -1) {
	int	j;
	printf(": overrun [%d]", first_overrun);
	if (last_overrun > first_overrun)
	    printf("...[%d]", last_overrun);
	printf(" \"");
	for (j = first_overrun; j <= last_overrun; j++) {
	    if (isprint(dst[j]))
		putchar(dst[j]);
	    else
		printf("\\%3.3o", dst[j]);
	}
	putchar('"');
    }

    putchar('\n');
}

int
main(int argc, char **argv)
{
    int		c;
    char	*src;
    int		sts;
    int		lentocat;
    char	*end;
    char	*p;
    size_t	len;
    char	dst[DST_SIZE+11];

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.errors || opts.optind != argc - 2) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    len = strtol(argv[opts.optind], &end, 10);
    if (*end != '\0') {
	fprintf(stderr, "Botch: length (%s) not numeric\n", argv[opts.optind]);
	exit(1);
    }
    if (len <= 0 || len > 50) {
	fprintf(stderr, "Botch: length (%ld) illegal (must be > 0 && <= 50)\n", (long)len);
	exit(1);
    }
    src = argv[opts.optind+1];

    printf("strncpy() ...\n");
    memset(dst, '\01', DST_SIZE);
    memset(&dst[DST_SIZE], '\02', 10);
    dst[DST_SIZE+10] = '\0';
    p = strncpy(dst, src, len);
    if (p != dst)
	fprintf(stderr, "Botch: strncpy() returns %p not %p\n", p, dst);
    dump(dst, sizeof(dst), src, len);

    printf("pmstrncpy() ...\n");
    memset(dst, '\01', DST_SIZE);
    memset(&dst[DST_SIZE], '\02', 10);
    dst[DST_SIZE+10] = '\0';
    sts = pmstrncpy(dst, len, src);
    if (sts != 0)
	printf("pmstrncpy() truncate\n");
    dump(dst, sizeof(dst), src, len);

    printf("strncat() ...\n");
    memset(dst, '\01', DST_SIZE);
    memset(&dst[DST_SIZE], '\02', 10);
    dst[DST_SIZE+10] = '\0';
    /* 35 bytes including the null */
    memcpy(dst, "mary had a little lamb, its fleece was white as snow", 34);
    dst[34] = '\0';
    if (strlen(dst) + len > DST_SIZE)
	lentocat = DST_SIZE - strlen(dst);
    else
	lentocat = len;
    p = strncat(dst, src, lentocat);
    if (p != dst)
	fprintf(stderr, "Botch: strncat() returns %p not %p\n", p, dst);
    dump(dst, sizeof(dst), src, len);

    printf("pmstrncat() ...\n");
    memset(dst, '\01', DST_SIZE);
    memset(&dst[DST_SIZE], '\02', 10);
    dst[DST_SIZE+10] = '\0';
    /* 35 bytes including the null */
    memcpy(dst, "mary had a little lamb, its fleece was white as snow", 34);
    dst[34] = '\0';
    sts = pmstrncat(dst, DST_SIZE, src);
    if (sts != 0)
	printf("pmstrncat() truncate\n");
    dump(dst, sizeof(dst), src, len);

    return 0;
}
