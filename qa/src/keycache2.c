/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * Exercise pmdaCacheLookup(), pmdaCacheLookupName() and pmdaCacheLookupKey()
 * in libpcp_pmda
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>
#include <arpa/inet.h>

static __uint32_t hash(const signed char *, int, __uint32_t);

/* hash attempts looking for hash synonyms */
#define MAXPOKE 200000

typedef struct {
    int	inst;
    int	key;
} inst_key_t;

static int
compar(const void *a, const void *b)
{
    const inst_key_t	*ia = (inst_key_t *)a;
    const inst_key_t	*ib = (inst_key_t *)b;

    if (ib->inst != ia->inst)
	return ib->inst - ia->inst;
    /*
     * inst same (synonyms), reverse sort on key to make results deterministic
     */
    return ia->key - ib->key;
}

int
main(int argc, char **argv)
{
    int		key;
    int		inst;
    pmInDom	indom;
    int		c;
    char	name[40];
    char	*oname;
    void	*addr;
    int		sts;
    int		errflag = 0;
    int		kflag = 0;
    char	*usage = "[-D debug] [-k]";
    __uint32_t	try = 0;
    int		dup;
    inst_key_t	poke[MAXPOKE];
    int		mykeylen = 0;			/* pander to gcc */
    void	*mykey = NULL;			/* pander to gcc */

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:k")) != EOF) {
	switch (c) {

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

	case 'k':	/* use key[], default is to use name[] */
	    kflag = 1;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc) {
	fprintf(stderr, "Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    indom = pmInDom_build(42, 42);

    /*
     * hash MAXPOKE keys
     */
    if (kflag) {
	mykeylen = sizeof(key);
	mykey = (void *)&key;
    }
    for (key = 0; key < MAXPOKE; key++) {
	if (!kflag) {
	    sprintf(name, "key-%d", key);
	    mykeylen = strlen(name);
	    mykey = (void *)name;
	}
	key = htonl(key);
	try = hash((const signed char *)mykey, mykeylen, 0);
	key = ntohl(key);
	/* strip top bit ... instance id must be positive */
	inst = try & ~(1 << (8*sizeof(__uint32_t)-1));
	poke[key].inst = inst;
	poke[key].key = key;
    }

    /* sort on hash value */
    qsort(poke, MAXPOKE, sizeof(poke[0]), compar);

    /*
     * look for hash synonyms, and stuff them away in pairs in the
     * start of poke[]
     */ 
    dup = 0;
    for (key = 1; key < MAXPOKE; key++) {
	if (poke[key-1].inst == poke[key].inst) {
	    if (kflag)
		fprintf(stderr, "keys %d & %d hash to %d\n", poke[key-1].key, poke[key].key, poke[key-1].inst);
	    else
		fprintf(stderr, "keys \"key-%d\" & \"key-%d\" hash to %d\n", poke[key-1].key, poke[key].key, poke[key-1].inst);
	    sprintf(name, "key-%d", poke[key-1].key);
	    if (kflag) {
		mykeylen = sizeof(key);
		mykey = (void *)&poke[key-1].key;
	    }
	    else {
		mykeylen = strlen(name);
		mykey = (void *)name;
	    }
	    poke[key-1].key = htonl(poke[key-1].key);
	    sts = pmdaCacheStoreKey(indom, PMDA_CACHE_ADD, name, mykeylen, mykey, &poke[dup]);
	    poke[key-1].key = ntohl(poke[key-1].key);
	    if (sts < 0) {
		if (kflag)
		    fprintf(stderr, "pmdaCacheStoreKey(... %s, ... %d): %s\n", name, poke[key-1].key, pmErrStr(sts));
		else
		    fprintf(stderr, "pmdaCacheStoreKey(... %s, ... NULL): %s\n", name, pmErrStr(sts));
	    }
	    else
		fprintf(stderr, "%s -> %d\n", name, sts);
	    poke[dup].key = poke[key-1].key;
	    poke[dup++].inst = sts;
	    sprintf(name, "key-%d", poke[key].key);
	    if (kflag) {
		mykeylen = sizeof(key);
		mykey = (void *)&poke[key].key;
	    }
	    else {
		mykeylen = strlen(name);
		mykey = (void *)name;
	    }
	    poke[key].key = htonl(poke[key].key);
	    sts = pmdaCacheStoreKey(indom, PMDA_CACHE_ADD, name, mykeylen, mykey, &poke[dup]);
	    poke[key].key = ntohl(poke[key].key);
	    if (sts < 0) {
		if (kflag)
		    fprintf(stderr, "pmdaCacheStoreKey(... %s, ... %d): %s\n", name, poke[key].key, pmErrStr(sts));
		else
		    fprintf(stderr, "pmdaCacheStoreKey(... %s, ... NULL): %s\n", name, pmErrStr(sts));
	    }
	    else
		fprintf(stderr, "%s -> %d\n", name, sts);
	    poke[dup].key = poke[key].key;
	    poke[dup++].inst = sts;
	}
    }

    /* mark the first and last as inactive, and cull the middle entry */
    sprintf(name, "key-%d", poke[0].key);
    sts = pmdaCacheStore(indom, PMDA_CACHE_HIDE, name, NULL);
    if (sts < 0)
	fprintf(stderr, "pmdaCacheStore(... HIDE \"%s\", ...) failed: %s\n", name, pmErrStr(sts));
    sprintf(name, "key-%d", poke[dup-1].key);
    sts = pmdaCacheStore(indom, PMDA_CACHE_HIDE, name, NULL);
    if (sts < 0)
	fprintf(stderr, "pmdaCacheStore(... HIDE \"%s\", ...) failed: %s\n", name, pmErrStr(sts));
    sprintf(name, "key-%d", poke[dup/2].key);
    sts = pmdaCacheStore(indom, PMDA_CACHE_CULL, name, NULL);
    if (sts < 0)
	fprintf(stderr, "pmdaCacheStore(... CULL \"%s\", ...) failed: %s\n", name, pmErrStr(sts));

    pmdaCacheOp(indom, PMDA_CACHE_DUMP);

    for (key = 0; key < dup; key++) {
	sprintf(name, "key-%d", poke[key].key);

	sts = pmdaCacheLookup(indom, poke[key].inst, &oname, &addr);
	fprintf(stderr, "pmdaCacheLookup(... %d, ...)", poke[key].inst);
	if (sts < 0)
	    fprintf(stderr, ": %s", pmErrStr(sts));
	else {
	    if (sts == PMDA_CACHE_ACTIVE)
		fprintf(stderr, " -> active");
	    else if (sts == PMDA_CACHE_INACTIVE)
		fprintf(stderr, " -> inactive");
	    else
		fprintf(stderr, " -> %d?", sts);
	    if (strcmp(name, oname) == 0)
		fprintf(stderr, " name ok");
	    else
		fprintf(stderr, " name? \"%s\" [expected \"%s\"]", oname, name);
	    if (addr == (void *)&poke[key])
		fprintf(stderr, " private ok");
	    else
		fprintf(stderr, " private? %p [expected %p]", &poke[key], addr);
	}
	fputc('\n', stderr);

	sts = pmdaCacheLookupName(indom, name, &inst, &addr);
	fprintf(stderr, "pmdaCacheLookupName(... \"%s\", ...)", name);
	if (sts < 0)
	    fprintf(stderr, ": %s", pmErrStr(sts));
	else {
	    if (sts == PMDA_CACHE_ACTIVE)
		fprintf(stderr, " -> active");
	    else if (sts == PMDA_CACHE_INACTIVE)
		fprintf(stderr, " -> inactive");
	    else
		fprintf(stderr, " -> %d?", sts);
	    if (inst == poke[key].inst)
		fprintf(stderr, " inst ok");
	    else
		fprintf(stderr, " inst? %d [expected %d]", inst, poke[key].inst);
	    if (addr == (void *)&poke[key])
		fprintf(stderr, " private ok");
	    else
		fprintf(stderr, " private? %p [expected %p]", &poke[key], addr);
	}
	fputc('\n', stderr);

	if (kflag) {
	    mykeylen = sizeof(key);
	    mykey = (void *)&poke[key].key;
	}
	else {
	    mykeylen = strlen(name);
	    mykey = (void *)name;
	}
	poke[key].key = htonl(poke[key].key);
	sts = pmdaCacheLookupKey(indom, name, mykeylen, mykey, &oname, &inst, &addr);
	poke[key].key = ntohl(poke[key].key);
	fprintf(stderr, "pmdaCacheLookupKey(... \"%s\", ...)", name);
	if (sts < 0)
	    fprintf(stderr, ": %s", pmErrStr(sts));
	else {
	    if (sts == PMDA_CACHE_ACTIVE)
		fprintf(stderr, " -> active");
	    else if (sts == PMDA_CACHE_INACTIVE)
		fprintf(stderr, " -> inactive");
	    else
		fprintf(stderr, " -> %d?", sts);
	    if (strcmp(name, oname) == 0)
		fprintf(stderr, " name ok");
	    else
		fprintf(stderr, " name? \"%s\" [expected \"%s\"]", oname, name);
	    if (inst == poke[key].inst)
		fprintf(stderr, " inst ok");
	    else
		fprintf(stderr, " inst? %d [expected %d]", inst, poke[key].inst);
	    if (addr == (void *)&poke[key])
		fprintf(stderr, " private ok");
	    else
		fprintf(stderr, " private? %p [expected %p]", &poke[key], addr);
	}
	fputc('\n', stderr);

    }


    return 0;
}

/*
 * lifted directly from libpcp_pmda/src/cache.c
 */

/*
--------------------------------------------------------------------
lookup2.c, by Bob Jenkins, December 1996, Public Domain.
hash(), hash2(), hash3, and mix() are externally useful functions.
Routines to test the hash are included if SELF_TEST is defined.
You can use this free for any purpose.  It has no warranty.
--------------------------------------------------------------------
*/

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
typedef  unsigned long  int  ub4;   /* unsigned 4-byte quantities */
typedef  unsigned       char ub1;

#define hashsize(n) ((ub4)1<<(n))
#define hashmask(n) (hashsize(n)-1)

/*
--------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.
For every delta with one or two bit set, and the deltas of all three
  high bits or all three low bits, whether the original value of a,b,c
  is almost all zero or is uniformly distributed,
* If mix() is run forward or backward, at least 32 bits in a,b,c
  have at least 1/4 probability of changing.
* If mix() is run forward, every bit of c will change between 1/3 and
  2/3 of the time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
mix() was built out of 36 single-cycle latency instructions in a 
  structure that could supported 2x parallelism, like so:
      a -= b; 
      a -= c; x = (c>>13);
      b -= c; a ^= x;
      b -= a; x = (a<<8);
      c -= a; b ^= x;
      c -= b; x = (b>>13);
      ...
  Unfortunately, superscalar Pentiums and Sparcs can't take advantage 
  of that parallelism.  They've also turned some of those single-cycle
  latency instructions into multi-cycle latency instructions.  Still,
  this is the fastest good hash I could find.  There were about 2^^68
  to choose from.  I only looked at a billion or so.
--------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

/*
--------------------------------------------------------------------
hash() -- hash a variable-length key into a 32-bit value
  k     : the key (the unaligned variable-length array of bytes)
  len   : the length of the key, counting by bytes
  level : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Every 1-bit and 2-bit delta achieves avalanche.
About 36+6len instructions.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (ub1 **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hash( k[i], len[i], h);

By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

See http://burlteburtle.net/bob/hash/evahash.html
Use for hash table lookup, or anything where one collision in 2^32 is
acceptable.  Do NOT use for cryptographic purposes.
--------------------------------------------------------------------
*/

static __uint32_t
hash(const signed char *k, int length, __uint32_t initval)
{
   __uint32_t a,b,c,len;

   /* Set up the internal state */
   len = length;
   a = b = 0x9e3779b9;  /* the golden ratio; an arbitrary value */
   c = initval;           /* the previous hash value */

   /*---------------------------------------- handle most of the key */
   while (len >= 12)
   {
      a += (k[0] +((__uint32_t)k[1]<<8) +((__uint32_t)k[2]<<16)
      +((__uint32_t)k[3]<<24));
      b += (k[4] +((__uint32_t)k[5]<<8) +((__uint32_t)k[6]<<16)
      +((__uint32_t)k[7]<<24));
      c += (k[8] +((__uint32_t)k[9]<<8)
      +((__uint32_t)k[10]<<16)+((__uint32_t)k[11]<<24));
      mix(a,b,c);
      k += 12; len -= 12;
   }

   /*------------------------------------- handle the last 11 bytes */
   c += length;
   switch(len)              /* all the case statements fall through */
   {
   case 11: c+=((__uint32_t)k[10]<<24);
   case 10: c+=((__uint32_t)k[9]<<16);
   case 9 : c+=((__uint32_t)k[8]<<8);
      /* the first byte of c is reserved for the length */
   case 8 : b+=((__uint32_t)k[7]<<24);
   case 7 : b+=((__uint32_t)k[6]<<16);
   case 6 : b+=((__uint32_t)k[5]<<8);
   case 5 : b+=k[4];
   case 4 : a+=((__uint32_t)k[3]<<24);
   case 3 : a+=((__uint32_t)k[2]<<16);
   case 2 : a+=((__uint32_t)k[1]<<8);
   case 1 : a+=k[0];
     /* case 0: nothing left to add */
   }
   mix(a,b,c);
   /*-------------------------------------------- report the result */
   return c;
}
