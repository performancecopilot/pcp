/*
 * Copyright (c) 2013-2015 Red Hat.
 * Copyright (c) 1995,2005 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef PCP_PMDAROOT_H
#define PCP_PMDAROOT_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal PDU exchange details for elevated privilege operations.
 * Only PMCD and some very specific PMDAs need to know about this.
 */
#define ROOT_PDU_VERSION1	1
#define ROOT_PDU_VERSION2	2
#define ROOT_PDU_VERSION	ROOT_PDU_VERSION2

#define PDUROOT_INFO		0x9000
#define PDUROOT_HOSTNAME_REQ	0x9001
#define PDUROOT_HOSTNAME	0x9002
#define PDUROOT_PROCESSID_REQ	0x9003
#define PDUROOT_PROCESSID	0x9004
#define PDUROOT_CGROUPNAME_REQ	0x9005
#define PDUROOT_CGROUPNAME	0x9006
#define PDUROOT_STARTPMDA_REQ	0x9007
#define PDUROOT_STARTPMDA	0x9008
#define PDUROOT_STOPPMDA_REQ	0x9009
#define PDUROOT_STOPPMDA	0x900a
/*#define PDUROOT_SASLAUTH_REQ	0x900b*/
/*#define PDUROOT_SASLAUTH	0x900c*/

typedef enum {
    PDUROOT_FLAG_HOSTNAME	= (1<<0),
    PDUROOT_FLAG_PROCESSID	= (1<<1),
    PDUROOT_FLAG_CGROUPNAME	= (1<<2),
} __pmdaRootServerFeature;

typedef struct {
    int		type;
    int		length;
    int		status;
    int		version;
} __pmdaRootPDUHdr;

typedef struct {
    __pmdaRootPDUHdr	hdr;
    int			features;
    int			zeroed;
} __pmdaRootPDUInfo;

/*
 * Common PDU for container operations
 * (PID, host and cgroup name requests and responses).
 */
typedef struct {
    __pmdaRootPDUHdr	hdr;
    int			pid;
    int			namelen;
    char		name[MAXPATHLEN];	/* max possible size */
} __pmdaRootPDUContainer;

/*
 * PDUs requesting pmdaroot start and stop PMDAs on behalf of
 * an unprivileged PMCD parent process.
 */
#define MAXPMDALEN	64			/* max label length */

typedef struct {
    __pmdaRootPDUHdr	hdr;
    int			pid;			/* out: process ID */
    int			infd;			/* out: process stdin */
    int			outfd;			/* out: process stdout */
    int			ipctype;		/* in: socket/pipe */
    int			namelen;		/* in: name length */
    int			argslen;		/* in: args length */
    char		name[MAXPMDALEN];	/* in: process label */
    char		args[MAXPATHLEN];	/* in: process args */
} __pmdaRootPDUStart;

typedef struct {
    __pmdaRootPDUHdr	hdr;
    int			pid;			/* process identifier */
    int			code;			/* waitpid exit status */
    int			force;			/* terminate don't wait */
    int			zeroed;
} __pmdaRootPDUStop;

PMDA_CALL extern int __pmdaSendRootPDUInfo(int, int, int);
PMDA_CALL extern int __pmdaRecvRootPDUInfo(int, int *, int *);
PMDA_CALL extern int __pmdaSendRootPDUContainer(int, int, int, const char *, int, int);
PMDA_CALL extern int __pmdaRecvRootPDUContainer(int, int, void *, int);
PMDA_CALL extern int __pmdaDecodeRootPDUContainer(void *, int, int *, char *, int);
PMDA_CALL extern int __pmdaSendRootPDUStartReq(int, int, const char *, int, const char*, int);
PMDA_CALL extern int __pmdaRecvRootPDUStartReq(int, void *, int);
PMDA_CALL extern int __pmdaSendRootPDUStart(int, int, int, int, const char *, int, int);
PMDA_CALL extern int __pmdaRecvRootPDUStart(int, void *, int);
PMDA_CALL extern int __pmdaDecodeRootPDUStart(void *, int, int *, int *, int *, int *, char *, int, char*, int);
PMDA_CALL extern int __pmdaSendRootPDUStop(int, int, int, int, int, int);
PMDA_CALL extern int __pmdaRecvRootPDUStop(int, int, void *, int);
PMDA_CALL extern int __pmdaDecodeRootPDUStop(void *, int, int *, int *, int *);

#ifdef __cplusplus
}
#endif

#endif /* PCP_PMDAROOT_H */
