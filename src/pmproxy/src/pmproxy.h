/*
 * Copyright (c) 2012-2013,2018 Red Hat.
 * Copyright (c) 2002 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#ifndef PMPROXY_H
#define PMPROXY_H

extern void *OpenRequestPorts(const char *, int);
extern void DumpRequestPorts(FILE *, void *);
extern void *GetServerInfo(void);
extern void MainLoop(void *);
extern void ShutdownPorts(void *);

extern void SignalShutdown(void);
extern void Shutdown(void);

extern int timeToDie;	/* for SIGINT handling */

#endif /* PMPROXY_H */
