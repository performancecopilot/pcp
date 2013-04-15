/*
 * Copyright (C) 2012-2013 Red Hat.
 * Copyright (C) 2009-2012 Michael T. Werner
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
** This C extension module mainly serves the purpose of loading constants **
** from PCP headers into the module dictionary.  The PMAPI functions and  **
** data structures are wrapped in pmapi.py and friends, using ctypes.     **
**                                                                        **
\**************************************************************************/

#include <Python.h>
#include <pcp/pmapi.h>

static void
dict_add_unsigned(PyObject *dict, char *symbol, unsigned long value)
{
    PyObject *pyvalue = PyLong_FromUnsignedLong(value);
    PyDict_SetItemString(dict, symbol, pyvalue);
    Py_XDECREF(pyvalue);
}

static void
dict_add(PyObject *dict, char *symbol, long value)
{
    PyObject *pyvalue = PyInt_FromLong(value);
    PyDict_SetItemString(dict, symbol, pyvalue);
    Py_XDECREF(pyvalue);
}

static void
edict_add(PyObject *dict, PyObject *edict, char *symbol, long value)
{
    PyObject *pyvalue = PyInt_FromLong(value);
    PyObject *pysymbol = PyString_FromString(symbol);

    PyDict_SetItemString(dict, symbol, pyvalue);
    PyDict_SetItem(edict, pyvalue, pysymbol);
    Py_XDECREF(pysymbol);
    Py_XDECREF(pyvalue);
}

static PyObject *
setExtendedTimeBase(PyObject *self, PyObject *args, PyObject *keywords)
{
    int type;
    char *keyword_list[] = {"type", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "i:PM_XTB_SET", keyword_list, &type))
        return NULL;
    return Py_BuildValue("i", PM_XTB_SET(type));
}

static PyObject *
getExtendedTimeBase(PyObject *self, PyObject *args, PyObject *keywords)
{
    int mode;
    char *keyword_list[] = {"mode", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "i:PM_XTB_GET", keyword_list, &mode))
        return NULL;
    return Py_BuildValue("i", PM_XTB_GET(mode));
}

static PyObject *
timevalSleep(PyObject *self, PyObject *args, PyObject *keywords)
{
    struct timeval *ctvp;
    char *keyword_list[] = {"timeval", NULL};
    extern void __pmtimevalSleep(struct timeval);

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "O:pmtimevalSleep", keyword_list, &ctvp))
        return NULL;
    __pmtimevalSleep(*ctvp);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setIdentity(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *name;
    char *keyword_list[] = {"name", NULL};
    extern int __pmSetProcessIdentity(const char *);

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "s:pmSetProcessIdentity", keyword_list, &name))
        return NULL;
    return Py_BuildValue("i", __pmSetProcessIdentity(name));
}

static PyMethodDef methods[] = {
    { .ml_name = "PM_XTB_SET", .ml_meth = (PyCFunction)setExtendedTimeBase,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "PM_XTB_GET", .ml_meth = (PyCFunction)getExtendedTimeBase,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmtimevalSleep", .ml_meth = (PyCFunction)timevalSleep,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { .ml_name = "pmSetProcessIdentity", .ml_meth = (PyCFunction)setIdentity,
        .ml_flags = METH_VARARGS|METH_KEYWORDS },
    { NULL }
};

