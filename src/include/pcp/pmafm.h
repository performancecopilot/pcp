/*
 * Copyright (c) 1997 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef PCP_PMAFM_H
#define PCP_PMAFM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Recording session support
 */
#define PM_REC_ON	40
#define PM_REC_OFF	41
#define PM_REC_DETACH	43
#define PM_REC_STATUS	44
#define PM_REC_SETARG	45

typedef struct pmRecordHost {
    FILE	*f_config;	/* caller writes pmlogger configuration here */
    int		fd_ipc;		/* IPC channel to pmlogger */
    char	*logfile;	/* full pathname for pmlogger error logfile */
    pid_t	pid;		/* process id for pmlogger */
    int		status;		/* exit status, -1 if unknown */
} pmRecordHost;

extern FILE *pmRecordSetup(const char *, const char *, int);
extern int pmRecordAddHost(const char *, int, pmRecordHost **);
extern int pmRecordControl(pmRecordHost *, int, const char *);

#ifdef __cplusplus
}
#endif

#endif	/* PCP_PMAFM_H */
