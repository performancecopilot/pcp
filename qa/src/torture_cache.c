/*
 * Copyright (C) 2013-2014 Red Hat.
 * Copyright (c) 2005 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

static pmInDom		indom;
static __pmInDom_int	*indomp;
static char		*xxx = "xxxsomefunnyinstancenamestringthatcanbechoppedabout";
static char		nbuf[80];	/* at least as big as xxx[] */
static	int		ncount;

static void
_a(int load, int verbose, int extra)
{
    int		inst;
    int		sts;

    indomp->domain = 123;
    indomp->serial = 7;

    if (load) {
	fprintf(stderr, "Load the instance domain ...\n");
	sts = pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	if (sts < 0) {
	    fprintf(stderr, "PMDA_CACHE_LOAD failed: %s\n", pmErrStr(sts));
	    return;
	}
    }

    fprintf(stderr, "Add foo ...\n");
    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, "foo", NULL);
    fprintf(stderr, "return -> %d", inst);
    if (inst < 0) fprintf(stderr, ": %s", pmErrStr(inst));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDumpAll(stderr, 0);

    fprintf(stderr, "\nAdd bar ...\n");
    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, "bar", (void *)((__psint_t)0xdeadbeef));
    fprintf(stderr, "return -> %d", inst);
    if (inst < 0) fprintf(stderr, ": %s", pmErrStr(inst));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDumpAll(stderr, 0);

    fprintf(stderr, "\nAdd java coffee beans ...\n");
    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, "java coffee beans", (void *)((__psint_t)0xcafecafe));
    fprintf(stderr, "return -> %d", inst);
    if (inst < 0) fprintf(stderr, ": %s", pmErrStr(inst));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDumpAll(stderr, 0);

    fprintf(stderr, "\nAdd another one ...\n");
    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, "another one", NULL);
    fprintf(stderr, "return -> %d", inst);
    if (inst < 0) fprintf(stderr, ": %s", pmErrStr(inst));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDumpAll(stderr, 0);

    fprintf(stderr, "\nHide another one ...\n");
    inst = pmdaCacheStore(indom, PMDA_CACHE_HIDE, "another one", NULL);
    fprintf(stderr, "return -> %d", inst);
    if (inst < 0) fprintf(stderr, ": %s", pmErrStr(inst));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDump(stderr, indom, 0);

    fprintf(stderr, "\nCull foo ...\n");
    inst = pmdaCacheStore(indom, PMDA_CACHE_CULL, "foo", NULL);
    fprintf(stderr, "return -> %d", inst);
    if (inst < 0) fprintf(stderr, ": %s", pmErrStr(inst));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDump(stderr, indom, 0);

    fprintf(stderr, "\nCull foo again, should fail ...\n");
    inst = pmdaCacheStore(indom, PMDA_CACHE_CULL, "foo", NULL);
    fprintf(stderr, "return -> %d", inst);
    if (inst < 0) fprintf(stderr, ": %s", pmErrStr(inst));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDump(stderr, indom, 0);

    fprintf(stderr, "\nCount instances ...\n");
    sts = pmdaCacheOp(indom, PMDA_CACHE_SIZE);
    fprintf(stderr, "entries: %d\n", sts);
    sts = pmdaCacheOp(indom, PMDA_CACHE_SIZE_ACTIVE);
    fprintf(stderr, "active entries: %d\n", sts);
    sts = pmdaCacheOp(indom, PMDA_CACHE_SIZE_INACTIVE);
    fprintf(stderr, "inactive entries: %d\n", sts);

    fprintf(stderr, "\nProbe bar ...\n");
    sts = pmdaCacheLookupName(indom, "bar", &inst, NULL);
    fprintf(stderr, "return ->");
    if (sts >= 0) fprintf(stderr, " %d", inst);
    if (sts == PMDA_CACHE_INACTIVE) fprintf(stderr, " [inactive]");
    if (sts < 0) fprintf(stderr, " %d: %s", sts, pmErrStr(sts));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDump(stderr, indom, 0);

    fprintf(stderr, "\nProbe another one (hidden) ...\n");
    sts = pmdaCacheLookupName(indom, "another one", &inst, NULL);
    fprintf(stderr, "return ->");
    if (sts >= 0) fprintf(stderr, " %d", inst);
    if (sts == PMDA_CACHE_INACTIVE) fprintf(stderr, " [inactive]");
    if (sts < 0) fprintf(stderr, " %d: %s", sts, pmErrStr(sts));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDump(stderr, indom, 0);

    if (load) {
	sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
	if (sts < 0) {
	    fprintf(stderr, "PMDA_CACHE_SAVE failed: %s\n", pmErrStr(sts));
	}
	return;
    }

    if (extra == 0)
	return;

    indomp->serial = 8;
    fprintf(stderr, "\nAdd foo in another indom ...\n");
    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, "foo", NULL);
    fprintf(stderr, "return -> %d", inst);
    if (inst < 0) fprintf(stderr, ": %s", pmErrStr(inst));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDumpAll(stderr, 0);

    fprintf(stderr, "\nProbe bar (not in this indom) ...\n");
    sts = pmdaCacheLookupName(indom, "bar", &inst, NULL);
    fprintf(stderr, "return ->");
    if (sts >= 0) fprintf(stderr, " %d", inst);
    if (sts == PMDA_CACHE_INACTIVE) fprintf(stderr, " [inactive]");
    if (sts < 0) fprintf(stderr, " %d: %s", sts, pmErrStr(sts));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDump(stderr, indom, 0);


    indomp->serial = 7;

    fprintf(stderr, "\nMark all active ...\n");
    sts = pmdaCacheOp(indom, PMDA_CACHE_ACTIVE);
    fprintf(stderr, "return -> %d", sts);
    if (sts < 0) fprintf(stderr, ": %s", pmErrStr(sts));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDump(stderr, indom, 0);

    fprintf(stderr, "\nMark all inactive ...\n");
    sts = pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
    fprintf(stderr, "return -> %d", sts);
    if (sts < 0) fprintf(stderr, ": %s", pmErrStr(sts));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDump(stderr, indom, 0);

    fprintf(stderr, "\nCull all ...\n");
    sts = pmdaCacheOp(indom, PMDA_CACHE_CULL);
    fprintf(stderr, "return -> %d", sts);
    if (sts < 0) fprintf(stderr, ": %s", pmErrStr(sts));
    fputc('\n', stderr);
    if (verbose) __pmdaCacheDump(stderr, indom, 0);

}

