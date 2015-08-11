/*
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
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
#ifndef _PRIVATE_H
#define _PRIVATE_H

typedef struct {
    char	*name;
    pmID	pmid;
    pmDesc	desc;
    int		meta_done;
} pmi_metric;

typedef struct {
    pmInDom	indom;
    int		ninstance;
    char	**name;		// list of external instance names
    int		*inst;		// list of internal instance identifiers
    int		namebuflen;	// names are packed in namebuf[] as
    char	*namebuf;	// required by __pmLogPutInDom()
    int		meta_done;
} pmi_indom;

typedef struct {
    int		midx;		// index into metric[]
    int		inst;		// internal instance identifier
} pmi_handle;

typedef struct {
    int		state;
    char	*archive;
    char	*hostname;
    char	*timezone;
    __pmLogCtl	logctl;
    pmResult	*result;
    int		nmetric;
    pmi_metric	*metric;
    int		nindom;
    pmi_indom	*indom;
    int		nhandle;
    pmi_handle	*handle;
    int		last_sts;
    struct timeval	last_stamp;
} pmi_context;

#define CONTEXT_START	1
#define CONTEXT_ACTIVE	2
#define CONTEXT_END	3

#if defined(__GNUC__) && (__GNUC__ >= 4) && !defined(IS_MINGW)
# define _PMI_HIDDEN __attribute__ ((visibility ("hidden")))
#else
# define _PMI_HIDDEN
#endif

extern int _pmi_stuff_value(pmi_context *, pmi_handle *, const char *) _PMI_HIDDEN;
extern int _pmi_put_result(pmi_context *, pmResult *) _PMI_HIDDEN;
extern int _pmi_end(pmi_context *) _PMI_HIDDEN;

#endif /* _PRIVATE_H */
