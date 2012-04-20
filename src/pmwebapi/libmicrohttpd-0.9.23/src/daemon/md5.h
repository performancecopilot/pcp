/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.	This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

/* Brutally hacked by John Walker back from ANSI C to K&R (no
   prototypes) to maintain the tradition that Netfone will compile
   with Sun's original "cc". */

#ifndef MD5_H
#define MD5_H

#include "platform.h"
#ifdef WORDS_BIGENDIAN
#define HIGHFIRST
#endif

#define MD5_DIGEST_SIZE 16

struct MD5Context 
{
  uint32_t buf[4];
  uint32_t bits[2];
  unsigned char in[64];
};


void 
MD5Init(struct MD5Context *ctx);

void
MD5Update(struct MD5Context *ctx,
	  const void *buf,
	  unsigned len);

void MD5Final(unsigned char digest[MD5_DIGEST_SIZE],
	      struct MD5Context *ctx);

#endif /* !MD5_H */