static void
_b(void)
{
    int		i;
    int		j;
    int		inst;
    int		sts;
    char	cmd[2*MAXPATHLEN+30];

    indomp->domain = 123;
    indomp->serial = 8;

    sprintf(cmd, "rm -f %s/config/pmda/%s", pmGetConfig("PCP_VAR_DIR"), pmInDomStr(indom));
    sts = system(cmd);
    if (sts != 0)
	fprintf(stderr, "Warning: %s: exit status %d\n", cmd, sts);
    sprintf(cmd, "[ -f %s/config/pmda/%s ] || exit 0; cat %s/config/pmda/%s", pmGetConfig("PCP_VAR_DIR"), pmInDomStr(indom), pmGetConfig("PCP_VAR_DIR"), pmInDomStr(indom));

    fprintf(stderr, "\nPopulate the instance domain ...\n");
    j = 1;
    for (i = 0; i < 20; i++) {
	strncpy(nbuf, xxx, ncount+3);
	sprintf(nbuf, "%03d", ncount);
	ncount++;
        inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, nbuf, (void *)((__psint_t)(0xbeef0000+ncount)));
	if (inst < 0)
	    fprintf(stderr, "PMDA_CACHE_ADD failed for \"%s\": %s\n", nbuf, pmErrStr(inst));
	else if (i > 14) {
	    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
	    fprintf(stderr, "Save -> %d\n", sts);
	}
	if (i == j) {
	    j <<= 1;
	    inst = pmdaCacheStore(indom, PMDA_CACHE_HIDE, nbuf, NULL);
	    if (inst < 0)
		fprintf(stderr, "PMDA_CACHE_HIDE failed for \"%s\": %s\n", nbuf, pmErrStr(inst));
	}
	if (i == 6 || i == 13) {
	    fprintf(stderr, "Save ...\n");
	    fprintf(stderr, "Before:\n");
	    sts = system(cmd);
	    if (sts != 0)
		fprintf(stderr, "Warning: _b:1: %s: exit status %d\n", cmd, sts);
	    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
	    fprintf(stderr, "return -> %d", sts);
	    if (sts < 0) fprintf(stderr, ": %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    fprintf(stderr, "After:\n");
	    sts = system(cmd);
	    if (sts != 0)
		fprintf(stderr, "Warning: _b:2: %s: exit status %d\n", cmd, sts);
	}
	if (i == 14) {
	    fprintf(stderr, "Start save after changes ...\n");
	}
	if (i > 14) {
	    sts = system(cmd);
	    if (sts != 0)
		fprintf(stderr, "Warning: _b:3: %s: exit status %d\n", cmd, sts);
	}
    }
    __pmdaCacheDump(stderr, indom, 0);
    strncpy(nbuf, xxx, 11+3);
    sprintf(nbuf, "%03d", 11);
    fprintf(stderr, "\nHide %s ...\n", nbuf);
    inst = pmdaCacheStore(indom, PMDA_CACHE_HIDE, nbuf, NULL);
    if (inst < 0)
	fprintf(stderr, "PMDA_CACHE_HIDE failed for \"%s\": %s\n", nbuf, pmErrStr(inst));
    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    fprintf(stderr, "Save -> %d\n", sts);
    sts = system(cmd);
    if (sts != 0)
	fprintf(stderr, "Warning: _b:4: %s: exit status %d\n", cmd, sts);
    fprintf(stderr, "Add %s ...\n", nbuf);
    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, nbuf, (void *)((__psint_t)0xdeadbeef));
    if (inst < 0)
	fprintf(stderr, "PMDA_CACHE_ADD failed for \"%s\": %s\n", nbuf, pmErrStr(inst));
    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    fprintf(stderr, "Save -> %d\n", sts);
    sts = system(cmd);
    if (sts != 0)
	fprintf(stderr, "Warning: _b:5: %s: exit status %d\n", cmd, sts);
    fprintf(stderr, "Cull %s ...\n", nbuf);
    inst = pmdaCacheStore(indom, PMDA_CACHE_CULL, nbuf, NULL);
    if (inst < 0)
	fprintf(stderr, "PMDA_CACHE_CULL failed for \"%s\": %s\n", nbuf, pmErrStr(inst));
    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    fprintf(stderr, "Save -> %d\n", sts);
    sts = system(cmd);
    if (sts != 0)
	fprintf(stderr, "Warning: _b:6: %s: exit status %d\n", cmd, sts);
    fprintf(stderr, "Add %s ...\n", nbuf);
    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, nbuf, (void *)((__psint_t)0xdeadbeef));
    if (inst < 0)
	fprintf(stderr, "PMDA_CACHE_ADD failed for \"%s\": %s\n", nbuf, pmErrStr(inst));
    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    fprintf(stderr, "Save -> %d\n", sts);
    sts = system(cmd);
    if (sts != 0)
	fprintf(stderr, "Warning: _b:7: %s: exit status %d\n", cmd, sts);

}

