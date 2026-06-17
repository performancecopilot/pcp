/*
 * Copyright (c) 2013-2018,2021-2022,2026 Red Hat.
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
    unsigned int	type;
    unsigned int	id;
    char		*content;
    int			meta_done;
} pmi_text;

typedef struct {
    unsigned int	type;
    unsigned int	id;
    pmLabelSet		*labelset;
} pmi_label;

typedef struct {
    int			state;
    int			version;
    char		*archive;
    char		*hostname;
    char		*timezone;
    __pmLogCtl		logctl;
    __pmArchCtl		archctl;
    __pmResult		*result;
    /*
     * Pair consecutive int fields to avoid 4-byte padding before each
     * pointer on 64-bit platforms (private struct, no ABI constraint).
     */
    int			nmetric;
    int			nindom;
    pmi_metric		*metric;
    pmi_indom		*indom;
    int			nhandle;
    int			ntext;
    pmi_handle		*handle;
    pmi_text		*text;
    int			nlabel;
    int			last_sts;
    pmi_label		*label;
    __pmTimestamp	last_stamp;
    /* optional volume rotation: 0 = disabled, callback may be NULL */
    size_t		max_volume_bytes;
    void		(*on_volume_rotate)(const char *vol_path);
} pmi_context;

#define CONTEXT_START	1
#define CONTEXT_ACTIVE	2
#define CONTEXT_END	3
#define CONTEXT_APPEND	4	/* open existing archive for appending */

#if defined(__GNUC__) && (__GNUC__ >= 4) && !defined(IS_MINGW)
# define _PMI_HIDDEN __attribute__ ((visibility ("hidden")))
#else
# define _PMI_HIDDEN
#endif

extern int _pmi_stuff_atomvalue(pmi_context *, pmi_handle *, pmAtomValue *) _PMI_HIDDEN;
extern int _pmi_stuff_value(pmi_context *, pmi_handle *, const char *) _PMI_HIDDEN;
extern int _pmi_put_result(pmi_context *, __pmResult *) _PMI_HIDDEN;
extern int _pmi_put_text(pmi_context *) _PMI_HIDDEN;
extern int _pmi_put_label(pmi_context *) _PMI_HIDDEN;
extern int _pmi_end(pmi_context *) _PMI_HIDDEN;

#endif /* _PRIVATE_H */
