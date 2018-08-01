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
** from PCP headers into the module dictionary.  The MMV functions and    **
** data structures are wrapped in mmv.py using ctypes.                    **
**                                                                        **
\**************************************************************************/

#include <Python.h>
#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

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

static void
dict_add(PyObject *dict, char *symbol, long value)
{
#if PY_MAJOR_VERSION >= 3
    PyObject *pyvalue = PyLong_FromLong(value);
#else
    PyObject *pyvalue = PyInt_FromLong(value);
#endif
    PyDict_SetItemString(dict, symbol, pyvalue);
    Py_XDECREF(pyvalue);
}

static PyMethodDef methods[] = { { NULL } };

/* called when the module is initialized. */
MOD_INIT(cmmv)
{
    PyObject *module, *dict;

    MOD_DEF(module, "cmmv", NULL, methods);
    if (module == NULL)
	return MOD_ERROR_VAL;

    dict = PyModule_GetDict(module);

    dict_add(dict, "MMV_NAMEMAX", MMV_NAMEMAX);
    dict_add(dict, "MMV_LABELMAX", MMV_LABELMAX);
    dict_add(dict, "MMV_STRINGMAX", MMV_STRINGMAX);

    dict_add(dict, "MMV_TYPE_NOSUPPORT", MMV_TYPE_NOSUPPORT);
    dict_add(dict, "MMV_TYPE_I32", MMV_TYPE_I32);
    dict_add(dict, "MMV_TYPE_U32", MMV_TYPE_U32);
    dict_add(dict, "MMV_TYPE_I64", MMV_TYPE_I64);
    dict_add(dict, "MMV_TYPE_U64", MMV_TYPE_U64);
    dict_add(dict, "MMV_TYPE_FLOAT", MMV_TYPE_FLOAT);
    dict_add(dict, "MMV_TYPE_DOUBLE", MMV_TYPE_DOUBLE);
    dict_add(dict, "MMV_TYPE_STRING", MMV_TYPE_STRING);
    dict_add(dict, "MMV_TYPE_ELAPSED", MMV_TYPE_ELAPSED);

    dict_add(dict, "MMV_SEM_COUNTER", MMV_SEM_COUNTER);
    dict_add(dict, "MMV_SEM_INSTANT", MMV_SEM_INSTANT);
    dict_add(dict, "MMV_SEM_DISCRETE", MMV_SEM_DISCRETE);

    dict_add(dict, "MMV_FLAG_NOPREFIX", MMV_FLAG_NOPREFIX);
    dict_add(dict, "MMV_FLAG_PROCESS", MMV_FLAG_PROCESS);
    dict_add(dict, "MMV_FLAG_SENTINEL", MMV_FLAG_SENTINEL);

    dict_add(dict, "MMV_STRING_TYPE", MMV_STRING_TYPE);
    dict_add(dict, "MMV_NUMBER_TYPE", MMV_NUMBER_TYPE);
    dict_add(dict, "MMV_BOOLEAN_TYPE", MMV_BOOLEAN_TYPE);
    dict_add(dict, "MMV_NULL_TYPE", MMV_NULL_TYPE);
    dict_add(dict, "MMV_ARRAY_TYPE", MMV_ARRAY_TYPE);
    dict_add(dict, "MMV_MAP_TYPE", MMV_MAP_TYPE);

    return MOD_SUCCESS_VAL(module);
}