static void
_c(void)
{
    int		inst;
    int		sts;

    indomp->domain = 123;
    indomp->serial = 13;

    fprintf(stderr, "Load the instance domain ...\n");
    sts = pmdaCacheOp(indom, PMDA_CACHE_LOAD);
    if (sts < 0) {
	fprintf(stderr, "PMDA_CACHE_LOAD failed: %s\n", pmErrStr(sts));
	return;
    }

    inst = pmdaCacheStore(indom, PMDA_CACHE_HIDE, "fubar-001", NULL);
    if (inst < 0)
	fprintf(stderr, "PMDA_CACHE_HIDE failed for \"fubar-001\": %s\n", pmErrStr(inst));
    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, "fubar-002", NULL);
    if (inst < 0)
	fprintf(stderr, "PMDA_CACHE_ADD failed for \"fubar-002\": %s\n", pmErrStr(inst));
    inst = pmdaCacheStore(indom, PMDA_CACHE_CULL, "fubar-003", NULL);
    if (inst < 0)
	fprintf(stderr, "PMDA_CACHE_CULL failed for \"fubar-003\": %s\n", pmErrStr(inst));
    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, "fubar-009", NULL);
    if (inst < 0)
	fprintf(stderr, "PMDA_CACHE_ADD failed for \"fubar-009\": %s\n", pmErrStr(inst));

    __pmdaCacheDump(stderr, indom, 0);

    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    if (sts < 0) {
	fprintf(stderr, "PMDA_CACHE_SAVE failed: %s\n", pmErrStr(sts));
	return;
    }
}

