/*
 * Copyright (c) 2012-2018,2022 Red Hat.
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
#ifndef PCP_IMPORT_H
#define PCP_IMPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(IS_MINGW)
# define PMI_CALL
#else
# if defined(PMI_INTERNAL)
#  define PMI_CALL __declspec(dllexport)
# else
#  define PMI_CALL __declspec(dllimport)
# endif
#endif

/* core libpcp_import API routines */
PMI_CALL extern int pmiStart(const char *, int);
PMI_CALL extern int pmiUseContext(int);
PMI_CALL extern int pmiEnd(void);
PMI_CALL extern int pmiSetHostname(const char *);
PMI_CALL extern int pmiSetTimezone(const char *);
PMI_CALL extern int pmiSetVersion(int);
PMI_CALL extern int pmiAddMetric(const char *, pmID, int, pmInDom, int, pmUnits);
PMI_CALL extern int pmiAddInstance(pmInDom, const char *, int);
PMI_CALL extern int pmiPutValue(const char *, const char *, const char *);
PMI_CALL extern int pmiGetHandle(const char *, const char *);
PMI_CALL extern int pmiPutValueHandle(int, const char *);
PMI_CALL extern int pmiWrite(int, int);
PMI_CALL extern int pmiPutResult(const pmResult *);
PMI_CALL extern int pmiPutMark(void);
PMI_CALL extern int pmiPutText(unsigned int, unsigned int, unsigned int, const char *);
PMI_CALL extern int pmiPutLabel(unsigned int, unsigned int, unsigned int, const char *, const char *);

/* helper routines */
PMI_CALL extern pmID pmiID(int, int, int);
PMI_CALL extern pmID pmiCluster(int, int);
PMI_CALL extern pmInDom pmiInDom(int, int);
PMI_CALL extern pmUnits pmiUnits(int, int, int, int, int, int);

/* diagnostic routines */
#define PMI_MAXERRMSGLEN	128	/* sized to accomodate any error message */
PMI_CALL extern char *pmiErrStr_r(int, char *, int);
PMI_CALL extern const char *pmiErrStr(int);
PMI_CALL extern void pmiDump(void);

/* libpcp_import error codes */
#define PMI_ERR_BASE 20000
#define PMI_ERR_DUPMETRICNAME	(-PMI_ERR_BASE-1) /* Metric name already defined */
#define PMI_ERR_DUPMETRICID	(-PMI_ERR_BASE-2) /* Metric pmID already defined */
#define PMI_ERR_DUPINSTNAME	(-PMI_ERR_BASE-3) /* External instance name already defined */
#define PMI_ERR_DUPINSTID	(-PMI_ERR_BASE-4) /* Internal instance identifer already defined */
#define PMI_ERR_INSTNOTNULL	(-PMI_ERR_BASE-5) /* Non-null instance expected for a singular metric */
#define PMI_ERR_INSTNULL	(-PMI_ERR_BASE-6) /* Null instance not allowed for a non-singular metric */
#define PMI_ERR_BADHANDLE	(-PMI_ERR_BASE-7) /* Illegal handle */
#define PMI_ERR_DUPVALUE	(-PMI_ERR_BASE-8) /* Value already assigned for singular metric */
#define PMI_ERR_BADTYPE		(-PMI_ERR_BASE-9) /* Illegal metric type */
#define PMI_ERR_BADSEM		(-PMI_ERR_BASE-10) /* Illegal metric semantics */
#define PMI_ERR_NODATA		(-PMI_ERR_BASE-11) /* No data to output */
#define PMI_ERR_BADMETRICNAME	(-PMI_ERR_BASE-12) /* Illegal metric name */
#define PMI_ERR_BADTIMESTAMP	(-PMI_ERR_BASE-13) /* Illegal result timestamp */
#define PMI_ERR_BADTEXTTYPE	(-PMI_ERR_BASE-14) /* Illegal text type */
#define PMI_ERR_BADTEXTCLASS	(-PMI_ERR_BASE-15) /* Illegal text class */
#define PMI_ERR_BADTEXTID	(-PMI_ERR_BASE-16) /* Illegal text identifier */
#define PMI_ERR_EMPTYTEXTCONTENT (-PMI_ERR_BASE-17)/* Empty text content */
#define PMI_ERR_DUPTEXT         (-PMI_ERR_BASE-18) /* Duplicate text */
#define PMI_ERR_BADLABELTYPE    (-PMI_ERR_BASE-19) /* Illegal label type */
#define PMI_ERR_BADLABELID      (-PMI_ERR_BASE-20) /* Illegal label id */
#define PMI_ERR_BADLABELINSTANCE (-PMI_ERR_BASE-21)/* Illegal label instance */
#define PMI_ERR_EMPTYLABELNAME  (-PMI_ERR_BASE-22) /* Empty label name */
#define PMI_ERR_EMPTYLABELVALUE (-PMI_ERR_BASE-23) /* Empty label value */
#define PMI_ERR_ADDLABELERROR   (-PMI_ERR_BASE-24) /* Error adding label */
#define PMI_ERR_BADVERSION      (-PMI_ERR_BASE-25) /* Illegal archive version */

#ifdef __cplusplus
}
#endif

#endif /* PCP_IMPORT_H */
