/*
 * Copyright (c) 2018 Red Hat.
 *
 * Convert internal sha1 (20 byte form, sds-repr-encoded) into external form.
 * Example usage:
 *     $ ./sha1int "\t\xbew3\xf1\xb5\xedBW-&\x92\x8a^V\xcc\xf9\x1e\xa8\xb8"
 *     09be7733f1b5ed42572d26928a5e56ccf91ea8b8
 */
#include <pcp/pmapi.h>
#include "sds.h"

static char *
hash_identity(const unsigned char *hash, char *buffer, int buflen)
{
    int         nbytes, off;

    /* Input 20-byte SHA1 hash, output 40-byte representation */
    for (nbytes = off = 0; nbytes < 20; nbytes++)
        off += pmsprintf(buffer + off, buflen - off, "%02x", hash[nbytes]);
    buffer[buflen-1] = '\0';
    return buffer;
}

static int
printsha1(int ix, sds internal)
{
    char	buffer[40+1];

    if (sdslen(internal) != 20) {
	fprintf(stderr, "[%d] bad argument - length is %d, not 20 (%s)\n",
			ix, (int)sdslen(internal), internal);
	return -1;
    }
    hash_identity((const unsigned char *)internal, buffer, sizeof(buffer));
    printf("%s\n", buffer);
    return 0;
}

int
main(int argc, char *argv[])
{
    sds		*result;
    char	buffer[256] = {0};
    int		count, i, exitsts = 0;

    while (optind < argc) {
	pmsprintf(buffer, sizeof(buffer), "\"%s\"", argv[optind++]);
	result = sdssplitargs(buffer, &count);
	for (i = 0; i < count; i++)
	    if (printsha1(i, result[i]) < 0)
		exitsts = 1;
	sdsfreesplitres(result, count);
    }
    return exitsts;
}