static void
_e(int since)
{
    int		i;
    int		j;
    int		sts;
    int		inst;

    indomp->domain = 123;
    indomp->serial = 11;

    sts = pmdaCacheOp(indom, PMDA_CACHE_LOAD);
    if (sts < 0) {
	fprintf(stderr, "PMDA_CACHE_LOAD failed: %s\n", pmErrStr(sts));
	return;
    }

    j = 1;
    for (i = 0; i < 10; i++) {
	sprintf(nbuf, "boring-instance-%03d", i);
        inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, nbuf, (void *)((__psint_t)(0xcaffe000+i)));
	if (inst < 0)
	    fprintf(stderr, "PMDA_CACHE_ADD failed for \"%s\": %s\n", nbuf, pmErrStr(inst));
	if (i == j) {
	    j <<= 1;
	    inst = pmdaCacheStore(indom, PMDA_CACHE_HIDE, nbuf, NULL);
	    if (inst < 0)
		fprintf(stderr, "PMDA_CACHE_HIDE failed for \"%s\": %s\n", nbuf, pmErrStr(inst));
	}
    }

    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    fprintf(stderr, "Save -> %d\n", sts);
    fprintf(stderr, "Before purge ...\n");
    __pmdaCacheDump(stderr, indom, 0);

    sleep(1);

    sts = pmdaCachePurge(indom, since);
    if (sts < 0) {
	fprintf(stderr, "pmdaCachePurge failed: %s\n", pmErrStr(sts));
	return;
    }
    fprintf(stderr, "Purged %d entries\nAfter purge ...\n", sts);
    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    fprintf(stderr, "Save -> %d\n", sts);
    __pmdaCacheDump(stderr, indom, 0);
}

static void
_g(void)
{
    int		inst;
    int		i;

    indomp->domain = 123;
    indomp->serial = 7;

    for (i = 0; i < 254; i++) {
	sprintf(nbuf, "hashing-instance-%03d", i);
        inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, nbuf, (void *)((__psint_t)(0xdeaf0000+i)));
	if (inst < 0)
	    fprintf(stderr, "PMDA_CACHE_ADD failed for \"%s\": %s\n", nbuf, pmErrStr(inst));
	if (i % 2 == 0) {
	    inst = pmdaCacheStore(indom, PMDA_CACHE_HIDE, nbuf, NULL);
	    if (inst < 0)
		fprintf(stderr, "PMDA_CACHE_HIDE failed for \"%s\": %s\n", nbuf, pmErrStr(inst));
	}
	if (i % 7 == 0) {
	    sprintf(nbuf, "hashing-instance-%03d", i);
	    inst = pmdaCacheStore(indom, PMDA_CACHE_CULL, nbuf, NULL);
	    if (inst < 0)
		fprintf(stderr, "PMDA_CACHE_CULL failed for \"%s\": %s\n", nbuf, pmErrStr(inst));
	}
    }

    __pmdaCacheDump(stderr, indom, 1);

    _a(0, 0, 0);

    __pmdaCacheDump(stderr, indom, 1);
}

static char *h_tab[] = { "foo", "foobar", "foo bar", NULL };

