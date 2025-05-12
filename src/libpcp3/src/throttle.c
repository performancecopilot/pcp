/*
 * Copyright (c) 2024 Ken McDonell.  All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "internal.h"
#include "fault.h"

typedef struct {
    char	*key;
    int		subkey;
    int		count;
    int		limit;
} thresh_t;

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	throttle_lock = PTHREAD_MUTEX_INITIALIZER;
#else
void			*throttle_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == throttle_lock
 */
int
__pmIsThrottleLock(void *lock)
{
    return lock == (void *)&throttle_lock;
}
#endif

static __pmHashCtl	hashtab;	/* hash table based on key + subkey */
static int		g_limit = -1;	/* from PCP_NOTIFY_THROTTLE, else 10 */

/*
 * hash() is lifted directly from libpcp_pmda/src/lookup2.c
 */
 
/*
--------------------------------------------------------------------
lookup2.c, by Bob Jenkins, December 1996, Public Domain.
hash(), hash2(), hash3, and mix() are externally useful functions.
You can use this free for any purpose.  It has no warranty.
--------------------------------------------------------------------
*/

#include "platform_defs.h"
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

/*
 * keep track of how often we're called for each key:subkey pair
 * and return
 * 	-1 for alloc related errors
 * 	 0 if the limit has not been reached
 * 	 1 if the limit has been reached
 * 	 2 if the limit has been exceeded
 * the "limit" is 10 or $PCP_NOTIFY_THROTTLE if set (see also
 * __pmResetNotifyThrottle() below.
 */
int
__pmNotifyThrottle(const char *key, int subkey)
{
    int			sts = 0;	/* assume success */
    uint32_t		mangle;
    __pmHashNode	*hp;
    thresh_t		*tp;

    PM_LOCK(throttle_lock);

    if (g_limit == -1) {
	/* one trip initialization */
	char	*p;
	g_limit = 10;	/* default */
	if ((p = getenv("PCP_NOTIFY_THROTTLE")) != NULL) {
	    long	val;
	    char	*pend;
	    val = strtol(p, &pend, 10);
	    if (val < 1 || *pend != '\0') {
		pmprintf("__pmNotifyThrottle: bad $PCP_NOTIFY_THROTTLE (%s) using default (10)\n", p);
		pmflush();
	    }
	    else {
		g_limit = val;
		if (pmDebugOptions.misc)
		    fprintf(stderr, "__pmNotifyThrottle: limit=%d from $PCP_NOTIFY_THROTTLE=%s\n", g_limit, p);
	    }
	}
	else {
	    if (pmDebugOptions.misc)
		fprintf(stderr, "__pmNotifyThrottle: default limit=%d\n", g_limit);
	}
    }

    mangle = hash((const signed char *)key, strlen(key), subkey);
    hp = __pmHashSearch(mangle, &hashtab);
    while (hp != NULL) {
	tp = (thresh_t *)hp->data;
	if (tp->subkey == subkey && strcmp(tp->key, key) == 0)
	    break;
	/* key:subkey hash synonym, keep searching ... */
	hp = hp->next;
    }
    if (hp == NULL) {
	/* key:subkey not found, add new hash entry */
	if ((tp = (thresh_t *)malloc(sizeof(*tp))) == NULL) {
	    pmNoMem("__pmNotifyThrottle thres_t", sizeof(*tp), PM_RECOV_ERR);
	    sts = -ENOMEM;
	    goto done;
	}
	if ((tp->key = strdup(key)) == NULL) {
	    pmNoMem("__pmNotifyThrottle strdup", strlen(key), PM_RECOV_ERR);
	    free(tp);
	    sts = -ENOMEM;
	    goto done;
	}
	tp->subkey = subkey;
	tp->count = 1;
	tp->limit = g_limit;
	if ((sts = __pmHashAdd(mangle, (void *)tp, &hashtab)) < 0) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    pmprintf("__pmNotifyThrottle(%s,%d): __pmHashAdd failed: %s\n",
		key, subkey, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    pmflush();
	    free(tp);
	}
	else {
	    /* __pmHashAdd returns 1 for success */
	    sts = 0;
	}
    }
    else {
	/*
	 * existing entry in hash table ... check limits
	 */
	tp = (thresh_t *)hp->data;
	if (++tp->count == tp->limit)
	    sts = 1;
	else if (tp->count > tp->limit)
	    sts = 2;
    }

done:
    PM_UNLOCK(throttle_lock);

    return sts;
}

/*
 * clear the count, (optionally) reset the limit and return
 * the number of "throttled" calls
 *
 * if limit < 1 then no change to the limit.
 *
 * if key == NULL, then this applies to *all* key:subkey pairs and
 * the global limit (for new pairs) if limit >= 1.
 *
 * returns the number of "throttled" events.
 */
int
__pmResetNotifyThrottle(const char *key, int subkey, int limit)
{
    int			sts;
    uint32_t		mangle;
    __pmHashNode	*hp;
    thresh_t		*tp;

    PM_LOCK(throttle_lock);

    if (key != NULL) {
	/* reset a single entry */
	mangle = hash((const signed char *)key, strlen(key), subkey);
	hp = __pmHashSearch(mangle, &hashtab);
	while (hp != NULL) {
	    tp = (thresh_t *)hp->data;
	    if (tp->subkey == subkey && strcmp(tp->key, key) == 0)
		break;
	    /* key:subkey hash synonym, keep searching ... */
	    hp = hp->next;
	}
	if (hp == NULL) {
	    /* not found */
	    if (pmDebugOptions.misc)
		fprintf(stderr, "__pmResetNotifyThrottle(%s, %d, %d) entry not found\n",
		    key, subkey, limit);
	    sts = -ENOENT;
	    goto done;
	}
	tp = (thresh_t *)hp->data;
	sts = tp->count > tp->limit ? tp->count - tp->limit : 0;
	tp->count = 0;
	if (limit >= 1)
	    tp->limit = limit;
	goto done;
    }

    /* do 'em all */
    sts = 0;
    for (hp = __pmHashWalk(&hashtab, PM_HASH_WALK_START);
         hp != NULL;
	 hp = __pmHashWalk(&hashtab, PM_HASH_WALK_NEXT)) {
	tp = (thresh_t *)hp->data;
	sts += tp->count > tp->limit ? tp->count - tp->limit : 0;
	tp->count = 0;
	if (limit >= 1)
	    tp->limit = limit;
    }
    /* and update global limit for any new key:subkey pairs */
    if (limit >= 1) {
	g_limit = limit;
	if (pmDebugOptions.misc)
	    fprintf(stderr, "__pmResetNotifyThrottle: limit=%d\n", g_limit);
    }

done:
    PM_UNLOCK(throttle_lock);

    return sts;
}
