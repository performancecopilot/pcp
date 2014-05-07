/*
 * Copyright (C) 2013 Red Hat.
 *
 * This file is part of the "pcp" module, the python interfaces for the
 * Performance Co-Pilot toolkit.
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

/**************************************************************************\
**                                                                        **
** This C extension module mainly serves the purpose of loading functions **
** and macros needed to implement PMDAs in python.  These are exported to **
** python PMDAs via the pmda.py module, using ctypes.                     **
**                                                                        **
\**************************************************************************/

#include <Python.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include <pcp/impl.h>

static pmdaInterface dispatch;
static __pmnsTree *pmns;
static int need_refresh;
static PyObject *pmns_dict;		/* metric pmid:names dictionary */
static PyObject *pmid_oneline_dict;	/* metric pmid:short text */
static PyObject *pmid_longtext_dict;	/* metric pmid:long help */
static PyObject *indom_oneline_dict;	/* indom pmid:short text */
static PyObject *indom_longtext_dict;	/* indom pmid:long help */

static PyObject *fetch_func;
static PyObject *refresh_func;
static PyObject *instance_func;
static PyObject *store_cb_func;
static PyObject *fetch_cb_func;

#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION <= 5
typedef int Py_ssize_t;
#endif

static void
pmns_refresh(void)
{
    int sts, count = 0;
    Py_ssize_t pos = 0;
    PyObject *key, *value;

    if (pmDebug & DBG_TRACE_LIBPMDA)
        fprintf(stderr, "pmns_refresh: rebuilding namespace\n");

    if (pmns)
        __pmFreePMNS(pmns);

    if ((sts = __pmNewPMNS(&pmns)) < 0) {
        __pmNotifyErr(LOG_ERR, "failed to create namespace root: %s",
                      pmErrStr(sts));
        return;
    }

    while (PyDict_Next(pmns_dict, &pos, &key, &value)) {
        const char *name;
        long pmid;

        pmid = PyLong_AsLong(key);
        name = PyString_AsString(value);
        if (pmDebug & DBG_TRACE_LIBPMDA)
            fprintf(stderr, "pmns_refresh: adding metric %s(%s)\n",
                    name, pmIDStr(pmid));
        if ((sts = __pmAddPMNSNode(pmns, pmid, name)) < 0) {
            __pmNotifyErr(LOG_ERR,
                    "failed to add metric %s(%s) to namespace: %s",
                    name, pmIDStr(pmid), pmErrStr(sts));
        } else {
            count++;
        }
    }

    pmdaTreeRebuildHash(pmns, count); /* for reverse (pmid->name) lookups */
    Py_DECREF(pmns_dict);
    need_refresh = 0;
    pmns_dict = NULL;
}