static void
_h(void)
{
    int		sts;
    int		inst;
    int		i;

    indomp->domain = 123;
    indomp->serial = 17;

    sts = pmdaCacheOp(indom, PMDA_CACHE_LOAD);
    if (sts < 0) {
	fprintf(stderr, "PMDA_CACHE_LOAD failed: %s\n", pmErrStr(sts));
	return;
    }

    for (i = 0; h_tab[i] != NULL; i++) {
	fprintf(stderr, "[%s]\n", h_tab[i]);
	inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, h_tab[i], NULL);
	if (inst < 0) {
	    fprintf(stderr, "ADD failed: %s\n", pmErrStr(inst));
	    continue;
	}
	fprintf(stderr, "ADD -> %d\n", inst);

	inst = pmdaCacheStore(indom, PMDA_CACHE_HIDE, h_tab[i], NULL);
	if (inst < 0) {
	    fprintf(stderr, "HIDE failed: %s\n", pmErrStr(inst));
	    continue;
	}
	fprintf(stderr, "HIDE -> %d\n", inst);

    }
}

static char *i_tab[] = { "ernie", "bert", "kermit", "oscar", "big bird", "miss piggy", NULL };

/*
 * revised dirty cache semantics
 * style & 1 => SAVE
 * style & 2 => SYNC
 */
static void
_i(int style)
{
    int		sts;
    int		inst;
    int		i;
    static int	first = 1;

    indomp->domain = 123;
    indomp->serial = 15;

    if (first) {
	sts = pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	if (sts < 0) {
	    fprintf(stderr, "PMDA_CACHE_LOAD failed: %s\n", pmErrStr(sts));
	}
	first = 0;
    }

    for (i = 0; i_tab[i] != NULL; i++) {
	fprintf(stderr, "[%s]\n", i_tab[i]);
	inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, i_tab[i], NULL);
	if (inst < 0) {
	    fprintf(stderr, "ADD failed: %s\n", pmErrStr(inst));
	    continue;
	}
	fprintf(stderr, "ADD -> %d\n", inst);
	sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
	if (sts < 0) {
	    fprintf(stderr, "PMDA_CACHE_SAVE failed: %s\n", pmErrStr(sts));
	}
	else
	    fprintf(stderr, "SAVE -> %d\n", sts);

	if (i == 0) {
	    /* last one -> INACTIVE */
	    inst = pmdaCacheStore(indom, PMDA_CACHE_HIDE, i_tab[i], NULL);
	    if (inst < 0) {
		fprintf(stderr, "HIDE failed: %s\n", pmErrStr(inst));
		continue;
	    }
	    fprintf(stderr, "HIDE -> %d\n", inst);
	}
	else if (i == 1) {
	    /* last one -> INACTIVE -> ACTIVE */
	    inst = pmdaCacheStore(indom, PMDA_CACHE_HIDE, i_tab[i], NULL);
	    if (inst < 0) {
		fprintf(stderr, "HIDE failed: %s\n", pmErrStr(inst));
		continue;
	    }
	    fprintf(stderr, "HIDE -> %d\n", inst);
	    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, i_tab[i], NULL);
	    if (inst < 0) {
		fprintf(stderr, "ADD failed: %s\n", pmErrStr(inst));
		continue;
	    }
	    fprintf(stderr, "ADD -> %d\n", inst);
	}
	else if (i == 2) {
	    /* last one -> EMPTY */
	    inst = pmdaCacheStore(indom, PMDA_CACHE_CULL, i_tab[i], NULL);
	    if (inst < 0) {
		fprintf(stderr, "CULL failed: %s\n", pmErrStr(inst));
		continue;
	    }
	    fprintf(stderr, "CULL -> %d\n", inst);
	}
	else if (i == 3) {
	    /* add another one */
	    fprintf(stderr, "extra [%s]\n", "felix-the-cat");
	    inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, "felix-the-cat", NULL);
	    if (inst < 0) {
		fprintf(stderr, "ADD failed: %s\n", pmErrStr(inst));
		continue;
	    }
	    fprintf(stderr, "ADD -> %d\n", inst);
	}
	else if (i == 4) {
	    /* do nothing! */
	    ;
	}
	else {
	    /* empty cache */
	    sts = pmdaCacheOp(indom, PMDA_CACHE_CULL);
	    if (sts < 0) {
		fprintf(stderr, "PMDA_CACHE_CULL failed: %s\n", pmErrStr(sts));
		return;
	    }
	    fprintf(stderr, "CULL ALL -> %d\n", inst);
	}

	if (style & 1) {
	    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
	    if (sts < 0) {
		fprintf(stderr, "PMDA_CACHE_SAVE failed: %s\n", pmErrStr(sts));
	    }
	    else
		fprintf(stderr, "SAVE -> %d\n", sts);
	}

	if (style & 2) {
	    sts = pmdaCacheOp(indom, PMDA_CACHE_SYNC);
	    if (sts < 0) {
		fprintf(stderr, "PMDA_CACHE_SYNC failed: %s\n", pmErrStr(sts));
	    }
	    else
		fprintf(stderr, "SYNC -> %d\n", sts);
	}
    }
}

