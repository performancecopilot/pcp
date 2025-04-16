/*
 * Copyright (C) 2012-2014,2025 Red Hat.
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
** from PCP headers into the module dictionary.  The GUI API functions    **
** and data structures are wrapped in pmgui.py and friends, using ctypes. **
**                                                                        **
\**************************************************************************/
#define PY_SSIZE_T_CLEAN
#define _FILE_OFFSET_BITS 64
#define _TIME_BITS 64
#include <Python.h>
#include <pcp/pmafm.h>
#include <pcp/pmtime.h>

#define MOD_ERROR_VAL NULL
#define MOD_SUCCESS_VAL(val) val
#define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
#define MOD_DEF(ob, name, doc, methods) \
        static struct PyModuleDef moduledef = { \
          PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
        ob = PyModule_Create(&moduledef);

static void
pmgui_dict_add(PyObject *dict, char *sym, long val)
{
    PyObject *pyVal = PyLong_FromLong(val);

    PyDict_SetItemString(dict, sym, pyVal);
    Py_XDECREF(pyVal);
} 

static PyMethodDef methods[] = { { NULL } };

/* called when the module is initialized. */ 
MOD_INIT(cpmgui)
{
    PyObject *module, *dict;

    MOD_DEF(module, "cpmgui", NULL, methods);
    if (module == NULL)
	return MOD_ERROR_VAL;

    dict = PyModule_GetDict(module);

    /* pmafm.h */
    pmgui_dict_add(dict, "PM_REC_ON", PM_REC_ON);
    pmgui_dict_add(dict, "PM_REC_OFF", PM_REC_OFF);
    pmgui_dict_add(dict, "PM_REC_DETACH", PM_REC_DETACH);
    pmgui_dict_add(dict, "PM_REC_STATUS", PM_REC_STATUS);
    pmgui_dict_add(dict, "PM_REC_SETARG", PM_REC_SETARG);

    return MOD_SUCCESS_VAL(module);
}