/* called when the module is initialized. */
void
initcpmapi(void)
{
    PyObject *module, *dict, *edict;

    module = Py_InitModule("cpmapi", methods);
    dict = PyModule_GetDict(module);
    edict = PyDict_New();
    Py_INCREF(edict);
    PyModule_AddObject(module, "pmErrSymDict", edict);

    dict_add(dict, "PMAPI_VERSION_2", PMAPI_VERSION_2);
    dict_add(dict, "PMAPI_VERSION", PMAPI_VERSION);

    dict_add_unsigned(dict, "PM_ID_NULL", PM_ID_NULL);
    dict_add_unsigned(dict, "PM_INDOM_NULL", PM_INDOM_NULL);
    dict_add_unsigned(dict, "PM_IN_NULL", PM_IN_NULL);

    dict_add_unsigned(dict, "PM_NS_DEFAULT", 0);

#ifdef HAVE_BITFIELDS_LTOR
    dict_add(dict, "HAVE_BITFIELDS_LTOR", 1);
    dict_add(dict, "HAVE_BITFIELDS_RTOL", 0);
#else
    dict_add(dict, "HAVE_BITFIELDS_LTOR", 0);
    dict_add(dict, "HAVE_BITFIELDS_RTOL", 1);
#endif

    dict_add(dict, "PM_SPACE_BYTE", PM_SPACE_BYTE);
    dict_add(dict, "PM_SPACE_KBYTE", PM_SPACE_KBYTE);
    dict_add(dict, "PM_SPACE_MBYTE", PM_SPACE_MBYTE);
    dict_add(dict, "PM_SPACE_GBYTE", PM_SPACE_GBYTE);
    dict_add(dict, "PM_SPACE_TBYTE", PM_SPACE_TBYTE);
    dict_add(dict, "PM_SPACE_PBYTE", PM_SPACE_PBYTE);
    dict_add(dict, "PM_SPACE_EBYTE", PM_SPACE_EBYTE);

    dict_add(dict, "PM_TIME_NSEC", PM_TIME_NSEC);
    dict_add(dict, "PM_TIME_USEC", PM_TIME_USEC);
    dict_add(dict, "PM_TIME_MSEC", PM_TIME_MSEC);
    dict_add(dict, "PM_TIME_SEC", PM_TIME_SEC);
    dict_add(dict, "PM_TIME_MIN", PM_TIME_MIN);
    dict_add(dict, "PM_TIME_HOUR", PM_TIME_HOUR);
    dict_add(dict, "PM_COUNT_ONE", PM_COUNT_ONE);

    dict_add(dict, "PM_TYPE_NOSUPPORT", PM_TYPE_NOSUPPORT);
    dict_add(dict, "PM_TYPE_32", PM_TYPE_32);
    dict_add(dict, "PM_TYPE_U32", PM_TYPE_U32);
    dict_add(dict, "PM_TYPE_64", PM_TYPE_64);
    dict_add(dict, "PM_TYPE_U64", PM_TYPE_U64);
    dict_add(dict, "PM_TYPE_FLOAT", PM_TYPE_FLOAT);
    dict_add(dict, "PM_TYPE_DOUBLE", PM_TYPE_DOUBLE);
    dict_add(dict, "PM_TYPE_STRING", PM_TYPE_STRING);
    dict_add(dict, "PM_TYPE_AGGREGATE", PM_TYPE_AGGREGATE);
    dict_add(dict, "PM_TYPE_AGGREGATE_STATIC", PM_TYPE_AGGREGATE_STATIC);
    dict_add(dict, "PM_TYPE_EVENT", PM_TYPE_EVENT);
    dict_add(dict, "PM_TYPE_UNKNOWN", PM_TYPE_UNKNOWN);

    dict_add(dict, "PM_SEM_COUNTER", PM_SEM_COUNTER);
    dict_add(dict, "PM_SEM_INSTANT", PM_SEM_INSTANT);
    dict_add(dict, "PM_SEM_DISCRETE", PM_SEM_DISCRETE);

    dict_add(dict, "PMNS_LOCAL", PMNS_LOCAL);
    dict_add(dict, "PMNS_REMOTE", PMNS_REMOTE);
    dict_add(dict, "PMNS_ARCHIVE", PMNS_ARCHIVE);
    dict_add(dict, "PMNS_LEAF_STATUS", PMNS_LEAF_STATUS);
    dict_add(dict, "PMNS_NONLEAF_STATUS", PMNS_NONLEAF_STATUS);

    dict_add(dict, "PM_CONTEXT_UNDEF", PM_CONTEXT_UNDEF);
    dict_add(dict, "PM_CONTEXT_HOST", PM_CONTEXT_HOST);
    dict_add(dict, "PM_CONTEXT_ARCHIVE", PM_CONTEXT_ARCHIVE);
    dict_add(dict, "PM_CONTEXT_LOCAL", PM_CONTEXT_LOCAL);
    dict_add(dict, "PM_CONTEXT_TYPEMASK", PM_CONTEXT_TYPEMASK);
    dict_add(dict, "PM_CTXFLAG_SECURE", PM_CTXFLAG_SECURE);
    dict_add(dict, "PM_CTXFLAG_COMPRESS", PM_CTXFLAG_COMPRESS);
    dict_add(dict, "PM_CTXFLAG_RELAXED", PM_CTXFLAG_RELAXED);

    dict_add(dict, "PM_VAL_HDR_SIZE", PM_VAL_HDR_SIZE);
    dict_add(dict, "PM_VAL_VLEN_MAX", PM_VAL_VLEN_MAX);
    dict_add(dict, "PM_VAL_INSITU", PM_VAL_INSITU);
    dict_add(dict, "PM_VAL_DPTR",   PM_VAL_DPTR);
    dict_add(dict, "PM_VAL_SPTR",   PM_VAL_SPTR);

    dict_add(dict, "PMCD_NO_CHANGE", PMCD_NO_CHANGE);
    dict_add(dict, "PMCD_ADD_AGENT", PMCD_ADD_AGENT);
    dict_add(dict, "PMCD_RESTART_AGENT", PMCD_RESTART_AGENT);
    dict_add(dict, "PMCD_DROP_AGENT", PMCD_DROP_AGENT);

    dict_add(dict, "PM_MAXERRMSGLEN", PM_MAXERRMSGLEN);
    dict_add(dict, "PM_TZ_MAXLEN",    PM_TZ_MAXLEN);

    dict_add(dict, "PM_LOG_MAXHOSTLEN", PM_LOG_MAXHOSTLEN);
    dict_add(dict, "PM_LOG_MAGIC",    PM_LOG_MAGIC);
    dict_add(dict, "PM_LOG_VERS02",   PM_LOG_VERS02);
    dict_add(dict, "PM_LOG_VOL_TI",   PM_LOG_VOL_TI);
    dict_add(dict, "PM_LOG_VOL_META", PM_LOG_VOL_META);

    dict_add(dict, "PM_MODE_LIVE",   PM_MODE_LIVE);
    dict_add(dict, "PM_MODE_INTERP", PM_MODE_INTERP);
    dict_add(dict, "PM_MODE_FORW",   PM_MODE_FORW);
    dict_add(dict, "PM_MODE_BACK",   PM_MODE_BACK);

    dict_add(dict, "PM_TEXT_ONELINE", PM_TEXT_ONELINE);
    dict_add(dict, "PM_TEXT_HELP",    PM_TEXT_HELP);

    dict_add(dict, "PM_XTB_FLAG", PM_XTB_FLAG);

    dict_add(dict, "PM_EVENT_FLAG_POINT",  PM_EVENT_FLAG_POINT);
    dict_add(dict, "PM_EVENT_FLAG_START",  PM_EVENT_FLAG_START);
    dict_add(dict, "PM_EVENT_FLAG_END",    PM_EVENT_FLAG_END);
    dict_add(dict, "PM_EVENT_FLAG_ID",     PM_EVENT_FLAG_ID);
    dict_add(dict, "PM_EVENT_FLAG_PARENT", PM_EVENT_FLAG_PARENT);
    dict_add(dict, "PM_EVENT_FLAG_MISSED", PM_EVENT_FLAG_MISSED);

    edict_add(dict, edict, "PM_ERR_GENERIC", PM_ERR_GENERIC);
    edict_add(dict, edict, "PM_ERR_PMNS", PM_ERR_PMNS);
    edict_add(dict, edict, "PM_ERR_NOPMNS", PM_ERR_NOPMNS);
    edict_add(dict, edict, "PM_ERR_DUPPMNS", PM_ERR_DUPPMNS);
    edict_add(dict, edict, "PM_ERR_TEXT", PM_ERR_TEXT);
    edict_add(dict, edict, "PM_ERR_APPVERSION", PM_ERR_APPVERSION);
    edict_add(dict, edict, "PM_ERR_VALUE", PM_ERR_VALUE);
    edict_add(dict, edict, "PM_ERR_TIMEOUT", PM_ERR_TIMEOUT);
    edict_add(dict, edict, "PM_ERR_NODATA", PM_ERR_NODATA);
    edict_add(dict, edict, "PM_ERR_RESET", PM_ERR_RESET);
    edict_add(dict, edict, "PM_ERR_NAME", PM_ERR_NAME);
    edict_add(dict, edict, "PM_ERR_PMID", PM_ERR_PMID);
    edict_add(dict, edict, "PM_ERR_INDOM", PM_ERR_INDOM);
    edict_add(dict, edict, "PM_ERR_INST", PM_ERR_INST);
    edict_add(dict, edict, "PM_ERR_UNIT", PM_ERR_UNIT);
    edict_add(dict, edict, "PM_ERR_CONV", PM_ERR_CONV);
    edict_add(dict, edict, "PM_ERR_TRUNC", PM_ERR_TRUNC);
    edict_add(dict, edict, "PM_ERR_SIGN", PM_ERR_SIGN);
    edict_add(dict, edict, "PM_ERR_PROFILE", PM_ERR_PROFILE);
    edict_add(dict, edict, "PM_ERR_IPC", PM_ERR_IPC);
    edict_add(dict, edict, "PM_ERR_EOF", PM_ERR_EOF);
    edict_add(dict, edict, "PM_ERR_NOTHOST", PM_ERR_NOTHOST);
    edict_add(dict, edict, "PM_ERR_EOL", PM_ERR_EOL);
    edict_add(dict, edict, "PM_ERR_MODE", PM_ERR_MODE);
    edict_add(dict, edict, "PM_ERR_LABEL", PM_ERR_LABEL);
    edict_add(dict, edict, "PM_ERR_LOGREC", PM_ERR_LOGREC);
    edict_add(dict, edict, "PM_ERR_NOTARCHIVE", PM_ERR_NOTARCHIVE);
    edict_add(dict, edict, "PM_ERR_LOGFILE", PM_ERR_LOGFILE);
    edict_add(dict, edict, "PM_ERR_NOCONTEXT", PM_ERR_NOCONTEXT);
    edict_add(dict, edict, "PM_ERR_PROFILESPEC", PM_ERR_PROFILESPEC);
    edict_add(dict, edict, "PM_ERR_PMID_LOG", PM_ERR_PMID_LOG);
    edict_add(dict, edict, "PM_ERR_INDOM_LOG", PM_ERR_INDOM_LOG);
    edict_add(dict, edict, "PM_ERR_INST_LOG", PM_ERR_INST_LOG);
    edict_add(dict, edict, "PM_ERR_NOPROFILE", PM_ERR_NOPROFILE);
    edict_add(dict, edict, "PM_ERR_NOAGENT", PM_ERR_NOAGENT);
    edict_add(dict, edict, "PM_ERR_PERMISSION", PM_ERR_PERMISSION);
    edict_add(dict, edict, "PM_ERR_CONNLIMIT", PM_ERR_CONNLIMIT);
    edict_add(dict, edict, "PM_ERR_AGAIN", PM_ERR_AGAIN);
    edict_add(dict, edict, "PM_ERR_ISCONN", PM_ERR_ISCONN);
    edict_add(dict, edict, "PM_ERR_NOTCONN", PM_ERR_NOTCONN);
    edict_add(dict, edict, "PM_ERR_NEEDPORT", PM_ERR_NEEDPORT);
    edict_add(dict, edict, "PM_ERR_NONLEAF", PM_ERR_NONLEAF);
    edict_add(dict, edict, "PM_ERR_TYPE", PM_ERR_TYPE);
    edict_add(dict, edict, "PM_ERR_THREAD", PM_ERR_THREAD);
    edict_add(dict, edict, "PM_ERR_TOOSMALL", PM_ERR_TOOSMALL);
    edict_add(dict, edict, "PM_ERR_TOOBIG", PM_ERR_TOOBIG);
    edict_add(dict, edict, "PM_ERR_FAULT", PM_ERR_FAULT);
    edict_add(dict, edict, "PM_ERR_PMDAREADY", PM_ERR_PMDAREADY);
    edict_add(dict, edict, "PM_ERR_PMDANOTREADY", PM_ERR_PMDANOTREADY);
    edict_add(dict, edict, "PM_ERR_NYI", PM_ERR_NYI);
}