static void
_j(void)
{
    int		inst;
    int		sts;
    int		i;

    indomp->domain = 123;
    indomp->serial = 10;

    fprintf(stderr, "\nPopulate the instance domain ...\n");
    for (i = 0; i < 20; i++) {
	strncpy(nbuf, xxx, ncount+3);
	sprintf(nbuf, "%03d", ncount);
	ncount++;
        inst = pmdaCacheStore(indom, PMDA_CACHE_ADD, nbuf, (void *)((__psint_t)(0xbeef0000+ncount)));
	if (inst < 0)
	    fprintf(stderr, "PMDA_CACHE_ADD failed for \"%s\": %s\n", nbuf, pmErrStr(inst));
    }
    sts = pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    fprintf(stderr, "Save -> %d\n", sts);
    __pmdaCacheDump(stderr, indom, 0);

    /* We've now got a cache with 20 items. Try to resize the cache. */
    sts = pmdaCacheResize(indom, 1234);
    fprintf(stderr, "Resize (good) -> %d", sts);
    if (sts < 0) fprintf(stderr, ": %s", pmErrStr(sts));
    fputc('\n', stderr);

    /* This should fail, since we've got more than 10 items in this cache. */
    sts = pmdaCacheResize(indom, 10);
    fprintf(stderr, "Resize (bad) -> %d", sts);
    if (sts < 0) fprintf(stderr, ": %s", pmErrStr(sts));
    fputc('\n', stderr);

    /* Since we've changed the max, we'll need to save
       again. PMDA_CACHE_SAVE won't resave the cache with just a
       dirty cache header, but PMDA_CACHE_SYNC will. */
    sts = pmdaCacheOp(indom, PMDA_CACHE_SYNC);
    fprintf(stderr, "Sync -> %d\n", sts);
}

int
main(int argc, char **argv)
{
    int		errflag = 0;
    int		sts;
    int		c;

    __pmSetProgname(argv[0]);

    indomp = (__pmInDom_int *)&indom;

    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {

#ifdef PCP_DEBUG
	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;
#endif

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s [-D...] [a|b|c|d|...|i 1|2|3}\n", pmProgname);
	exit(1);
    }

    while (optind < argc) {
	if (strcmp(argv[optind], "a") == 0) _a(0, 1, 1);
	else if (strcmp(argv[optind], "b") == 0) _b();
	else if (strcmp(argv[optind], "c") == 0) _c();
	else if (strcmp(argv[optind], "d") == 0) _a(1, 0, 1);
	else if (strcmp(argv[optind], "e") == 0) _e(0);
	else if (strcmp(argv[optind], "f") == 0) _e(3600);
	else if (strcmp(argv[optind], "g") == 0) _g();
	else if (strcmp(argv[optind], "h") == 0) _h();
	else if (strcmp(argv[optind], "i") == 0) {
	    optind++;
	    _i(atoi(argv[optind]));
	}
	else if (strcmp(argv[optind], "j") == 0) _j();
	else
	    fprintf(stderr, "torture_cache: no idea what to do with option \"%s\"\n", argv[optind]);
	optind++;
    }

    exit(0);
}
