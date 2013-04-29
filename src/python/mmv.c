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
** This C extension module mainly serves the purpose of loading constants **
** from PCP headers into the module dictionary.  The MMV functions and    **
** data structures are wrapped in mmv.py using ctypes.                    **
**                                                                        **
\**************************************************************************/

#include <Python.h>
#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>

static void
dict_add(PyObject *dict, char *symbol, long value)
{
    PyObject *pyvalue = PyInt_FromLong(value);
    PyDict_SetItemString(dict, symbol, pyvalue);
    Py_XDECREF(pyvalue);
}

static PyMethodDef methods[] = { { NULL } };

/* called when the module is initialized. */
void
initcmmv(void)
{
    PyObject *module, *dict;

    module = Py_InitModule("cmmv", methods);
    dict = PyModule_GetDict(module);

    dict_add(dict, "MMV_NAMEMAX", MMV_NAMEMAX);
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
}
