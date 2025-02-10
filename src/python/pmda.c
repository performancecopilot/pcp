/*
 * Copyright (C) 2013-2015,2017-2021,2024 Red Hat.
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

/*
 * This C extension module mainly serves the purpose of loading functions
 * and macros needed to implement PMDAs in python.  These are exported to
 * python PMDAs via the pmda.py module, using ctypes.
 */
#define PY_SSIZE_T_CLEAN
#define _FILE_OFFSET_BITS 64
#define _TIME_BITS 64
#include <Python.h>
#include <pcp/pmapi.h>
#include <pcp/pmda.h>

/* libpcp internal routines used by this module */
PCP_CALL extern int __pmAddLabels(pmLabelSet **, const char *, int);

#if PY_MAJOR_VERSION >= 3
#define MOD_ERROR_VAL NULL
#define MOD_SUCCESS_VAL(val) val
#define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
#define MOD_DEF(ob, name, doc, methods) \
	static struct PyModuleDef moduledef = { \
	  PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
	ob = PyModule_Create(&moduledef);
#else
#define MOD_ERROR_VAL
#define MOD_SUCCESS_VAL(val)
#define MOD_INIT(name) void init##name(void)
#define MOD_DEF(ob, name, doc, methods) \
	ob = Py_InitModule3(name, methods, doc);
#endif

static pmdaInterface dispatch;
static pmdaNameSpace *pmns;
static int need_refresh;
static char *helptext_file;
static PyObject *indom_list;	  	/* indom list */
static PyObject *metric_list;	  	/* metric list */
static PyObject *pmns_dict;		/* metric pmid:names dictionary */
static PyObject *pmid_oneline_dict;	/* metric pmid:short text */
static PyObject *pmid_longtext_dict;	/* metric pmid:long help */
static PyObject *indom_oneline_dict;	/* indom pmid:short text */
static PyObject *indom_longtext_dict;	/* indom pmid:long help */

static PyObject *fetch_func;
static PyObject *label_func;
static PyObject *notes_func;
static PyObject *refresh_func;
static PyObject *instance_func;
static PyObject *store_cb_func;
static PyObject *fetch_cb_func;
static PyObject *label_cb_func;
static PyObject *notes_cb_func;
static PyObject *attribute_cb_func;
static PyObject *endcontext_cb_func;
static PyObject *refresh_all_func;
static PyObject *refresh_metrics_func;

static PyThreadState *thread_state;

static Py_ssize_t nindoms;
static pmdaIndom *indom_buffer;
static Py_ssize_t nmetrics;
static pmdaMetric *metric_buffer;

#if PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION <= 5
typedef int Py_ssize_t;
#endif

static void pmns_refresh(void);
static void pmda_refresh_metrics(void);

static void
refresh_if_needed(void)
{
    if (need_refresh) {
        pmns_refresh();
        pmda_refresh_metrics();
        need_refresh = 0;
    }
}

static void
maybe_refresh_all(void)
{
    /* Call the refresh metrics hook (if it exists). */
    if (refresh_metrics_func) {
	PyObject *arglist, *result;

	arglist = Py_BuildValue("()");
	if (arglist == NULL)
	    return;
	result = PyObject_Call(refresh_metrics_func, arglist, NULL);
	Py_DECREF(arglist);
	if (result == NULL)
	    PyErr_Print();
	else
	    /* Just ignore the result. */
	    Py_DECREF(result);
    }

    refresh_if_needed();
}

static void
pmns_refresh(void)
{
    int sts, count = 0;
    Py_ssize_t pos = 0;
    PyObject *key, *value;

    if (pmDebugOptions.libpmda)
	fprintf(stderr, "pmns_refresh: rebuilding namespace\n");

    /* If there is nothing to do, just exit. */
    if (pmns_dict == NULL)
	return;

    if (pmns)
	pmdaTreeRelease(pmns);

    if ((sts = pmdaTreeCreate(&pmns)) < 0) {
	pmNotifyErr(LOG_ERR, "failed to create namespace root: %s",
		      pmErrStr(sts));
	return;
    }

    while (PyDict_Next(pmns_dict, &pos, &key, &value)) {
	const char *name;
	long pmid;

	pmid = PyLong_AsLong(key);
#if PY_MAJOR_VERSION >= 3
	name = PyUnicode_AsUTF8(value);
#else
	name = PyString_AsString(value);
#endif
	if (pmDebugOptions.libpmda)
	    fprintf(stderr, "pmns_refresh: adding metric %s(%s)\n",
		    name, pmIDStr(pmid));
	if ((sts = pmdaTreeInsert(pmns, pmid, name)) < 0) {
	    pmNotifyErr(LOG_ERR,
		    "failed to add metric %s(%s) to namespace: %s",
		    name, pmIDStr(pmid), pmErrStr(sts));
	} else {
	    count++;
	}
    }

    pmdaTreeRebuildHash(pmns, count); /* for reverse (pmid->name) lookups */
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
	/* PyArg_ParseTupleAndKeywords() returns a "borrowed" reference.
	 * Since we're going to keep this object around for use later,
	 * increase its reference count.
	 */
	Py_INCREF(pmns_dict);

	if (!PyDict_Check(pmns_dict)) {
	    pmNotifyErr(LOG_ERR,
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
	/* PyArg_ParseTupleAndKeywords() returns a "borrowed" reference.
	 * Since we're going to keep this object around for use later,
	 * increase its reference count.
	 */
	Py_INCREF(pmid_oneline_dict);

	if (!PyDict_Check(pmid_oneline_dict)) {
	    pmNotifyErr(LOG_ERR,
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
	/* PyArg_ParseTupleAndKeywords() returns a "borrowed" reference.
	 * Since we're going to keep this object around for use later,
	 * increase its reference count.
	 */
	Py_INCREF(pmid_longtext_dict);

	if (!PyDict_Check(pmid_longtext_dict)) {
	    pmNotifyErr(LOG_ERR,
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
	/* PyArg_ParseTupleAndKeywords() returns a "borrowed" reference.
	 * Since we're going to keep this object around for use later,
	 * increase its reference count.
	 */
	Py_INCREF(indom_oneline_dict);

	if (!PyDict_Check(indom_oneline_dict)) {
	    pmNotifyErr(LOG_ERR,
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
	/* PyArg_ParseTupleAndKeywords() returns a "borrowed" reference.
	 * Since we're going to keep this object around for use later,
	 * increase its reference count.
	 */
	Py_INCREF(indom_longtext_dict);

	if (!PyDict_Check(indom_longtext_dict)) {
	    pmNotifyErr(LOG_ERR,
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
    maybe_refresh_all();
    return pmdaDesc(pmid, desc, ep);
}

int
pmns_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    maybe_refresh_all();
    return pmdaTreePMID(pmns, name, pmid);
}

int
pmns_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    maybe_refresh_all();
    return pmdaTreeName(pmns, pmid, nameset);
}

int
pmns_children(const char *name, int traverse, char ***kids, int **sts, pmdaExt *pmda)
{
    maybe_refresh_all();
    return pmdaTreeChildren(pmns, name, traverse, kids, sts);
}

static int
callback_error(const char *name)
{
    pmNotifyErr(LOG_ERR, "%s: callback failed", name);
    /* force the stacktrace out now if there is one */
    if (PyErr_Occurred())
	PyErr_Print();
    return -EAGAIN;
}

static int
prefetch(void)
{
    PyObject *arglist, *result;

    arglist = Py_BuildValue("()");
    if (arglist == NULL)
	return -ENOMEM;
    result = PyObject_Call(fetch_func, arglist, NULL);
    Py_DECREF(arglist);
    if (result == NULL)
	return callback_error("prefetch");
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
    result = PyObject_Call(refresh_func, arglist, NULL);
    Py_DECREF(arglist);
    if (result == NULL)
	return callback_error("refresh_cluster");
    Py_DECREF(result);
    return 0;
}

static int
refresh_all_clusters(int numclusters, int *clusters)
{
    PyObject *arglist, *result, *list;
    int i;

    list = PyList_New(numclusters);
    if (list == NULL){
	pmNotifyErr(LOG_ERR, "refresh: Unable to allocate memory");
	return 1;
    }
    for (i = 0; i < numclusters; i++) {
	PyObject *num = PyLong_FromLong(clusters[i]);
	PyList_SET_ITEM(list, i, num);
    }

    arglist = Py_BuildValue("(N)", list);
    if (arglist == NULL)
	return -ENOMEM;
    result = PyObject_Call(refresh_all_func, arglist, NULL);
    Py_DECREF(list);
    Py_DECREF(arglist);
    if (result == NULL)
	return callback_error("refresh_all_clusters");
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
    need = sizeof(int) * numpmid;	/* max cluster count */
    if ((clusters = malloc(need)) == NULL)
	return -ENOMEM;
    for (i = 0; i < numpmid; i++) {
	int cluster = pmID_cluster(pmidlist[i]);
	for (j = 0; j < count; j++)
	    if (clusters[j] == cluster)
		break;
	if (j == count)
	    clusters[count++] = cluster;
    }
    if (refresh_all_func)
	sts |= refresh_all_clusters(count, clusters);
    if (refresh_func) {
	for (j = 0; j < count; j++)
	    sts |= refresh_cluster(clusters[j]);
    }
    free(clusters);

    // if the refresh() method of the PMDA sets the need_refresh flag
    // (e.g. because the instances got changed), update the metrics and
    // indom buffers
    refresh_if_needed();
    return sts;
}

static int
fetch(int numpmid, pmID *pmidlist, pmResult **rp, pmdaExt *pmda)
{
    int sts;

    maybe_refresh_all();
    if (fetch_func && (sts = prefetch()) < 0)
	return sts;
    if ((refresh_func || refresh_all_func) &&
	(sts = refresh(numpmid, pmidlist)) < 0)
	return sts;
    return pmdaFetch(numpmid, pmidlist, rp, pmda);
}

/*
 * The empty label set allows python PMDAs to return only a
 * string for their label callbacks (as opposed to both the
 * number of labels and the labels themselves, as the lower
 * level APIs do).
 */
static inline int
empty_labelset(const char *set)
{
    if (strlen(set) < 2)
	return 1;
    return (strncmp(set, "{}", 2) == 0);
}

static int
label(int ident, int type, pmLabelSet **lp, pmdaExt *ep)
{
    int id, sts = 0;
    char *s = NULL;

    if (label_func || notes_func) {
	PyObject *arglist, *label_result, *notes_result;

	id = (type == PM_LABEL_CLUSTER) ? (int)pmID_cluster(ident) : ident;

	arglist = Py_BuildValue("(ii)", id, type);
	if (arglist == NULL)
	    return -ENOMEM;
	if (label_func)
	   label_result = PyObject_Call(label_func, arglist, NULL);
	else
	   label_result = NULL;
	if (notes_func)
	   notes_result = PyObject_Call(notes_func, arglist, NULL);
	else
	   notes_result = NULL;
	Py_DECREF(arglist);

	if (label_func) {
	    if (!label_result) {
		PyErr_Print();
		return -EAGAIN;
	    }

	    if (PyArg_Parse(label_result, "s:label", &s) == 0 || s == NULL) {
		pmNotifyErr(LOG_ERR, "bad labels result (expected string)");
		Py_DECREF(label_result);
		return -EINVAL;
	    }

	    if (!empty_labelset(s) && (sts = __pmAddLabels(lp, s, type)) < 0)
	        pmNotifyErr(LOG_ERR, "__pmAddLabels failed: %s - %s", s, pmErrStr(sts));

	    Py_DECREF(label_result);
	}

	if (notes_func) {
	    if (!notes_result) {
		PyErr_Print();
		return -EAGAIN;
	    }

	    if (PyArg_Parse(notes_result, "s:notes", &s) == 0 || s == NULL) {
		pmNotifyErr(LOG_ERR, "bad notes result (expected string)");
		Py_DECREF(notes_result);
		return -EINVAL;
	    }

	    type |= PM_LABEL_OPTIONAL;
	    if (!empty_labelset(s) && (sts = __pmAddLabels(lp, s, type)) < 0)
	        pmNotifyErr(LOG_ERR, "__pmAddLabels (optional) failed: %s - %s", s, pmErrStr(sts));

	    Py_DECREF(notes_result);
	}
    }

    if (sts < 0)
	return sts;

    return pmdaLabel(ident, type, lp, ep);
}

static int
preinstance(pmInDom indom)
{
    PyObject *arglist, *result;

    arglist = Py_BuildValue("(i)", pmInDom_serial(indom));
    if (arglist == NULL)
	return -ENOMEM;
    result = PyObject_Call(instance_func, arglist, NULL);
    Py_DECREF(arglist);
    if (result == NULL)
	return callback_error("preinstance");
    Py_DECREF(result);
    return 0;
}

static int
instance(pmInDom indom, int a, char *b, pmInResult **rp, pmdaExt *pmda)
{
    int sts;

    maybe_refresh_all();
    if (instance_func && (sts = preinstance(indom)) < 0)
	return sts;
    return pmdaInstance(indom, a, b, rp, pmda);
}

static int
fetch_callback(pmdaMetric *metric, unsigned int inst, pmAtomValue *atom)
{
    char *s;
    int rc, sts, code;
    PyObject *arglist, *result;
    unsigned int item = pmID_item(metric->m_desc.pmid);
    unsigned int cluster = pmID_cluster(metric->m_desc.pmid);

    if (fetch_cb_func == NULL)
	return PM_ERR_VALUE;

    arglist = Py_BuildValue("(iiI)", cluster, item, inst);
    if (arglist == NULL) {
	pmNotifyErr(LOG_ERR, "fetch callback cannot alloc parameters");
	return -EINVAL;
    }
    result = PyObject_Call(fetch_cb_func, arglist, NULL);
    Py_DECREF(arglist);
    if (result == NULL)
	return callback_error("fetch_callback");
    else if (result == Py_None || PyTuple_Check(result)) {
	/* non-tuple returned from fetch callback, e.g. None - no values */
	Py_DECREF(result);
	return PMDA_FETCH_NOVALUES;
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
	    pmNotifyErr(LOG_ERR, "unsupported metric type in fetch callback");
	    sts = -ENOTSUP;
	    rc = code = 1;		/* Don't fall into code below. */
	    break;
    }

    if (!rc || !code) {    /* tuple not parsed or atom contains bad value */
	/* If PyArg_Parse() failed above, it could be because the
	 * calling code returned a error code tuple that didn't look
	 * like what we were looking for. An error code tuple looks
	 * like '[PM_ERR_VALUE, 0]' (for example). In this case, we
	 * don't want PyArg_Parse() to raise an error, so we'll clear
	 * it out. */
	PyErr_Clear();

	if (!PyArg_Parse(result, "(ii):fetch_cb_error", &sts, &code)) {
	    pmNotifyErr(LOG_ERR, "extracting error code in fetch callback");
	    sts = -EINVAL;
	}
	/* If we got a code of 1, that's means the fetch
	 * worked. However, if we're here, the fetch didn't really
	 * work. For example, we could have been expecting a string
	 * value and instead got a numeric value. So, force an
	 * error. */
	else if (code == 1) {
	    pmNotifyErr(LOG_ERR, "forcing error code in fetch callback");
	    sts = PM_ERR_TYPE;
	}
    }
    Py_DECREF(result);
    return sts;
}

static int
label_callback(pmInDom indom, unsigned int inst, pmLabelSet **lp)
{
    int sts = 0;
    int type = PM_LABEL_INSTANCES;
    char *s = NULL;
    PyObject *arglist, *label_result, *notes_result;

    if (label_cb_func == NULL && notes_cb_func == NULL)
	return PM_ERR_VALUE;

    arglist = Py_BuildValue("(II)", indom, inst);
    if (arglist == NULL) {
	pmNotifyErr(LOG_ERR, "label callback cannot alloc parameters");
	return -EINVAL;
    }
    if (label_cb_func)
	label_result = PyObject_Call(label_cb_func, arglist, NULL);
    else
	label_result = NULL;
    if (notes_cb_func)
	notes_result = PyObject_Call(notes_cb_func, arglist, NULL);
    else
	notes_result = NULL;
    Py_DECREF(arglist);

    if (label_cb_func) {
	if (label_result == NULL) {
	    PyErr_Print();
	    return -EAGAIN; /* exception thrown */
	}
	if (PyArg_Parse(label_result, "s:label_callback", &s) == 0 || !s) {
	    pmNotifyErr(LOG_ERR, "bad label callback result (expected string)");
	    Py_DECREF(label_result);
	    return -EINVAL;
	}

	if (!empty_labelset(s) && (sts = __pmAddLabels(lp, s, type)) < 0)
	    pmNotifyErr(LOG_ERR, "__pmAddLabels failed: %s - %s", s, pmErrStr(sts));

	Py_DECREF(label_result);
    }

    if (sts >= 0 && notes_cb_func) {
	if (notes_result == NULL) {
	    PyErr_Print();
	    return -EAGAIN; /* exception thrown */
	}
	if (PyArg_Parse(notes_result, "s:notes_callback", &s) == 0 || !s) {
	    pmNotifyErr(LOG_ERR, "bad notes callback result (expected string)");
	    Py_DECREF(notes_result);
	    return -EINVAL;
	}

	type |= PM_LABEL_OPTIONAL;
	if (!empty_labelset(s) && (sts = __pmAddLabels(lp, s, type)) < 0)
	    pmNotifyErr(LOG_ERR, "__pmAddLabels (optional) failed: %s - %s", s, pmErrStr(sts));

	Py_DECREF(notes_result);
    }

    return sts;
}

static int
store_callback(pmID pmid, unsigned int inst, pmAtomValue av, int type)
{
    int rc, code;
    int item = pmID_item(pmid);
    int cluster = pmID_cluster(pmid);
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
	    pmNotifyErr(LOG_ERR, "unsupported type in store callback");
	    return -EINVAL;
    }
    result = PyObject_Call(store_cb_func, arglist, NULL);
    Py_DECREF(arglist);
    if (result == NULL)
	return callback_error("store_callback");
    rc = PyArg_Parse(result, "i:store_callback", &code);
    Py_DECREF(result);
    if (rc == 0) {
	pmNotifyErr(LOG_ERR, "store callback gave bad status (int expected)");
	return -EINVAL;
    }
    return code;
}

static pmdaMetric *
lookup_metric(pmID pmid, pmdaExt *pmda)
{
    int		i;
    pmdaMetric	*mp;
    unsigned int item = pmID_item(pmid);
    unsigned int cluster = pmID_cluster(pmid);

    for (i = 0; i < pmda->e_nmetrics; i++) {
	mp = &pmda->e_metrics[i];
	if (item != pmID_item(mp->m_desc.pmid))
	    continue;
	if (cluster != pmID_cluster(mp->m_desc.pmid))
	    continue;
	return mp;
    }
    return NULL;
}

static int
store(pmResult *result, pmdaExt *pmda)
{
    int		i, j, sts, type;
    pmAtomValue	av;
    pmdaMetric  *mp;
    pmValueSet  *vsp;

    maybe_refresh_all();

    if (store_cb_func == NULL)
	return PM_ERR_PERMISSION;

    pmdaStore(result, pmda);

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];

	/* find the type associated with this PMID */
	if ((mp = lookup_metric(vsp->pmid, pmda)) == NULL)
	    return PM_ERR_PMID;
	type = mp->m_desc.type;

	for (j = 0; j < vsp->numval; j++) {
	    sts = pmExtractValue(vsp->valfmt, &vsp->vlist[j],type, &av, type);
	    if (sts < 0)
		return sts;
	    sts = store_callback(vsp->pmid, vsp->vlist[j].inst, av, type);
	    if (sts < 0)
		return sts;
	}
    }
    return 0;
}

static int
text(int ident, int type, char **buffer, pmdaExt *pmda)
{
    PyObject *dict, *value, *key;

    maybe_refresh_all();

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
#if PY_MAJOR_VERSION >= 3
    *buffer = (char *)PyUnicode_AsUTF8(value);
#else
    *buffer = (char *)PyString_AsString(value);
#endif
    /* "value" is a borrowed reference, do not decrement */
    return 0;
}

static int
attribute(int ctx, int attr, const char *value, int length, pmdaExt *pmda)
{
    PyObject *arglist, *result;
    int	sts;

    if ((sts = pmdaAttribute(ctx, attr, value, length, pmda)) < 0)
        return sts;

    if (attribute_cb_func == NULL)
        return 0;

    // 'length' parameter includes the terminating NULL byte here
    arglist = Py_BuildValue("(iis#)", ctx, attr, value, (Py_ssize_t)length-1);
    if (arglist == NULL)
        return -ENOMEM;

    result = PyObject_Call(attribute_cb_func, arglist, NULL);
    Py_DECREF(arglist);
    if (result == NULL)
        return callback_error("attribute");

    Py_DECREF(result);
    return 0;
}

static void
endContextCallBack(int ctx)
{
    PyObject *arglist, *result;

    if (endcontext_cb_func == NULL)
        return;

    arglist = Py_BuildValue("(i)", ctx);
    if (arglist == NULL)
        return;

    result = PyObject_Call(endcontext_cb_func, arglist, NULL);
    Py_DECREF(arglist);
    if (result == NULL) {
        callback_error("endcontext");
        return;
    }

    Py_DECREF(result);
}

/*
 * Allocate a new PMDA dispatch structure and fill it
 * in for the agent we have been asked to instantiate.
 */

static inline int
pmda_generating_pmns(void) { return getenv("PCP_PYTHON_PMNS") != NULL; }

static inline int
pmda_probing_domain(void) { return getenv("PCP_PYTHON_PROBE") != NULL; }

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
    pmSetProgname(name);
    if ((p = getenv("PCP_PYTHON_DEBUG")) != NULL)
	if (pmSetDebug(p) < 0)
	    PyErr_SetString(PyExc_TypeError, "unrecognized debug options specification");

    if (access(help, R_OK) != 0) {
	pmdaDaemon(&dispatch, PMDA_INTERFACE_7, name, domain, logfile, NULL);
	dispatch.version.four.text = text;
    } else {
	if (helptext_file)
	    free(helptext_file);
	helptext_file = p = strdup(help);	/* permanent reference */
	pmdaDaemon(&dispatch, PMDA_INTERFACE_7, name, domain, logfile, p);
    }

    dispatch.version.seven.fetch = fetch;
    dispatch.version.seven.store = store;
    dispatch.version.seven.instance = instance;
    dispatch.version.seven.desc = pmns_desc;
    dispatch.version.seven.pmid = pmns_pmid;
    dispatch.version.seven.name = pmns_name;
    dispatch.version.seven.children = pmns_children;
    dispatch.version.seven.attribute = attribute;
    dispatch.version.seven.label = label;
    pmdaSetLabelCallBack(&dispatch, label_callback);
    pmdaSetFetchCallBack(&dispatch, fetch_callback);
    pmdaSetEndContextCallBack(&dispatch, endContextCallBack);

    if (!pmda_generating_pmns() && !pmda_generating_domain() && !pmda_probing_domain())
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
    if (!pmda_generating_pmns() && !pmda_generating_domain() && !pmda_probing_domain()) {
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

static PyObject *
pmda_notready(void)
{
    pmdaSendError(&dispatch, PM_ERR_PMDANOTREADY);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pmda_ready(void)
{
    pmdaSendError(&dispatch, PM_ERR_PMDAREADY);
    Py_INCREF(Py_None);
    return Py_None;
}

static int
update_indom_metric_buffers(void)
{
    Py_ssize_t i;
    PyObject *item;
    Py_buffer buffer;
    const void *ptr;
    Py_ssize_t len;
    int error = 0;

    if (indom_list == NULL || metric_list == NULL)
	return 1;

    /* If we have old data, free it up. We have to keep it around
     * since pmdaRehash() doesn't copy data, it just points to it.
     */
    if (indom_buffer) {
	free(indom_buffer);
	indom_buffer = NULL;
    }
    if (metric_buffer) {
	free(metric_buffer);
	metric_buffer = NULL;
    }

    /* Figure out how many indoms/metrics we've got. */
    nindoms = PyList_Size(indom_list);
    nmetrics = PyList_Size(metric_list);

    /* Allocate buffers to hold all the indoms/metrics. */
    indom_buffer = nindoms ? calloc(nindoms, sizeof(pmdaIndom)) : NULL;
    metric_buffer = nmetrics ? calloc(nmetrics, sizeof(pmdaMetric)) : NULL;
    if ((nindoms > 0 && indom_buffer == NULL)
	|| (nmetrics > 0 && metric_buffer == NULL)) {
	PyErr_SetString(PyExc_TypeError, "Unable to allocate memory");
	error = 1;
    }

    /* Copy the indoms. */
    for (i = 0; !error && i < nindoms; i++) {
	item = PyList_GetItem(indom_list, i);
#if defined(ENABLE_PYTHON3)
	/* Newer buffer interface */
	if (item && PyObject_CheckBuffer(item)) {
	    /* Attempt to extract buffer information from it. */
	    if (PyObject_GetBuffer(item, &buffer, PyBUF_ANY_CONTIGUOUS) == -1) {
		PyErr_SetString(PyExc_TypeError,
				"Unable to get indom item buffer");
		error = 1;
		break;
	    }
	    ptr = buffer.buf;
	    len = buffer.len;
	}
#else
	/* Older buffer interface */
	if (item && PyObject_CheckReadBuffer(item)) {
	    /* Attempt to extract information from the item. */
	    if (PyObject_AsReadBuffer(item, &ptr, &len) == -1) {
		PyErr_SetString(PyExc_TypeError,
				"Unable to get indom item buffer");
		error = 1;
		break;
	    }
	    buffer.buf = NULL;
	}
#endif
	else {
 	    PyErr_SetString(PyExc_TypeError, "Unable to retrieve indom");
	    error = 1;
	    break;
	}

	/* The indom table is supposed to be composed of
	 * 'pmdaIndom(Structure)' items, which should be laid out
	 * like a 'pmdaIndom' structure in memory.
	 */
	if (len != sizeof(pmdaIndom)) {
	    PyErr_SetString(PyExc_TypeError, "Invalid indom item size");
	    if (buffer.buf)
		PyBuffer_Release(&buffer);
	    error = 1;
	    break;
	}
	indom_buffer[i] = *(pmdaIndom *)ptr;
	if (buffer.buf)
	    PyBuffer_Release(&buffer);
    }

    /* Copy the metrics. */
    for (i = 0; !error && i < nmetrics; i++) {
	item = PyList_GetItem(metric_list, i);
#if defined(ENABLE_PYTHON3)
	/* Newer buffer interface */
	if (item && PyObject_CheckBuffer(item)) {
	    /* Attempt to extract buffer information from it. */
	    if (PyObject_GetBuffer(item, &buffer, PyBUF_ANY_CONTIGUOUS)
		== -1) {
		PyErr_SetString(PyExc_TypeError,
				"Unable to get metric item buffer");
		error = 1;
		break;
	    }
	    ptr = buffer.buf;
	    len = buffer.len;
	}
#else
	/* Older buffer interface */
	if (item && PyObject_CheckReadBuffer(item)) {
	    /* Attempt to extract information from the item. */
	    if (PyObject_AsReadBuffer(item, &ptr, &len) == -1) {
		PyErr_SetString(PyExc_TypeError,
				"Unable to get metric item buffer");
		error = 1;
		break;
	    }
	    buffer.buf = NULL;
	}
#endif
	else {
	    PyErr_SetString(PyExc_TypeError, "Unable to retrieve metric");
	    error = 1;
	    break;
	}

	/* The metric table is supposed to be composed of
	 * 'pmdaMetric(Structure)' items, which should be laid out
	 * like a 'pmdaMetric' structure in memory.
	 */
	if (len != sizeof(pmdaMetric)) {
	    PyErr_SetString(PyExc_TypeError, "Invalid metric item size");
	    if (buffer.buf)
		PyBuffer_Release(&buffer);
	    error = 1;
	    break;
	}
	metric_buffer[i] = *(pmdaMetric *)ptr;
	if (buffer.buf)
	    PyBuffer_Release(&buffer);
    }
    if (error) {
	if (indom_buffer) {
	    free(indom_buffer);
	    indom_buffer = NULL;
	}
	nindoms = 0;
	if (metric_buffer) {
	    free(metric_buffer);
	    metric_buffer = NULL;
	}
	nmetrics = 0;
    }
    return error;
}

static void
pmda_refresh_metrics(void)
{
    /* Update the metrics/indoms. */
    if (!update_indom_metric_buffers()) {
	if (pmDebugOptions.libpmda)
	    fprintf(stderr,
		    "pmda_refresh_metrics: rehash %ld indoms, %ld metrics\n",
		    (long)nindoms, (long)nmetrics);
	dispatch.version.any.ext->e_indoms = indom_buffer;
	dispatch.version.any.ext->e_nindoms = nindoms;
	pmdaRehash(dispatch.version.any.ext, metric_buffer, nmetrics);
    }
}

/*
 * Acquire the global interpreter lock before calling Python callbacks
 */
static int
check_callback(void)
{
    if (thread_state) {
	PyEval_RestoreThread(thread_state);
	thread_state = NULL;
    }
    return 1;
}

/*
 * Release the global interpreter lock after calling Python callbacks
 * This ensures that Python threads can execute while the PMDA is waiting
 * for new PDUs
 */
static void
done_callback(void)
{
    thread_state = PyEval_SaveThread();
}

static PyObject *
pmda_dispatch(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *keyword_list[] = {"indoms", "metrics", NULL};

    if (indom_list) {
	Py_DECREF(indom_list);
	indom_list = NULL;
    }
    if (metric_list) {
	Py_DECREF(metric_list);
	metric_list = NULL;
    }

    if (!PyArg_ParseTupleAndKeywords(args, keywords, "OO", keyword_list,
				     &indom_list, &metric_list))
	return NULL;

    if (indom_list && metric_list) {
	/* PyArg_ParseTupleAndKeywords() returns "borrowed" * references.
	 * Since we're going to keep these objects around for use later,
	 * increase their reference counts.
	 */
	Py_INCREF(indom_list);
	Py_INCREF(metric_list);

	if (!PyList_Check(indom_list) || !PyList_Check(metric_list)) {
	    pmNotifyErr(LOG_ERR,
		"pmda_dispatch failed to get metrics/indoms (non-list types)");
	    PyErr_SetString(PyExc_TypeError,
		"pmda_dispatch failed to get metrics/indoms (non-list types)");
	    Py_DECREF(indom_list);
	    indom_list = NULL;
	    Py_DECREF(metric_list);
	    metric_list = NULL;
	    return NULL;
	}
    }
    else {
	pmNotifyErr(LOG_ERR,
			"pmda_dispatch failed to get metric/indom lists");
	PyErr_SetString(PyExc_TypeError,
			"pmda_dispatch failed to get metric/indom lists");
	return NULL;
    }

    /* Update the indoms/metrics. */
    if (! update_indom_metric_buffers()) {
	if (pmDebugOptions.libpmda)
	    fprintf(stderr, "pmda_dispatch pmdaInit for metrics/indoms\n");
	pmdaInit(&dispatch, indom_buffer, nindoms, metric_buffer, nmetrics);
	if ((dispatch.version.any.ext->e_flags & PMDA_EXT_CONNECTED)
	    != PMDA_EXT_CONNECTED) {
	    /*
	     * connect_pmcd() not called before, so need pmdaConnect()
	     * here before falling into the PDU-driven pmdaMain() loop.
	     */
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "pmda_dispatch connect to pmcd\n");
	    pmdaConnect(&dispatch);
	}

	if (pmDebugOptions.libpmda)
	    fprintf(stderr, "pmda_dispatch entering PDU loop\n");

        dispatch.version.any.ext->e_checkCallBack = check_callback;
        dispatch.version.any.ext->e_doneCallBack = done_callback;

        /*
         * done_callback() releases the GIL
         * it will be reacquired in check_callback() once a PDU arrives
         */
        done_callback();
        pmdaMain(&dispatch);
	check_callback(); /* reacquire GIL for graceful exit */
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pmda_log(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *message;
    char *keyword_list[] = {"message", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"s:pmda_log", keyword_list, &message))
	return NULL;
    pmNotifyErr(LOG_INFO, "%s", message);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pmda_dbg(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *message;
    char *keyword_list[] = {"message", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"s:pmda_dbg", keyword_list, &message))
	return NULL;
    pmNotifyErr(LOG_DEBUG, "%s", message);
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
    pmNotifyErr(LOG_ERR, "%s", message);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pmda_pmid(PyObject *self, PyObject *args, PyObject *keywords)
{
    int result;
    int cluster, item;
    char *keyword_list[] = {"cluster", "item", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"ii:pmda_pmid", keyword_list,
			&cluster, &item))
	return NULL;
    result = pmID_build(dispatch.domain, cluster, item);
    return Py_BuildValue("i", result);
}

static PyObject *
pmid_build(PyObject *self, PyObject *args, PyObject *keywords)
{
    int result;
    int domain, cluster, item;
    char *keyword_list[] = {"domain", "cluster", "item", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"iii:pmid_build", keyword_list,
			&domain, &cluster, &item))
	return NULL;
    result = pmID_build(domain, cluster, item);
    return Py_BuildValue("i", result);
}

static PyObject *
pmid_cluster(PyObject *self, PyObject *args, PyObject *keywords)
{
    int result;
    int pmid;
    char *keyword_list[] = {"pmid", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"i:pmid_cluster", keyword_list,
			&pmid))
	return NULL;
    result = pmID_cluster(pmid);
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
indom_build(PyObject *self, PyObject *args, PyObject *keywords)
{
    int result;
    int domain, serial;
    char *keyword_list[] = {"domain", "serial", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"ii:indom_build", keyword_list,
			&domain, &serial))
	return NULL;
    result = pmInDom_build(domain, serial);
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
	memcpy(&result, &units, sizeof(result));
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
	pmsprintf(s, sz, "%ddays %02d:%02d:%02d", days, hours, mins, secs);
    else if (days == 1)
	pmsprintf(s, sz, "%dday %02d:%02d:%02d", days, hours, mins, secs);
    else
	pmsprintf(s, sz, "%02d:%02d:%02d", hours, mins, secs);

    return Py_BuildValue("s", s);
}

static PyObject *
pmda_set_comm_flags(PyObject *self, PyObject *args, PyObject *keywords)
{
    int flags;
    char *keyword_list[] = {"flags", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
        "i:pmda_set_comm_flags", keyword_list, &flags))
        return NULL;

    pmdaSetCommFlags(&dispatch, flags);
    return Py_None;
}

static PyObject *
set_notify_change(PyObject *self, PyObject *args)
{
    const int flags = PMDA_EXT_LABEL_CHANGE | PMDA_EXT_NAMES_CHANGE;
    pmdaExtSetFlags(dispatch.version.any.ext, flags);
    Py_INCREF(Py_None);
    return Py_None;
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
set_label(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_label", &label_func);
}

static PyObject *
set_notes(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_notes", &notes_func);
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

static PyObject *
set_label_callback(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_label_callback", &label_cb_func);
}

static PyObject *
set_notes_callback(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_notes_callback", &notes_cb_func);
}

static PyObject *
set_attribute_callback(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_attribute_callback", &attribute_cb_func);
}

static PyObject *
set_endcontext_callback(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_endcontext_callback", &endcontext_cb_func);
}

static PyObject *
set_refresh_all(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_refresh_all", &refresh_all_func);
}

static PyObject *
set_refresh_metrics(PyObject *self, PyObject *args)
{
    return set_callback(self, args, "O:set_refresh_metrics",
			&refresh_metrics_func);
}

static PyMethodDef methods[] = {
    { .ml_name = "pmda_pmid", .ml_meth = (PyCFunction)pmda_pmid,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmid_build", .ml_meth = (PyCFunction)pmid_build,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmid_cluster", .ml_meth = (PyCFunction)pmid_cluster,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_indom", .ml_meth = (PyCFunction)pmda_indom,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "indom_build", .ml_meth = (PyCFunction)indom_build,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_units", .ml_meth = (PyCFunction)pmda_units,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_uptime", .ml_meth = (PyCFunction)pmda_uptime,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_set_comm_flags", .ml_meth = (PyCFunction)pmda_set_comm_flags,
	.ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "init_dispatch", .ml_meth = (PyCFunction)init_dispatch,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_dispatch", .ml_meth = (PyCFunction)pmda_dispatch,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "connect_pmcd", .ml_meth = (PyCFunction)connect_pmcd,
	.ml_flags = METH_NOARGS },
    { .ml_name = "pmda_notready", .ml_meth = (PyCFunction)pmda_notready,
	.ml_flags = METH_NOARGS },
    { .ml_name = "pmda_ready", .ml_meth = (PyCFunction)pmda_ready,
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
    { .ml_name = "set_notify_change", .ml_meth = (PyCFunction)set_notify_change,
	.ml_flags = METH_NOARGS },
    { .ml_name = "set_need_refresh", .ml_meth = (PyCFunction)set_need_refresh,
	.ml_flags = METH_NOARGS },
    { .ml_name = "set_fetch", .ml_meth = (PyCFunction)set_fetch,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_refresh", .ml_meth = (PyCFunction)set_refresh,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_instance", .ml_meth = (PyCFunction)set_instance,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_label", .ml_meth = (PyCFunction)set_label,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_notes", .ml_meth = (PyCFunction)set_notes,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_store_callback", .ml_meth = (PyCFunction)set_store_callback,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_fetch_callback", .ml_meth = (PyCFunction)set_fetch_callback,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_label_callback", .ml_meth = (PyCFunction)set_label_callback,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_notes_callback", .ml_meth = (PyCFunction)set_notes_callback,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_attribute_callback", .ml_meth = (PyCFunction)set_attribute_callback,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_endcontext_callback", .ml_meth = (PyCFunction)set_endcontext_callback,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_refresh_metrics",
      .ml_meth = (PyCFunction)set_refresh_metrics,
      .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "set_refresh_all", .ml_meth = (PyCFunction)set_refresh_all,
	.ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmda_log", .ml_meth = (PyCFunction)pmda_log,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_dbg", .ml_meth = (PyCFunction)pmda_dbg,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmda_err", .ml_meth = (PyCFunction)pmda_err,
	.ml_flags = METH_VARARGS|METH_KEYWORDS },
    { NULL },
};

static void
pmda_dict_add(PyObject *dict, char *sym, long val)
{
#if PY_MAJOR_VERSION >= 3
    PyObject *pyVal = PyLong_FromLong(val);
#else
    PyObject *pyVal = PyInt_FromLong(val);
#endif

    PyDict_SetItemString(dict, sym, pyVal);
    Py_XDECREF(pyVal);
}

/* called when the module is initialized. */
MOD_INIT(cpmda)
{
    PyObject *module, *dict;

    MOD_DEF(module, "cpmda", NULL, methods);
    if (module == NULL)
	return MOD_ERROR_VAL;

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

    /* pmda.h - communication flags */
    pmda_dict_add(dict, "PMDA_FLAG_AUTHORIZE", PMDA_FLAG_AUTHORIZE);
    pmda_dict_add(dict, "PMDA_FLAG_CONTAINER", PMDA_FLAG_CONTAINER);

    /* pmda.h - context attributes */
    pmda_dict_add(dict, "PMDA_ATTR_USERNAME", PMDA_ATTR_USERNAME);
    pmda_dict_add(dict, "PMDA_ATTR_USERID", PMDA_ATTR_USERID);
    pmda_dict_add(dict, "PMDA_ATTR_GROUPID", PMDA_ATTR_GROUPID);
    pmda_dict_add(dict, "PMDA_ATTR_PROCESSID", PMDA_ATTR_PROCESSID);
    pmda_dict_add(dict, "PMDA_ATTR_CONTAINER", PMDA_ATTR_CONTAINER);

    return MOD_SUCCESS_VAL(module);
}
