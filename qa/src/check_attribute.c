/*
 * Exercise libpcp __pmCheckAttribute routine.
 *
 * Copyright (c) 2024 Red Hat.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

static int
mapattr(const char *name)
{
    static struct attrmap {
	__pmAttrKey	key;
	const char	*name;
    } maptable[] = {
	{ PCP_ATTR_PROTOCOL, "protocol" },
	{ PCP_ATTR_SECURE, "secure" },
	{ PCP_ATTR_COMPRESS, "compress" },
	{ PCP_ATTR_USERAUTH, "userauth" },
	{ PCP_ATTR_USERNAME, "username" },
	{ PCP_ATTR_AUTHNAME, "authname" },
	{ PCP_ATTR_PASSWORD, "password" },
	{ PCP_ATTR_METHOD, "method" },
	{ PCP_ATTR_REALM, "realm" },
	{ PCP_ATTR_UNIXSOCK, "unixsock" },
	{ PCP_ATTR_USERID, "userid" },
	{ PCP_ATTR_GROUPID, "groupid" },
	{ PCP_ATTR_LOCAL, "local" },
	{ PCP_ATTR_PROCESSID, "processid" },
	{ PCP_ATTR_CONTAINER, "container" },
	{ PCP_ATTR_EXCLUSIVE, "exclusive" },
	/* must be final entry: zero/none */
	{ PCP_ATTR_NONE, "" },
    };
    struct attrmap *p;
    char id[64];

    for (p = &maptable[0]; p->key != PCP_ATTR_NONE; p++) {
	pmsprintf(id, sizeof(id), "%s", p->name);
	if (strcasecmp(id, name) == 0)
	    return p->key;
	pmsprintf(id, sizeof(id), "ATTR_%s", p->name);
	if (strcasecmp(id, name) == 0)
	    return p->key;
	pmsprintf(id, sizeof(id), "PCP_ATTR_%s", p->name);
	if (strcasecmp(id, name) == 0)
	    return p->key;
    }
    return -ENOENT;
}

static void
check(int sts, const char *name, const char *value)
{
    if (sts < 0) {
	fprintf(stderr, "%s: \"%s\" error: %s\n", name, value, pmErrStr(sts));
    } else {
	fprintf(stderr, "%s: \"%s\" is OK", name, value);
	if (sts != 0) fprintf(stderr, " ->%d", sts);
	fputc('\n', stderr);
    }
}

int
main(int argc, char **argv)
{
    int		c, sts, attr = -1, errflag = 0;
    char	*name, *value;
    static char	*usage = "[-D debugspec] name value";

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
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

    if (argc != 3)
	errflag++;

    if (errflag) {
	fprintf(stderr, "Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    name = argv[1];
    value = argv[2];

    if ((attr = mapattr(name)) < 0) {
	fprintf(stderr, "%s: \"%s\" is unknown\n", pmGetProgname(), name);
	exit(1);
    }

    sts = __pmCheckAttribute(attr, value);
    check(sts, name, value);

    exit(0);
}
