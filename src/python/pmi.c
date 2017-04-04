/*
 * Copyright (C) 2013-2014 Red Hat.
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
** from the PCP log import header into the module dictionary.  PMI data   **
** structures and interfaces are wrapped in pmi.py, using ctypes.         **
**                                                                        **
\**************************************************************************/

#include <Python.h>
#include <pcp/pmapi.h>
#include <pcp/import.h>

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

static PyMethodDef methods[] = { { NULL } };

static void
pmi_dict_add(PyObject *dict, char *sym, long val)
{
#if PY_MAJOR_VERSION >= 3
    PyObject *pyVal = PyLong_FromLong(val);
#else
    PyObject *pyVal = PyInt_FromLong(val);
#endif

    PyDict_SetItemString(dict, sym, pyVal);
    Py_XDECREF(pyVal);
}

static void
pmi_edict_add(PyObject *dict, PyObject *edict, char *sym, long val)
{
    PyObject *pySym;
#if PY_MAJOR_VERSION >= 3
    PyObject *pyVal = PyLong_FromLong(val);
#else
    PyObject *pyVal = PyInt_FromLong(val);
#endif

    PyDict_SetItemString(dict, sym, pyVal);
#if PY_MAJOR_VERSION >= 3
    pySym = PyUnicode_FromString(sym);
#else
    pySym = PyString_FromString(sym);
#endif
    PyDict_SetItem(edict, pyVal, pySym);
    Py_XDECREF(pySym);
    Py_XDECREF(pyVal);
}

/* This function is called when the module is initialized. */ 
MOD_INIT(cpmi)
{
    PyObject *module, *dict, *edict;

    MOD_DEF(module, "cpmi", NULL, methods);
    if (module == NULL)
	return MOD_ERROR_VAL;

    dict = PyModule_GetDict(module);
    edict = PyDict_New();
    Py_INCREF(edict); 
    PyModule_AddObject(module, "pmiErrSymDict", edict); 

    /* import.h */
    pmi_dict_add(dict, "PMI_MAXERRMSGLEN", PMI_MAXERRMSGLEN);
    pmi_dict_add(dict, "PMI_ERR_BASE", PMI_ERR_BASE);

    pmi_edict_add(dict, edict, "PMI_ERR_DUPMETRICNAME", PMI_ERR_DUPMETRICNAME);
    pmi_edict_add(dict, edict, "PMI_ERR_DUPMETRICID", PMI_ERR_DUPMETRICID);
    pmi_edict_add(dict, edict, "PMI_ERR_DUPINSTNAME", PMI_ERR_DUPINSTNAME);
    pmi_edict_add(dict, edict, "PMI_ERR_DUPINSTID", PMI_ERR_DUPINSTID);
    pmi_edict_add(dict, edict, "PMI_ERR_INSTNOTNULL", PMI_ERR_INSTNOTNULL);
    pmi_edict_add(dict, edict, "PMI_ERR_INSTNULL", PMI_ERR_INSTNULL);
    pmi_edict_add(dict, edict, "PMI_ERR_BADHANDLE", PMI_ERR_BADHANDLE);
    pmi_edict_add(dict, edict, "PMI_ERR_DUPVALUE", PMI_ERR_DUPVALUE);
    pmi_edict_add(dict, edict, "PMI_ERR_BADTYPE", PMI_ERR_BADTYPE);
    pmi_edict_add(dict, edict, "PMI_ERR_BADSEM", PMI_ERR_BADSEM);
    pmi_edict_add(dict, edict, "PMI_ERR_NODATA", PMI_ERR_NODATA);
    pmi_edict_add(dict, edict, "PMI_ERR_BADMETRICNAME", PMI_ERR_BADMETRICNAME);
    pmi_edict_add(dict, edict, "PMI_ERR_BADTIMESTAMP", PMI_ERR_BADTIMESTAMP);

    return MOD_SUCCESS_VAL(module);
}