static PyObject *
namespace_refresh(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *keyword_list[] = {"metrics", NULL};

    if (pmns_dict) {
	Py_DECREF(pmns_dict);
	pmns_dict = NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "O:namespace_refresh", keyword_list, &pmns_dict))
        return NULL;
    if (pmns_dict) {
        if (!PyDict_Check(pmns_dict)) {
            __pmNotifyErr(LOG_ERR,
                "attempted to refresh namespace with non-dict type");
            Py_DECREF(pmns_dict);
            pmns_dict = NULL;
        } else if (need_refresh) {
            pmns_refresh();
        }
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pmid_oneline_refresh(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *keyword_list[] = {"oneline", NULL};

    if (pmid_oneline_dict) {
	Py_DECREF(pmid_oneline_dict);
	pmid_oneline_dict = NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "O:pmid_oneline_refresh",
                        keyword_list, &pmid_oneline_dict))
        return NULL;

    if (pmid_oneline_dict) {
        if (!PyDict_Check(pmid_oneline_dict)) {
            __pmNotifyErr(LOG_ERR,
                "attempted to refresh pmid oneline help with non-dict type");
            Py_DECREF(pmid_oneline_dict);
            pmid_oneline_dict = NULL;
        }
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pmid_longtext_refresh(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *keyword_list[] = {"longtext", NULL};

    if (pmid_longtext_dict) {
	Py_DECREF(pmid_longtext_dict);
	pmid_longtext_dict = NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "O:pmid_longtext_refresh",
                        keyword_list, &pmid_longtext_dict))
        return NULL;

    if (pmid_longtext_dict) {
        if (!PyDict_Check(pmid_longtext_dict)) {
            __pmNotifyErr(LOG_ERR,
                "attempted to refresh pmid long help with non-dict type");
            Py_DECREF(pmid_longtext_dict);
            pmid_longtext_dict = NULL;
        }
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
indom_oneline_refresh(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *keyword_list[] = {"oneline", NULL};

    if (indom_oneline_dict) {
	Py_DECREF(indom_oneline_dict);
	indom_oneline_dict = NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "O:indom_oneline_refresh",
                        keyword_list, &indom_oneline_dict))
        return NULL;

    if (indom_oneline_dict) {
        if (!PyDict_Check(indom_oneline_dict)) {
            __pmNotifyErr(LOG_ERR,
                "attempted to refresh indom oneline help with non-dict type");
            Py_DECREF(indom_oneline_dict);
            indom_oneline_dict = NULL;
        }
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
indom_longtext_refresh(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *keyword_list[] = {"longtext", NULL};

    if (indom_longtext_dict) {
	Py_DECREF(indom_longtext_dict);
	indom_longtext_dict = NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "O:indom_longtext_refresh",
                        keyword_list, &indom_longtext_dict))
        return NULL;

    if (indom_longtext_dict) {
        if (!PyDict_Check(indom_longtext_dict)) {
            __pmNotifyErr(LOG_ERR,
                "attempted to refresh indom long help with non-dict type");
            Py_DECREF(indom_longtext_dict);
            indom_longtext_dict = NULL;
        }
    }
    Py_INCREF(Py_None);
    return Py_None;
}

int
pmns_desc(pmID pmid, pmDesc *desc, pmdaExt *ep)
{
    if (need_refresh)
        pmns_refresh();
    return pmdaDesc(pmid, desc, ep);
}

int
pmns_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    if (need_refresh)
        pmns_refresh();
    return pmdaTreePMID(pmns, name, pmid);
}

int
pmns_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    if (need_refresh)
        pmns_refresh();
    return pmdaTreeName(pmns, pmid, nameset);
}

int
pmns_children(const char *name, int traverse, char ***kids, int **sts, pmdaExt *pmda)
{
    if (need_refresh)
        pmns_refresh();
    return pmdaTreeChildren(pmns, name, traverse, kids, sts);
}

static int
prefetch(void)
{
    PyObject *arglist, *result;

    arglist = Py_BuildValue("()");
    if (arglist == NULL)
        return -ENOMEM;
    result = PyEval_CallObject(fetch_func, arglist);
    Py_DECREF(arglist);
    if (!result) {
        PyErr_Print();
        return -EAGAIN;	/* exception thrown */
    }
    Py_DECREF(result);
    return 0;
}

static int
refresh_cluster(int cluster)
{
    PyObject *arglist, *result;

    arglist = Py_BuildValue("(i)", cluster);
    if (arglist == NULL)
        return -ENOMEM;
    result = PyEval_CallObject(refresh_func, arglist);
    Py_DECREF(arglist);
    if (result == NULL) {
        PyErr_Print();
        return -EAGAIN;	/* exception thrown */
    }
    Py_DECREF(result);
    return 0;
}

static int
refresh(int numpmid, pmID *pmidlist)
{
    size_t need;
    int *clusters = NULL;
    int i, j, count = 0;
    int sts = 0;

    /*
     * Invoke a callback once for each affected PMID cluster (and not for the
     * unaffected clusters).  This allows specific subsets of metric values to
     * be refreshed, rather than just blindly fetching everything at the start
     * of a fetch request.  Accomplish this by building an array of the unique
     * cluster numbers from the given PMID list.
     */
    need = sizeof(int) * numpmid;        /* max cluster count */
    if ((clusters = malloc(need)) == NULL)
        return -ENOMEM;
    for (i = 0; i < numpmid; i++) {
        int cluster = pmid_cluster(pmidlist[i]);
        for (j = 0; j < count; j++)
            if (clusters[j] == cluster)
                break;
        if (j == count)
            clusters[count++] = cluster;
    }
    for (j = 0; j < count; j++)
        sts |= refresh_cluster(clusters[j]);
    free(clusters);
    return sts;
}

static int
fetch(int numpmid, pmID *pmidlist, pmResult **rp, pmdaExt *pmda)
{
    int sts;

    if (need_refresh)
        pmns_refresh();
    if (fetch_func && (sts = prefetch()) < 0)
        return sts;
    if (refresh_func && (sts = refresh(numpmid, pmidlist)) < 0)
        return sts;
    return pmdaFetch(numpmid, pmidlist, rp, pmda);
}

static int
preinstance(pmInDom indom)
{
    PyObject *arglist, *result;

    arglist = Py_BuildValue("(i)", pmInDom_serial(indom));
    if (arglist == NULL)
        return -ENOMEM;
    result = PyEval_CallObject(instance_func, arglist);
    Py_DECREF(arglist);
    if (result == NULL) {
        PyErr_Print();
        return -EAGAIN;	/* exception thrown */
    }
    Py_DECREF(result);
    return 0;
}

int
instance(pmInDom indom, int a, char *b, __pmInResult **rp, pmdaExt *pmda)
{
    int sts;

    if (need_refresh)
        pmns_refresh();
    if (instance_func && (sts = preinstance(indom)) < 0)
        return sts;
    return pmdaInstance(indom, a, b, rp, pmda);
}

int
fetch_callback(pmdaMetric *metric, unsigned int inst, pmAtomValue *atom)
{
    char *s;
    int rc, sts, code;
    PyObject *arglist, *result;
    __pmID_int *pmid = (__pmID_int *)&metric->m_desc.pmid;

    if (fetch_cb_func == NULL)
	return PM_ERR_VALUE;

    arglist = Py_BuildValue("(iiI)", pmid->cluster, pmid->item, inst);
    if (arglist == NULL) {
        __pmNotifyErr(LOG_ERR, "fetch callback cannot alloc parameters");
        return -EINVAL;
    }
    result = PyEval_CallObject(fetch_cb_func, arglist);
    Py_DECREF(arglist);
    if (result == NULL) {
        PyErr_Print();
        return -EAGAIN;	/* exception thrown */
    } else if (PyTuple_Check(result)) {
        __pmNotifyErr(LOG_ERR, "non-tuple returned from fetch callback");
        Py_DECREF(result);
	return -EINVAL;
    }
    rc = code = 0;
    sts = PMDA_FETCH_STATIC;
    switch (metric->m_desc.type) {
        case PM_TYPE_32:
            rc = PyArg_Parse(result, "(ii):fetch_cb_s32", &atom->l, &code);
            break;
        case PM_TYPE_U32:
            rc = PyArg_Parse(result, "(Ii):fetch_cb_u32", &atom->ul, &code);
            break;
        case PM_TYPE_64:
            rc = PyArg_Parse(result, "(Li):fetch_cb_s64", &atom->ll, &code);
            break;
        case PM_TYPE_U64:
            rc = PyArg_Parse(result, "(Ki):fetch_cb_u64", &atom->ull, &code);
            break;
        case PM_TYPE_FLOAT:
            rc = PyArg_Parse(result, "(fi):fetch_cb_float", &atom->f, &code);
            break;
        case PM_TYPE_DOUBLE:
            rc = PyArg_Parse(result, "(di):fetch_cb_double", &atom->d, &code);
            break;
        case PM_TYPE_STRING:
            s = NULL;
            rc = PyArg_Parse(result, "(si):fetch_cb_string", &s, &code);
            if (rc == 0)
                break;
            if (s == NULL)
                sts = PM_ERR_VALUE;
            else if ((atom->cp = strdup(s)) == NULL)
                sts = -ENOMEM;
            else
                sts = PMDA_FETCH_DYNAMIC;
            break;
        default:
            __pmNotifyErr(LOG_ERR, "unsupported metric type in fetch callback");
            sts = -ENOTSUP;
    }

    if (!rc || !code) {    /* tuple not parsed or atom contains bad value */
        if (!PyArg_Parse(result, "(ii):fetch_cb_error", &sts, &code)) {
            __pmNotifyErr(LOG_ERR, "extracting error code in fetch callback");
            sts = -EINVAL;
        }
    }
    Py_DECREF(result);
    return sts;
}

int 
store_callback(__pmID_int *pmid, unsigned int inst, pmAtomValue av, int type)
{       
    int rc, code;
    int item = pmid->item;
    int cluster = pmid->cluster;
    PyObject *arglist, *result;

    switch (type) {
        case PM_TYPE_32:
            arglist = Py_BuildValue("(iiIi)", cluster, item, inst, av.l);
            break;
        case PM_TYPE_U32:
            arglist = Py_BuildValue("(iiII)", cluster, item, inst, av.ul);
            break;
        case PM_TYPE_64:
            arglist = Py_BuildValue("(iiIL)", cluster, item, inst, av.ll);
            break;
        case PM_TYPE_U64:
            arglist = Py_BuildValue("(iiIK)", cluster, item, inst, av.ull);
            break;
        case PM_TYPE_FLOAT:
            arglist = Py_BuildValue("(iiIf)", cluster, item, inst, av.f);
            break;
        case PM_TYPE_DOUBLE:
            arglist = Py_BuildValue("(iiId)", cluster, item, inst, av.d);
            break;
        case PM_TYPE_STRING:
            arglist = Py_BuildValue("(iiIs)", cluster, item, inst, av.cp);
            break;
        default:
            __pmNotifyErr(LOG_ERR, "unsupported type in store callback");
            return -EINVAL;
    }
    result = PyEval_CallObject(store_cb_func, arglist);
    Py_DECREF(arglist);
    if (!result) {
        PyErr_Print();
        return -EAGAIN;	/* exception thrown */
    }
    rc = PyArg_Parse(result, "i:store_callback", &code);
    Py_DECREF(result);
    if (rc == 0) {
        __pmNotifyErr(LOG_ERR, "store callback gave bad status (int expected)");
        return -EINVAL;
    }
    return code;
}

static pmdaMetric *
lookup_metric(__pmID_int *pmid, pmdaExt *pmda)
{
    int                i;
    pmdaMetric        *mp;

    for (i = 0; i < pmda->e_nmetrics; i++) {
        mp = &pmda->e_metrics[i];
        if (pmid->item != pmid_item(mp->m_desc.pmid))
            continue;
        if (pmid->cluster != pmid_cluster(mp->m_desc.pmid))
            continue;
        return mp;
    }
    return NULL;
}

int
store(pmResult *result, pmdaExt *pmda)
{
    int         i, j;
    int         type;
    int         sts;
    pmAtomValue av;
    pmdaMetric  *mp;
    pmValueSet  *vsp;
    __pmID_int  *pmid;

    if (need_refresh)
        pmns_refresh();

    if (store_cb_func == NULL)
	return PM_ERR_PERMISSION;

    for (i = 0; i < result->numpmid; i++) {
        vsp = result->vset[i];
        pmid = (__pmID_int *)&vsp->pmid;

        /* find the type associated with this PMID */
        if ((mp = lookup_metric(pmid, pmda)) == NULL)
            return PM_ERR_PMID;
        type = mp->m_desc.type;

        for (j = 0; j < vsp->numval; j++) {
            sts = pmExtractValue(vsp->valfmt, &vsp->vlist[j],type, &av, type);
            if (sts < 0)
                return sts;
            sts = store_callback(pmid, vsp->vlist[j].inst, av, type);
            if (sts < 0)
                return sts;
        }
    }
    return 0;
}

int
text(int ident, int type, char **buffer, pmdaExt *pmda)
{
    PyObject *dict, *value, *key;

    if (need_refresh)
        pmns_refresh();

    if ((type & PM_TEXT_PMID) != 0) {
	if ((type & PM_TEXT_ONELINE) != 0)
	    dict = pmid_oneline_dict;
	else
	    dict = pmid_longtext_dict;
    } else {
	if ((type & PM_TEXT_ONELINE) != 0)
	    dict = indom_oneline_dict;
	else
	    dict = indom_longtext_dict;
    }

    key = PyLong_FromLong((long)ident);
    if (!key)
        return PM_ERR_TEXT;
    value = PyDict_GetItem(dict, key);
    Py_DECREF(key);
    if (value == NULL)
        return PM_ERR_TEXT;
    *buffer = PyString_AsString(value);
    /* "value" is a borrowed reference, do not decrement */
    return 0;
}

int
attribute(int ctx, int attr, const char *value, int length, pmdaExt *pmda)
{
    if (pmDebug & DBG_TRACE_AUTH) {
        char buffer[256];

        if (!__pmAttrStr_r(attr, value, buffer, sizeof(buffer))) {
            __pmNotifyErr(LOG_ERR, "Bad Attribute: ctx=%d, attr=%d\n", ctx, attr);
        } else {
            buffer[sizeof(buffer)-1] = '\0';
            __pmNotifyErr(LOG_INFO, "Attribute: ctx=%d %s", ctx, buffer);
        }
    }
    /* handle connection attributes - need per-connection state code */
    return 0;
}

/*
 * Allocate a new PMDA dispatch structure and fill it
 * in for the agent we have been asked to instantiate.
 */

static inline int
pmda_generating_pmns(void) { return getenv("PCP_PYTHON_PMNS") != NULL; }

static inline int
pmda_generating_domain(void) { return getenv("PCP_PYTHON_DOMAIN") != NULL; }

static PyObject *
init_dispatch(PyObject *self, PyObject *args, PyObject *keywords)
{
    int domain;
    char *p, *name, *help, *logfile, *pmdaname;
    char *keyword_list[] = {"domain", "name", "log", "help", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "isss:init_dispatch", keyword_list,
                        &domain, &pmdaname, &logfile, &help))
        return NULL;

    name = strdup(pmdaname);
    __pmSetProgname(name);
    if ((p = getenv("PCP_PYTHON_DEBUG")) != NULL)
        if ((pmDebug = __pmParseDebug(p)) < 0)
            pmDebug = 0;

    if (access(help, R_OK) != 0) {
        pmdaDaemon(&dispatch, PMDA_INTERFACE_6, name, domain, logfile, NULL);
        dispatch.version.four.text = text;
    } else {
        p = strdup(help);
        pmdaDaemon(&dispatch, PMDA_INTERFACE_6, name, domain, logfile, p);
    }
    dispatch.version.six.fetch = fetch;
    dispatch.version.six.store = store;
    dispatch.version.six.instance = instance;
    dispatch.version.six.desc = pmns_desc;
    dispatch.version.six.pmid = pmns_pmid;
    dispatch.version.six.name = pmns_name;
    dispatch.version.six.children = pmns_children;
    dispatch.version.six.attribute = attribute;
    pmdaSetFetchCallBack(&dispatch, fetch_callback);

    if (!pmda_generating_pmns() && !pmda_generating_domain())
        pmdaOpenLog(&dispatch);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
connect_pmcd(void)
{
    /*
     * Need to mimic the same special cases handled by run() in
     * pcp/pmda.py that explicitly do NOT connect to pmcd and treat
     * these as no-ops here.
     *
     * Otherwise call pmdaConnect() to complete the PMDA's IPC
     * channel setup and complete the connection handshake with
     * pmcd.
     */
    if (!pmda_generating_pmns() && !pmda_generating_domain()) {
	/*
	 * On success pmdaConnect sets PMDA_EXT_CONNECTED in e_flags ...
	 * this used in the guard below to stop pmda_dispatch() calling
	 * pmdaConnect() again.
	 */
	pmdaConnect(&dispatch);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

#ifdef PyBUF_SIMPLE
static PyObject *
pmda_dispatch(PyObject *self, PyObject *args)
{
    int nindoms, nmetrics;
    PyObject *ibuf, *mbuf;
    pmdaMetric *metrics;
    pmdaIndom *indoms;
    Py_buffer mv, iv;

    if (!PyArg_ParseTuple(args, "OiOi", &ibuf, &nindoms, &mbuf, &nmetrics))
        return NULL;

    if (!PyObject_CheckBuffer(ibuf)) {
        PyErr_SetString(PyExc_TypeError, "pmda_dispatch expected buffer 1st arg");
        return NULL;
    }
    if (!PyObject_CheckBuffer(mbuf)) {
        PyErr_SetString(PyExc_TypeError, "pmda_dispatch expected buffer 3rd arg");
        return NULL;
    }

    if (PyObject_GetBuffer(ibuf, &iv, PyBUF_SIMPLE)) {
        PyErr_SetString(PyExc_TypeError, "pmda_dispatch failed to get indoms");
        return NULL;
    }
    if (PyObject_GetBuffer(mbuf, &mv, PyBUF_SIMPLE)) {
        PyErr_SetString(PyExc_TypeError, "pmda_dispatch failed to get metrics");
	PyBuffer_Release(&iv);
        return NULL;
    }

    if (iv.len != nindoms * sizeof(pmdaIndom)) {
        PyErr_SetString(PyExc_TypeError, "pmda_dispatch: invalid indom array");
	PyBuffer_Release(&iv);
	PyBuffer_Release(&mv);
        return NULL;
    }
    if (mv.len != nmetrics * sizeof(pmdaMetric)) {
        PyErr_SetString(PyExc_TypeError, "pmda_dispatch: invalid metric array");
	PyBuffer_Release(&iv);
	PyBuffer_Release(&mv);
        return NULL;
    }

    indoms = nindoms ? (pmdaIndom *)iv.buf : NULL;
    metrics = nmetrics ? (pmdaMetric *)mv.buf : NULL;
    if (pmDebug & DBG_TRACE_LIBPMDA)
	fprintf(stderr, "pmda_dispatch pmdaInit for metrics/indoms\n");
    pmdaInit(&dispatch, indoms, nindoms, metrics, nmetrics);
    if ((dispatch.version.any.ext->e_flags & PMDA_EXT_CONNECTED) != PMDA_EXT_CONNECTED) {
	/*
	 * connect_pmcd() not called before, so need pmdaConnect()
	 * here before falling into the PDU-driven pmdaMain() loop.
	 */
	if (pmDebug & DBG_TRACE_LIBPMDA)
	    fprintf(stderr, "pmda_dispatch connect to pmcd\n");
	pmdaConnect(&dispatch);
    }

    PyBuffer_Release(&iv);
    PyBuffer_Release(&mv);

    if (pmDebug & DBG_TRACE_LIBPMDA)
        fprintf(stderr, "pmda_dispatch entering PDU loop\n");

    pmdaMain(&dispatch);
    Py_INCREF(Py_None);
    return Py_None;
}

#else	/* old-school python */
static PyObject *
pmda_dispatch(PyObject *self, PyObject *args)
{
    int nindoms, nmetrics, size;
    PyObject *ibuf, *mbuf, *iv, *mv;
    pmdaMetric *metrics = NULL;
    pmdaIndom *indoms = NULL;

    if (!PyArg_ParseTuple(args, "OiOi", &ibuf, &nindoms, &mbuf, &nmetrics))
        return NULL;

    size = nindoms * sizeof(pmdaIndom);
    if ((iv = PyBuffer_FromObject(ibuf, 0, size)) == NULL) {
        PyErr_SetString(PyExc_TypeError, "pmda_dispatch indom extraction");
        return NULL;
    }
    if (!PyBuffer_Check(iv)) {
        PyErr_SetString(PyExc_TypeError, "pmda_dispatch wants buffer 1st arg");
        return NULL;
    }
    size = nmetrics * sizeof(pmdaMetric);
    if ((mv = PyBuffer_FromObject(mbuf, 0, size)) == NULL) {
        PyErr_SetString(PyExc_TypeError, "pmda_dispatch metric extraction");
        return NULL;
    }
    if (!PyBuffer_Check(mv)) {
        PyErr_SetString(PyExc_TypeError, "pmda_dispatch wants buffer 3rd arg");
        return NULL;
    }

    if (pmDebug & DBG_TRACE_LIBPMDA)
        fprintf(stderr, "pmda_dispatch pmdaInit for metrics/indoms\n");

    PyBuffer_Type.tp_as_buffer->bf_getreadbuffer(iv, 0, (void *)&indoms);
    PyBuffer_Type.tp_as_buffer->bf_getreadbuffer(mv, 0, (void *)&metrics);
    if (pmDebug & DBG_TRACE_LIBPMDA)
	fprintf(stderr, "pmda_dispatch pmdaInit for metrics/indoms\n");
    pmdaInit(&dispatch, indoms, nindoms, metrics, nmetrics);
    if ((dispatch.version.any.ext->e_flags & PMDA_EXT_CONNECTED) != PMDA_EXT_CONNECTED) {
	/*
	 * connect_pmcd() not called before, so need pmdaConnect()
	 * here before falling into the PDU-driven pmdaMain() loop.
	 */
	if (pmDebug & DBG_TRACE_LIBPMDA)
	    fprintf(stderr, "pmda_dispatch connect to pmcd\n");
	pmdaConnect(&dispatch);
    }

    Py_DECREF(ibuf);
    Py_DECREF(mbuf);

    if (pmDebug & DBG_TRACE_LIBPMDA)
        fprintf(stderr, "pmda_dispatch entering PDU loop\n");

    pmdaMain(&dispatch);
    Py_INCREF(Py_None);
    return Py_None;
}
#endif

static PyObject *
pmda_log(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *message;
    char *keyword_list[] = {"message", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "s:pmda_log", keyword_list, &message))
        return NULL;
    __pmNotifyErr(LOG_INFO, "%s", message);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pmda_err(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *message;
    char *keyword_list[] = {"message", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "s:pmda_err", keyword_list, &message))
        return NULL;
    __pmNotifyErr(LOG_ERR, "%s", message);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pmda_pmid(PyObject *self, PyObject *args, PyObject *keywords)
{
    int result;
    int cluster, item;
    char *keyword_list[] = {"item", "cluster", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "ii:pmda_pmid", keyword_list,
                        &item, &cluster))
        return NULL;
    result = pmid_build(dispatch.domain, item, cluster);
    return Py_BuildValue("i", result);
}

static PyObject *
pmda_indom(PyObject *self, PyObject *args, PyObject *keywords)
{
    int result;
    int serial;
    char *keyword_list[] = {"serial", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "i:pmda_indom", keyword_list, &serial))
        return NULL;
    result = pmInDom_build(dispatch.domain, serial);
    return Py_BuildValue("i", result);
}

static PyObject *
pmda_units(PyObject *self, PyObject *args, PyObject *keywords)
{
    int result;
    int dim_time, dim_space, dim_count;
    int scale_space, scale_time, scale_count;
    char *keyword_list[] = {"dim_time", "dim_space", "dim_count",
                        "scale_space", "scale_time", "scale_count", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "iiiiii:pmda_units", keyword_list,
                        &dim_time, &dim_space, &dim_count,
                        &scale_space, &scale_time, &scale_count))
        return NULL;
    {
        pmUnits units = PMDA_PMUNITS(dim_time, dim_space, dim_count,
                                        scale_space, scale_time, scale_count);
        result = *(int *)&units;
    }
    return Py_BuildValue("i", result);
}

