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
static PyObject *need_refresh;
static PyObject *fetch_func;
//static PyObject *instance_func;

static void
pmns_refresh(void)
{
    int sts, count = 0;
    PyObject *iterator, *item;

    if (pmns)
        __pmFreePMNS(pmns);

    if ((sts = __pmNewPMNS(&pmns)) < 0) {
        __pmNotifyErr(LOG_ERR, "failed to create namespace root: %s",
                      pmErrStr(sts));
        return;
    }

    if ((iterator = PyObject_GetIter(need_refresh)) == NULL) {
        __pmNotifyErr(LOG_ERR, "failed to create metric iterator");
        return;
    }
    while ((item = PyIter_Next(iterator)) != NULL) {
        const char *name;
        long pmid;

        if (!PyTuple_Check(item) || PyTuple_GET_SIZE(item) != 2) {
            __pmNotifyErr(LOG_ERR, "method iterator not findind 2-tuples");
            continue;
        }
        pmid = PyLong_AsLong(PyTuple_GET_ITEM(item, 0));
        name = PyString_AsString(PyTuple_GET_ITEM(item, 1));
        if ((sts = __pmAddPMNSNode(pmns, pmid, name)) < 0) {
            __pmNotifyErr(LOG_ERR,
                    "failed to add metric %s(%s) to namespace: %s",
                    name, pmIDStr(pmid), pmErrStr(sts));
        } else {
            count++;
        }
        Py_DECREF(item);
    }
    Py_DECREF(iterator);

    pmdaTreeRebuildHash(pmns, count); /* for reverse (pmid->name) lookups */
    Py_DECREF(need_refresh);
    need_refresh = NULL;
}

static PyObject *
namespace_refresh(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *keyword_list[] = {"metrics", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "O:namespace_refresh", keyword_list, &need_refresh))
        return NULL;
    if (need_refresh)
        pmns_refresh();
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

static void
prefetch(void)
{
    // TODO: call python fetch_func
}

static int
fetch(int numpmid, pmID *pmidlist, pmResult **rp, pmdaExt *pmda)
{
    if (need_refresh)
        pmns_refresh();
    if (fetch_func)
        prefetch();
    return pmdaFetch(numpmid, pmidlist, rp, pmda);
}

#if 0
static void
preinstance(indom)
{
    // TODO: call python instance_func
}
#endif

int
instance(pmInDom indom, int a, char *b, __pmInResult **rp, pmdaExt *pmda)
{
    if (need_refresh)
        pmns_refresh();
//  if (instance_func)
//      preinstance(instance_index(indom));
    return pmdaInstance(indom, a, b, rp, pmda);
}

int
text(int ident, int type, char **buffer, pmdaExt *pmda)
{
    // TODO: call python text_func
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

static void
init_dispatch(int domain, char *name, char *logfile, char *helpfile)
{
    char *p;

    __pmSetProgname(name);
    if ((p = getenv("PCP_PYTHON_DEBUG")) != NULL)
        if ((pmDebug = __pmParseDebug(p)) < 0)
            pmDebug = 0;

    if (access(helpfile, R_OK) != 0) {
        pmdaDaemon(&dispatch, PMDA_INTERFACE_5, name, domain, logfile, NULL);
        dispatch.version.four.text = text;
    } else {
        char *help = strdup(helpfile);
        pmdaDaemon(&dispatch, PMDA_INTERFACE_5, name, domain, logfile, help);
    }
    dispatch.version.four.fetch = fetch;
    dispatch.version.four.instance = instance;
    dispatch.version.four.desc = pmns_desc;
    dispatch.version.four.pmid = pmns_pmid;
    dispatch.version.four.name = pmns_name;
    dispatch.version.four.children = pmns_children;

    if (!pmda_generating_pmns() && !pmda_generating_domain())
        pmdaOpenLog(&dispatch);
}

static PyObject *
pmda_dispatch(PyObject *self, PyObject *args, PyObject *keywords)
{
    int domain;
    char *name, *help, *logfile;
    char *keyword_list[] = {"domain", "name", "log", "help", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "isss:pmda_dispatch", keyword_list,
                        &domain, &name, &logfile, &help))
        return NULL;

    init_dispatch(domain, name, logfile, help);
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
    result = PMDA_PMID(item, cluster);
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
get_need_refresh(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", (need_refresh == NULL));
}

static PyObject *
set_need_refresh(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *keyword_list[] = {"metrics", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "O:set_need_refresh", keyword_list, &need_refresh))
        return NULL;
    Py_INCREF(need_refresh);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef methods[] = {
    { .ml_name = "pmda_pmid", .ml_meth = (PyCFunction)pmda_pmid,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_units", .ml_meth = (PyCFunction)pmda_units,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_dispatch", .ml_meth = (PyCFunction)pmda_dispatch,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmns_refresh", .ml_meth = (PyCFunction)namespace_refresh,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "need_refresh", .ml_meth = (PyCFunction)get_need_refresh,
        .ml_flags = METH_NOARGS },
    { .ml_name = "set_need_refresh", .ml_meth = (PyCFunction)set_need_refresh,
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
