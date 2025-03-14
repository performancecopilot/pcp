/*
 * Copyright (c) 2025 Red Hat.
 *
 * Convert external sha1 (40 byte) to internal (20 byte) form.
 * Example usages:
 *     $ ./sha1ext2int 09be7733f1b5ed42572d26928a5e56ccf91ea8b8
 *     "\t\xbew3\xf1\xb5\xedBW-&\x92\x8a^V\xcc\xf9\x1e\xa8\xb8"
 *     $ ./sha1ext2int --raw 09be7733f1b5ed42572d26928a5e56ccf91ea8b8
 */
#include <pcp/pmapi.h>
#include "sds.h"

/* Input 40-byte SHA1 hash, output 20-byte representation */
static unsigned char *
decode_identity(const char *buffer, unsigned char *hash)
{
    int off;
    char cur;
    unsigned char val;

    for (off = 0; off < 40; off++) {
	/* Convert ASCII hex into binary */
	cur = buffer[off];
	if (cur >= 97)
	    val = cur - 97 + 10;
	else if (cur >= 65)
	    val = cur - 65 + 10;
	else
	    val = cur - 48;

	/* Even chars are first half of output byte, odd the second half */
	if (off % 2 == 0)
	    hash[off / 2] = val << 4;
        else
            hash[off / 2] |= val;
    }

    return hash;
}

static int
printsha1(int ix, int raw, char *external)
{
    unsigned char buffer[20] = {0};

    if (strlen(external) != 40) {
	fprintf(stderr, "[%d] bad argument - length is %d, not 40 (%s)\n",
			ix, (int)strlen(external), external);
	return -1;
    }
    decode_identity((const char *)external, buffer);
    if (!raw) {
	sds s = sdscatrepr(sdsempty(), (const char *)buffer, sizeof(buffer));
	printf("%s\n", s);
	sdsfree(s);
    } else {
	printf("%s\n", buffer);
    }
    return 0;
}

int
main(int argc, char *argv[])
{
    char *string;
    int exitsts = 0;
    int raw = 0;

    while (optind < argc) {
	string = argv[optind];
	if (strcmp(string, "--raw") == 0)
	    raw = 1;
	else if (printsha1(optind, raw, string) < 0)
	    exitsts = 1;
	optind++;
    }
    return exitsts;
}
