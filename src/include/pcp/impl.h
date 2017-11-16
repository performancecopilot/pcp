/*
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
#ifndef PCP_IMPL_H
#define PCP_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * functions to be promoted to pmapi.h and the __pm version added
 * to deprecated.*
 * === start ==
 */

/* platform independent set process identity */
PCP_CALL extern int __pmSetProcessIdentity(const char *);

/*
 * Startup handling:
 * set default user for __pmSetProcessIdentity() ... default is "pcp"
 */
PCP_CALL extern int __pmGetUsername(char **);

/* filesystem path name separator */
PCP_CALL extern int __pmPathSeparator(void);

/* safely insert an atom value into a pmValue */
PCP_CALL extern int __pmStuffValue(const pmAtomValue *, pmValue *, int);

/* struct timeval/timespec manipulations */
PCP_CALL extern void __pmtimevalNow(struct timeval *);
PCP_CALL extern void __pmtimevalInc(struct timeval *, const struct timeval *);
PCP_CALL extern void __pmtimevalDec(struct timeval *, const struct timeval *);
PCP_CALL extern double __pmtimevalAdd(const struct timeval *, const struct timeval *);
PCP_CALL extern double __pmtimevalSub(const struct timeval *, const struct timeval *);
PCP_CALL extern double __pmtimevalToReal(const struct timeval *);
PCP_CALL extern void __pmtimevalFromReal(double, struct timeval *);
PCP_CALL extern void __pmPrintStamp(FILE *, const struct timeval *);
PCP_CALL extern void __pmPrintHighResStamp(FILE *, const struct timespec *);

PCP_CALL extern void __pmPrintDesc(FILE *, const pmDesc *);


#ifdef __cplusplus
}
#endif

#endif /* PCP_IMPL_H */