static PyObject *
pmda_uptime(PyObject *self, PyObject *args, PyObject *keywords)
{
    static char s[32];
    size_t sz = sizeof(s);
    int now, days, hours, mins, secs;
    char *keyword_list[] = {"seconds", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "i:pmda_uptime", keyword_list, &now))
        return NULL;
    
    days = now / (60 * 60 * 24);
    now %= (60 * 60 * 24);
    hours = now / (60 * 60);
    now %= (60 * 60);
    mins = now / 60;
    now %= 60;
    secs = now;

    if (days > 1)
        snprintf(s, sz, "%ddays %02d:%02d:%02d", days, hours, mins, secs);
    else if (days == 1)
        snprintf(s, sz, "%dday %02d:%02d:%02d", days, hours, mins, secs);
    else
        snprintf(s, sz, "%02d:%02d:%02d", hours, mins, secs);

    return Py_BuildValue("s", s);
}

static PyObject *
set_need_refresh(PyObject *self, PyObject *args)
{
    need_refresh = 1;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
set_callback(PyObject *self, PyObject *args, char *params, PyObject **callback)
{
    PyObject *func;

    if (!PyArg_ParseTuple(args, params, &func))
        return NULL;
    if (!PyCallable_Check(func)) {
        PyErr_SetString(PyExc_TypeError, "parameter must be callable");
        return NULL;
    }
    Py_XINCREF(func);
    Py_XDECREF(*callback);
    *callback = func;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
set_fetch(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_fetch", &fetch_func);
}

static PyObject *
set_refresh(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_refresh", &refresh_func);
}

static PyObject *
set_instance(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_instance", &instance_func);
}

static PyObject *
set_store_callback(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_store_callback", &store_cb_func);
}

static PyObject *
set_fetch_callback(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_fetch_callback", &fetch_cb_func);
}


static PyMethodDef methods[] = {
    { .ml_name = "pmda_pmid", .ml_meth = (PyCFunction)pmda_pmid,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_indom", .ml_meth = (PyCFunction)pmda_indom,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_units", .ml_meth = (PyCFunction)pmda_units,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_uptime", .ml_meth = (PyCFunction)pmda_uptime,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "init_dispatch", .ml_meth = (PyCFunction)init_dispatch,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_dispatch", .ml_meth = (PyCFunction)pmda_dispatch,
        .ml_flags = METH_VARARGS },
    { .ml_name = "connect_pmcd", .ml_meth = (PyCFunction)connect_pmcd,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmns_refresh", .ml_meth = (PyCFunction)namespace_refresh,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmid_oneline_refresh",
        .ml_meth = (PyCFunction)pmid_oneline_refresh,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmid_longtext_refresh",
        .ml_meth = (PyCFunction)pmid_longtext_refresh,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "indom_oneline_refresh",
        .ml_meth = (PyCFunction)indom_oneline_refresh,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "indom_longtext_refresh",
        .ml_meth = (PyCFunction)indom_longtext_refresh,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_need_refresh", .ml_meth = (PyCFunction)set_need_refresh,
        .ml_flags = METH_NOARGS },
    { .ml_name = "set_fetch", .ml_meth = (PyCFunction)set_fetch,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_refresh", .ml_meth = (PyCFunction)set_refresh,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_instance", .ml_meth = (PyCFunction)set_instance,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_store_callback", .ml_meth = (PyCFunction)set_store_callback,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_fetch_callback", .ml_meth = (PyCFunction)set_fetch_callback,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_log", .ml_meth = (PyCFunction)pmda_log,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_err", .ml_meth = (PyCFunction)pmda_err,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { NULL },
};

static void
pmda_dict_add(PyObject *dict, char *sym, long val)
{
    PyObject *pyVal = PyInt_FromLong(val);

    PyDict_SetItemString(dict, sym, pyVal);
    Py_XDECREF(pyVal);
}

/* called when the module is initialized. */ 
void
initcpmda(void)
{
    PyObject *module, *dict;

    module = Py_InitModule("cpmda", methods);
    dict = PyModule_GetDict(module);

    /* pmda.h - fetch callback return codes */
    pmda_dict_add(dict, "PMDA_FETCH_NOVALUES", PMDA_FETCH_NOVALUES);
    pmda_dict_add(dict, "PMDA_FETCH_STATIC", PMDA_FETCH_STATIC);
    pmda_dict_add(dict, "PMDA_FETCH_DYNAMIC", PMDA_FETCH_DYNAMIC);

    /* pmda.h - indom cache operation codes */
    pmda_dict_add(dict, "PMDA_CACHE_LOAD", PMDA_CACHE_LOAD);
    pmda_dict_add(dict, "PMDA_CACHE_ADD", PMDA_CACHE_ADD);
    pmda_dict_add(dict, "PMDA_CACHE_HIDE", PMDA_CACHE_HIDE);
    pmda_dict_add(dict, "PMDA_CACHE_CULL", PMDA_CACHE_CULL);
    pmda_dict_add(dict, "PMDA_CACHE_EMPTY", PMDA_CACHE_EMPTY);
    pmda_dict_add(dict, "PMDA_CACHE_SAVE", PMDA_CACHE_SAVE);
    pmda_dict_add(dict, "PMDA_CACHE_ACTIVE", PMDA_CACHE_ACTIVE);
    pmda_dict_add(dict, "PMDA_CACHE_INACTIVE", PMDA_CACHE_INACTIVE);
    pmda_dict_add(dict, "PMDA_CACHE_SIZE", PMDA_CACHE_SIZE);
    pmda_dict_add(dict, "PMDA_CACHE_SIZE_ACTIVE", PMDA_CACHE_SIZE_ACTIVE);
    pmda_dict_add(dict, "PMDA_CACHE_SIZE_INACTIVE", PMDA_CACHE_SIZE_INACTIVE);
    pmda_dict_add(dict, "PMDA_CACHE_REUSE", PMDA_CACHE_REUSE);
    pmda_dict_add(dict, "PMDA_CACHE_WALK_REWIND", PMDA_CACHE_WALK_REWIND);
    pmda_dict_add(dict, "PMDA_CACHE_WALK_NEXT", PMDA_CACHE_WALK_NEXT);
    pmda_dict_add(dict, "PMDA_CACHE_CHECK", PMDA_CACHE_CHECK);
    pmda_dict_add(dict, "PMDA_CACHE_REORG", PMDA_CACHE_REORG);
    pmda_dict_add(dict, "PMDA_CACHE_SYNC", PMDA_CACHE_SYNC);
    pmda_dict_add(dict, "PMDA_CACHE_DUMP", PMDA_CACHE_DUMP);
    pmda_dict_add(dict, "PMDA_CACHE_DUMP_ALL", PMDA_CACHE_DUMP_ALL);
}
