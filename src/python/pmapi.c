/*
 * Copyright (C) 2012-2015 Red Hat.
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
#include <pcp/impl.h>

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

static pmOptions options;
static int longOptionsCount;
static PyObject *optionCallback;
static PyObject *overridesCallback;

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
#if PY_MAJOR_VERSION >= 3
    PyObject *pyvalue = PyLong_FromLong(value);
#else
    PyObject *pyvalue = PyInt_FromLong(value);
#endif
    PyDict_SetItemString(dict, symbol, pyvalue);
    Py_XDECREF(pyvalue);
}

static void
edict_add(PyObject *dict, PyObject *edict, char *symbol, long value)
{
#if PY_MAJOR_VERSION >= 3
    PyObject *pyvalue = PyLong_FromLong(value);
    PyObject *pysymbol = PyUnicode_FromString(symbol);
#else
    PyObject *pyvalue = PyInt_FromLong(value);
    PyObject *pysymbol = PyString_FromString(symbol);
#endif

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
    struct timeval ctv;
    long seconds, useconds;
    char *keyword_list[] = {"seconds", "useconds", NULL};
    extern void __pmtimevalSleep(struct timeval);

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "ll:pmtimevalSleep", keyword_list, &seconds, &useconds))
        return NULL;
    ctv.tv_sec = seconds;
    ctv.tv_usec = useconds;
    __pmtimevalSleep(ctv);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
timevalToReal(PyObject *self, PyObject *args, PyObject *keywords)
{
    struct timeval ctv;
    long seconds, useconds;
    char *keyword_list[] = {"seconds", "useconds", NULL};
    extern double __pmtimevalToReal(const struct timeval *);

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
                        "ll:pmtimevalToReal", keyword_list, &seconds, &useconds))
        return NULL;
    ctv.tv_sec = seconds;
    ctv.tv_usec = useconds;
    return Py_BuildValue("d", __pmtimevalToReal(&ctv));
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

/*
 * Common command line option handling code - wrapping pmOptions
 */

static int
addLongOption(pmLongOptions *opt, int duplicate)
{
    size_t bytes;
    pmLongOptions *lp;
    int index = longOptionsCount;

    if (!opt->long_opt)
	return -EINVAL;

    bytes = (index + 2) * sizeof(pmLongOptions); /* +2 for PMAPI_OPTIONS_END */
    if ((lp = realloc(options.long_options, bytes)) == NULL)
	return -ENOMEM;
    options.long_options = lp;

    if (!duplicate)
	goto update;

    if ((opt->long_opt = strdup(opt->long_opt)) == NULL)
	return -ENOMEM;
    if (opt->argname &&
	(opt->argname = strdup(opt->argname)) == NULL) {
	free((char *)opt->long_opt);
	return -ENOMEM;
    }
    if (opt->message &&
	(opt->message = strdup(opt->message)) == NULL) {
	free((char *)opt->long_opt);
	free((char *)opt->argname);
	return -ENOMEM;
    }

update:
    lp[index].long_opt = opt->long_opt;
    lp[index].has_arg = opt->has_arg;
    lp[index].short_opt = opt->short_opt;
    lp[index].argname = opt->argname;
    lp[index].message = opt->message;
    memset(&lp[index+1], 0, sizeof(pmLongOptions));	/* PMAPI_OPTIONS_END */
    longOptionsCount++;
    return 0;
}

static PyObject *
setLongOptionHeader(PyObject *self, PyObject *args, PyObject *keywords)
{
    pmLongOptions header = PMAPI_OPTIONS_HEADER("");
    char *keyword_list[] = {"heading", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"s:pmSetLongOptionHeader", keyword_list,
			&header.message))
	return NULL;
    if ((header.message = strdup(header.message)) == NULL)
	return PyErr_NoMemory();

    if (addLongOption(&header, 0) < 0) {
	free((char *)header.message);
	return PyErr_NoMemory();
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setLongOptionText(PyObject *self, PyObject *args, PyObject *keywords)
{
    pmLongOptions text = PMAPI_OPTIONS_TEXT("");
    char *keyword_list[] = {"text", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"s:pmSetLongOptionText", keyword_list,
			&text.message))
	return NULL;
    if ((text.message = strdup(text.message)) == NULL)
	return PyErr_NoMemory();

    if (addLongOption(&text, 0) < 0) {
	free((char *)text.message);
	return PyErr_NoMemory();
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
addLongOptionObject(pmLongOptions *option)
{
    if (addLongOption(option, 1) < 0)
	return PyErr_NoMemory();
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setLongOption(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *short_opt = NULL;
    pmLongOptions option = { 0 };
    char *keyword_list[] = {"long_opt", "has_arg", "short_opt",
			    "argname", "message", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"sisss:pmSetLongOption", keyword_list,
			&option.long_opt, &option.has_arg, &short_opt,
			&option.argname, &option.message))
	return NULL;
    if (short_opt)
	option.short_opt = (int)short_opt[0];
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionAlign(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_ALIGN;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionArchive(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_ARCHIVE;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionHostList(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_HOST_LIST;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionArchiveList(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_ARCHIVE_LIST;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionArchiveFolio(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_ARCHIVE_FOLIO;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionDebug(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_DEBUG;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionGuiMode(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_GUIMODE;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionHost(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_HOST;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionHostsFile(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_HOSTSFILE;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionSpecLocal(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_SPECLOCAL;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionLocalPMDA(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_LOCALPMDA;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionOrigin(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_ORIGIN;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionGuiPort(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_GUIPORT;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionStart(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_START;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionSamples(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_SAMPLES;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionFinish(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_FINISH;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionInterval(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_INTERVAL;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionVersion(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_VERSION;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionTimeZone(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_TIMEZONE;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionHostZone(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_HOSTZONE;
    return addLongOptionObject(&option);
}

static PyObject *
setLongOptionHelp(PyObject *self, PyObject *args)
{
    pmLongOptions option = PMOPT_HELP;
    return addLongOptionObject(&option);
}

static PyObject *
resetAllOptions(PyObject *self, PyObject *args)
{
    pmFreeOptions(&options);
    memset(&options, 0, sizeof(options));
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setShortOptions(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *short_opts;
    char *keyword_list[] = {"short_options", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"s:pmSetShortOptions", keyword_list, &short_opts))
	return NULL;

    if ((short_opts = strdup(short_opts ? short_opts : "")) == NULL)
	return PyErr_NoMemory();
    if (options.short_options)
	free((void *)options.short_options);
    options.short_options = short_opts;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setShortUsage(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *short_usage;
    char *keyword_list[] = {"short_usage", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"s:pmSetShortUsage", keyword_list, &short_usage))
	return NULL;

    if ((short_usage = strdup(short_usage ? short_usage : "")) == NULL)
	return PyErr_NoMemory();
    if (options.short_usage)
	free((void *)options.short_usage);
    options.short_usage = short_usage;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setOptionFlags(PyObject *self, PyObject *args, PyObject *keywords)
{
    int flags;
    char *keyword_list[] = {"flags", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"i:pmSetOptionFlags", keyword_list, &flags))
	return NULL;

    options.flags |= flags;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setOptionArchiveFolio(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *folio;
    char *keyword_list[] = {PMLONGOPT_ARCHIVE_FOLIO, NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"s:pmSetOptionArchiveFolio", keyword_list, &folio))
	return NULL;

    __pmAddOptArchiveFolio(&options, folio);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setOptionArchiveList(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *archives;
    char *keyword_list[] = {PMLONGOPT_ARCHIVE_LIST, NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"s:pmSetOptionArchiveList", keyword_list, &archives))
	return NULL;

    __pmAddOptArchiveList(&options, archives);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setOptionHostList(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *hosts;
    char *keyword_list[] = {PMLONGOPT_HOST_LIST, NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"s:pmSetOptionHostList", keyword_list, &hosts))
	return NULL;

    __pmAddOptHostList(&options, hosts);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setOptionSamples(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *count, *endnum;
    char *keyword_list[] = {"count", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"s:pmSetOptionSamples", keyword_list, &count))
	return NULL;

    if (options.finish_optarg) {
	pmprintf("%s: at most one of finish time and sample count allowed\n",
		pmProgname);
	options.errors++;
    } else {
	options.samples = (int)strtol(count, &endnum, 10);
	if (*endnum != '\0' || options.samples < 0) {
	    pmprintf("%s: sample count must be a positive numeric argument\n",
		pmProgname);
	    options.errors++;
	}
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setOptionInterval(PyObject *self, PyObject *args, PyObject *keywords)
{
    char *delta, *errmsg;
    char *keyword_list[] = {"delta", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"s:pmSetOptionInterval", keyword_list, &delta))
	return NULL;

    if (pmParseInterval(delta, &options.interval, &errmsg) < 0) {
	pmprintf("%s: interval argument not in pmParseInterval(3) format:\n",
		pmProgname);
	pmprintf("%s\n", errmsg);
	options.errors++;
	free(errmsg);
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setOptionErrors(PyObject *self, PyObject *args, PyObject *keywords)
{
    int errors;
    char *keyword_list[] = {"errors", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"i:pmSetOptionErrors", keyword_list, &errors))
	return NULL;

    options.errors = errors;
    Py_INCREF(Py_None);
    return Py_None;
}

static int
override_callback(int opt, pmOptions *opts)
{
    PyObject *arglist, *result;
    char argstring[2] = { (char)opt, '\0' };
    int sts;

    arglist = Py_BuildValue("(s)", argstring);
    if (!arglist) {
	PyErr_Print();
	return -ENOMEM;
    }
    result = PyEval_CallObject(overridesCallback, arglist);
    Py_DECREF(arglist);
    if (!result) {
	PyErr_Print();
	return -EAGAIN; /* exception thrown */
    }
    sts = PyLong_AsLong(result);
    Py_DECREF(result);
    return sts;
}

static void
options_callback(int opt, pmOptions *opts)
{
    PyObject *arglist, *result;
    char argstring[2] = { (char)opt, '\0' };

    arglist = Py_BuildValue("(ssi)", argstring, options.optarg, options.index);
    if (!arglist) {
	PyErr_Print();
    } else {
	result = PyEval_CallObject(optionCallback, arglist);
	Py_DECREF(arglist);
        if (!result) {
            PyErr_Print();
            return;
        }
        Py_DECREF(result);
    }
}

static PyObject *
getNonOptionsFromList(PyObject *self, PyObject *args, PyObject *keywords)
{
    int argc, length, i;
    PyObject *result;
    PyObject *pyargv = NULL;
    char *keyword_list[] = {"argv", NULL};

    /* Caller must perform pmGetOptions before running this, check */
    if (!(options.flags & PM_OPTFLAG_DONE))
	return NULL;

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"O:pmGetNonOptionsFromList", keyword_list, &pyargv))
    if (pyargv == NULL)
	return NULL;

    if (!PyList_Check(pyargv)) {
	PyErr_SetString(PyExc_TypeError, "pmGetNonOptionsFromList uses a list");
	return NULL;
    }

    length = 0;
    if ((argc = PyList_GET_SIZE(pyargv)) > 0)
	length = argc - options.optind;

    if (length <= 0) {
	Py_INCREF(Py_None);
	return Py_None;
    }

    if ((result = PyList_New(length)) == NULL)
	return PyErr_NoMemory();

    for (i = 0; i < length; i++) {
	PyObject *pyarg = PyList_GET_ITEM(pyargv, options.optind + i);
	PyList_SET_ITEM(result, i, pyarg);
    }
    Py_INCREF(result);
    return result;
}

static PyObject *
getOptionsFromList(PyObject *self, PyObject *args, PyObject *keywords)
{
    int i, argc;
    char **argv;
    PyObject *pyargv = NULL;
    char *keyword_list[] = {"argv", NULL};

    // Note that PyArg_ParseTupleAndKeywords() returns a "borrowed"
    // reference, so there is no need to decrement a reference to
    // pyargv.
    if (!PyArg_ParseTupleAndKeywords(args, keywords,
			"O:pmGetOptionsFromList", keyword_list, &pyargv))
	return NULL;

    if (pyargv == NULL)
	return Py_BuildValue("i", 0);

    if (!PyList_Check(pyargv)) {
	PyErr_SetString(PyExc_TypeError, "pmGetOptionsFromList uses a list");
	return NULL;
    }

    if ((argc = PyList_GET_SIZE(pyargv)) <= 0) {
	return Py_BuildValue("i", 0);
    }

    if ((argv = malloc(argc * sizeof(char *))) == NULL) {
	return PyErr_NoMemory();
    }

    for (i = 0; i < argc; i++) {
	PyObject *pyarg = PyList_GET_ITEM(pyargv, i);
#if PY_MAJOR_VERSION >= 3
	char *string = PyUnicode_AsUTF8(pyarg);
#else
	char *string = PyString_AsString(pyarg);
#endif

	/* argv[0] parameter will be used for pmProgname, so need to
	 * ensure the memory that backs it will be with us forever.
         */
	if (i == 0 && (string = strdup(string)) == NULL) {
	    free(argv);
	    return PyErr_NoMemory();
	}
	argv[i] = string;
    }

    if (overridesCallback)
	options.override = override_callback;
    while ((i = pmGetOptions(argc, argv, &options)) != -1)
	options_callback(i, &options);
    free(argv);

    if (options.flags & PM_OPTFLAG_EXIT)
	return Py_BuildValue("i", PM_ERR_APPVERSION);

    return Py_BuildValue("i", options.errors);
}

static PyObject *
setContextOptions(PyObject *self, PyObject *args, PyObject *keywords)
{
    int sts, ctx, step, mode, delta;
    char *keyword_list[] = {"context", "mode", "delta", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
		"iii:pmSetContextOptions", keyword_list, &ctx, &mode, &delta))
	return NULL;

    /* complete time window and timezone setup */
    if ((sts = pmGetContextOptions(ctx, &options)) < 0)
	return Py_BuildValue("i", sts);

    /* initial archive mode, position and delta */
    if (options.context == PM_CONTEXT_ARCHIVE &&
	(options.flags & PM_OPTFLAG_BOUNDARIES)) {
	const int SECONDS_IN_24_DAYS = 2073600;
	struct timeval interval = options.interval;
	struct timeval position = options.origin;

	if (interval.tv_sec > SECONDS_IN_24_DAYS) {
	    step = interval.tv_sec;
	    mode |= PM_XTB_SET(PM_TIME_SEC);
	} else {
	    if (interval.tv_sec == 0 && interval.tv_usec == 0)
		interval.tv_sec = delta;
	    step = interval.tv_sec * 1e3 + interval.tv_usec / 1e3;
	    mode |= PM_XTB_SET(PM_TIME_MSEC);
	}
	if ((sts = pmSetMode(mode, &position, step)) < 0) {
	    pmprintf("%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
	    options.flags |= PM_OPTFLAG_RUNTIME_ERR;
	    options.errors++;
	}
    }
    return Py_BuildValue("i", sts);
}

static void
pmnsDecodeCallback(const char *name, void *closure)
{
    PyObject *arglist, *result;

    arglist = Py_BuildValue("(s)", name);
    if (arglist == NULL)
        return;
    result = PyEval_CallObject(closure, arglist);
    Py_DECREF(arglist);
    if (!result)
        PyErr_Print();
    else
	Py_DECREF(result);
}

/*
 * This pmTraversePMNS_r wrapper is specifically so that python3
 * installs can pass out correctly decoded python strings rather
 * than byte arrays.
 */
static PyObject *
pmnsTraverse(PyObject *self, PyObject *args, PyObject *keywords)
{
    PyObject *func;
    char *keyword_list[] = {"name", "callback", NULL};
    char *name;
    int sts;

    if (!PyArg_ParseTupleAndKeywords(args, keywords,
		"sO:pmnsTraverse", keyword_list, &name, &func))
	return NULL;
    if (!PyCallable_Check(func)) {
	PyErr_SetString(PyExc_TypeError, "pmnsTraverse needs a callable");
	return NULL;
    }
    sts = pmTraversePMNS_r(name, pmnsDecodeCallback, func);
    return Py_BuildValue("i", sts);
}

static PyObject *
usageMessage(PyObject *self, PyObject *args)
{
    pmUsageMessage(&options);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setOverrideCallback(PyObject *self, PyObject *args)
{
    PyObject *func;

    if (!PyArg_ParseTuple(args, "O:pmSetOverrideCallback", &func))
	return NULL;
    if (!PyCallable_Check(func)) {
	PyErr_SetString(PyExc_TypeError,
			"pmSetOverrideCallback parameter not callable");
	return NULL;
    }
    Py_XINCREF(func);
    Py_XDECREF(overridesCallback);
    overridesCallback = func;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
setOptionCallback(PyObject *self, PyObject *args)
{
    PyObject *func;

    if (!PyArg_ParseTuple(args, "O:pmSetOptionCallback", &func))
	return NULL;
    if (!PyCallable_Check(func)) {
	PyErr_SetString(PyExc_TypeError,
			"pmSetOptionCallback parameter not callable");
	return NULL;
    }
    Py_XINCREF(func);
    Py_XDECREF(optionCallback);
    optionCallback = func;
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionErrors(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", options.errors);
}

static PyObject *
getOptionFlags(PyObject *self, PyObject *args)
{
    return Py_BuildValue("i", options.flags);
}

static PyObject *
getOptionContext(PyObject *self, PyObject *args)
{
    if (options.context > 0)
	return Py_BuildValue("i", options.context);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionHosts(PyObject *self, PyObject *args)
{
    PyObject	*result;
    int		i;

    if (options.nhosts > 0) {
	if ((result = PyList_New(options.nhosts)) == NULL)
	    return PyErr_NoMemory();
	for (i = 0; i < options.nhosts; i++) {
#if PY_MAJOR_VERSION >= 3
	    PyObject *pyent = PyUnicode_FromString(options.hosts[i]);
#else
	    PyObject *pyent = PyString_FromString(options.hosts[i]);
#endif
	    PyList_SET_ITEM(result, i, pyent);
	}
	Py_INCREF(result);
	return result;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionArchives(PyObject *self, PyObject *args)
{
    PyObject	*result;
    int		i;

    if (options.narchives > 0) {
	if ((result = PyList_New(options.narchives)) == NULL)
	    return PyErr_NoMemory();
	for (i = 0; i < options.narchives; i++) {
#if PY_MAJOR_VERSION >= 3
	    PyObject *pyent = PyUnicode_FromString(options.archives[i]);
#else
	    PyObject *pyent = PyString_FromString(options.archives[i]);
#endif
	    PyList_SET_ITEM(result, i, pyent);
	}
	Py_INCREF(result);
	return result;
    }
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionStart_sec(PyObject *self, PyObject *args)
{
    if (options.start.tv_sec || options.start.tv_usec)
	return Py_BuildValue("l", options.start.tv_sec);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionStart_usec(PyObject *self, PyObject *args)
{
    if (options.start.tv_sec || options.start.tv_usec)
	return Py_BuildValue("l", options.start.tv_usec);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionAlign_optarg(PyObject *self, PyObject *args)
{
    if (options.align_optarg)
	return Py_BuildValue("s", options.align_optarg);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionFinish_optarg(PyObject *self, PyObject *args)
{
    if (options.finish_optarg)
	return Py_BuildValue("s", options.finish_optarg);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionFinish_sec(PyObject *self, PyObject *args)
{
    if (options.finish.tv_sec || options.finish.tv_usec)
	return Py_BuildValue("l", options.finish.tv_sec);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionFinish_usec(PyObject *self, PyObject *args)
{
    if (options.finish.tv_sec || options.finish.tv_usec)
	return Py_BuildValue("l", options.finish.tv_usec);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionOrigin_sec(PyObject *self, PyObject *args)
{
    if (options.origin.tv_sec || options.origin.tv_usec)
	return Py_BuildValue("l", options.origin.tv_sec);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionOrigin_usec(PyObject *self, PyObject *args)
{
    if (options.origin.tv_sec || options.origin.tv_usec)
	return Py_BuildValue("l", options.origin.tv_usec);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionInterval_sec(PyObject *self, PyObject *args)
{
    if (options.interval.tv_sec || options.interval.tv_usec)
	return Py_BuildValue("l", options.interval.tv_sec);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionInterval_usec(PyObject *self, PyObject *args)
{
    if (options.interval.tv_sec || options.interval.tv_usec)
	return Py_BuildValue("l", options.interval.tv_usec);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionSamples(PyObject *self, PyObject *args)
{
    if (options.samples)
	return Py_BuildValue("i", options.samples);
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
getOptionTimezone(PyObject *self, PyObject *args)
{
    if (options.timezone)
	return Py_BuildValue("s", options.timezone);
    Py_INCREF(Py_None);
    return Py_None;
}


static PyMethodDef methods[] = {
    { .ml_name = "PM_XTB_SET",
	.ml_meth = (PyCFunction) setExtendedTimeBase,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "PM_XTB_GET",
	.ml_meth = (PyCFunction) getExtendedTimeBase,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmtimevalSleep",
	.ml_meth = (PyCFunction) timevalSleep,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmtimevalToReal",
	.ml_meth = (PyCFunction) timevalToReal,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmSetProcessIdentity",
	.ml_meth = (PyCFunction) setIdentity,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmResetAllOptions",
	.ml_meth = (PyCFunction) resetAllOptions,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionHeader",
	.ml_meth = (PyCFunction) setLongOptionHeader,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmSetLongOptionText",
	.ml_meth = (PyCFunction) setLongOptionText,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmSetLongOption",
	.ml_meth = (PyCFunction) setLongOption,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmSetLongOptionAlign",
	.ml_meth = (PyCFunction) setLongOptionAlign,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionArchive",
	.ml_meth = (PyCFunction) setLongOptionArchive,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionArchiveList",
	.ml_meth = (PyCFunction) setLongOptionArchiveList,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionArchiveFolio",
	.ml_meth = (PyCFunction) setLongOptionArchiveFolio,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionDebug",
	.ml_meth = (PyCFunction) setLongOptionDebug,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionGuiMode",
	.ml_meth = (PyCFunction) setLongOptionGuiMode,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionHost",
	.ml_meth = (PyCFunction) setLongOptionHost,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionHostList",
	.ml_meth = (PyCFunction) setLongOptionHostList,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionHostsFile",
	.ml_meth = (PyCFunction) setLongOptionHostsFile,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionSpecLocal",
	.ml_meth = (PyCFunction) setLongOptionSpecLocal,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionLocalPMDA",
	.ml_meth = (PyCFunction) setLongOptionLocalPMDA,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionOrigin",
	.ml_meth = (PyCFunction) setLongOptionOrigin,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionGuiPort",
	.ml_meth = (PyCFunction) setLongOptionGuiPort,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionStart",
	.ml_meth = (PyCFunction) setLongOptionStart,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionSamples",
	.ml_meth = (PyCFunction) setLongOptionSamples,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionFinish",
	.ml_meth = (PyCFunction) setLongOptionFinish,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionInterval",
	.ml_meth = (PyCFunction) setLongOptionInterval,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionVersion",
	.ml_meth = (PyCFunction) setLongOptionVersion,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionTimeZone",
	.ml_meth = (PyCFunction) setLongOptionTimeZone,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionHostZone",
	.ml_meth = (PyCFunction) setLongOptionHostZone,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetLongOptionHelp",
	.ml_meth = (PyCFunction) setLongOptionHelp,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetShortOptions",
	.ml_meth = (PyCFunction) setShortOptions,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmSetShortUsage",
	.ml_meth = (PyCFunction) setShortUsage,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmSetOptionFlags",
	.ml_meth = (PyCFunction) setOptionFlags,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmSetOptionErrors",
	.ml_meth = (PyCFunction) setOptionErrors,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmGetOptionErrors",
	.ml_meth = (PyCFunction) getOptionErrors,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionFlags",
	.ml_meth = (PyCFunction) getOptionFlags,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionsFromList",
	.ml_meth = (PyCFunction) getOptionsFromList,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmGetNonOptionsFromList",
	.ml_meth = (PyCFunction) getNonOptionsFromList,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmSetContextOptions",
	.ml_meth = (PyCFunction) setContextOptions,
        .ml_flags = METH_VARARGS | METH_KEYWORDS},
    { .ml_name = "pmUsageMessage",
	.ml_meth = (PyCFunction) usageMessage,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetOptionCallback",
	.ml_meth = (PyCFunction) setOptionCallback,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmSetOverrideCallback",
	.ml_meth = (PyCFunction) setOverrideCallback,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmGetOptionContext",
	.ml_meth = (PyCFunction) getOptionContext,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionHosts",
	.ml_meth = (PyCFunction) getOptionHosts,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionArchives",
	.ml_meth = (PyCFunction) getOptionArchives,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionStart_sec",
	.ml_meth = (PyCFunction) getOptionStart_sec,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionStart_usec",
	.ml_meth = (PyCFunction) getOptionStart_usec,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionAlign_optarg",
	.ml_meth = (PyCFunction) getOptionAlign_optarg,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionFinish_optarg",
	.ml_meth = (PyCFunction) getOptionFinish_optarg,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionFinish_sec",
	.ml_meth = (PyCFunction) getOptionFinish_sec,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionFinish_usec",
	.ml_meth = (PyCFunction) getOptionFinish_usec,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionOrigin_sec",
	.ml_meth = (PyCFunction) getOptionOrigin_sec,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionOrigin_usec",
	.ml_meth = (PyCFunction) getOptionOrigin_usec,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionInterval_sec",
	.ml_meth = (PyCFunction) getOptionInterval_sec,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmGetOptionInterval_usec",
	.ml_meth = (PyCFunction) getOptionInterval_usec,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetOptionInterval",
	.ml_meth = (PyCFunction) setOptionInterval,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmGetOptionSamples",
	.ml_meth = (PyCFunction) getOptionSamples,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetOptionSamples",
	.ml_meth = (PyCFunction) setOptionSamples,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmGetOptionTimezone",
	.ml_meth = (PyCFunction) getOptionTimezone,
        .ml_flags = METH_NOARGS },
    { .ml_name = "pmSetOptionArchiveList",
	.ml_meth = (PyCFunction) setOptionArchiveList,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmSetOptionArchiveFolio",
	.ml_meth = (PyCFunction) setOptionArchiveFolio,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmSetOptionHostList",
	.ml_meth = (PyCFunction) setOptionHostList,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { .ml_name = "pmnsTraverse",
	.ml_meth = (PyCFunction) pmnsTraverse,
        .ml_flags = METH_VARARGS | METH_KEYWORDS },
    { NULL }
};

/* called when the module is initialized. */
MOD_INIT(cpmapi)
{
    PyObject *module, *dict, *edict;

    MOD_DEF(module, "cpmapi", NULL, methods);
    if (module == NULL)
	return MOD_ERROR_VAL;

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
    dict_add(dict, "PM_TYPE_HIGHRES_EVENT", PM_TYPE_HIGHRES_EVENT);
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

    dict_add(dict, "PM_OPTFLAG_INIT", PM_OPTFLAG_INIT);
    dict_add(dict, "PM_OPTFLAG_DONE", PM_OPTFLAG_DONE);
    dict_add(dict, "PM_OPTFLAG_MULTI", PM_OPTFLAG_MULTI);
    dict_add(dict, "PM_OPTFLAG_USAGE_ERR", PM_OPTFLAG_USAGE_ERR);
    dict_add(dict, "PM_OPTFLAG_RUNTIME_ERR", PM_OPTFLAG_RUNTIME_ERR);
    dict_add(dict, "PM_OPTFLAG_EXIT", PM_OPTFLAG_EXIT);
    dict_add(dict, "PM_OPTFLAG_POSIX", PM_OPTFLAG_POSIX);
    dict_add(dict, "PM_OPTFLAG_MIXED", PM_OPTFLAG_MIXED);
    dict_add(dict, "PM_OPTFLAG_ENV_ONLY", PM_OPTFLAG_ENV_ONLY);
    dict_add(dict, "PM_OPTFLAG_LONG_ONLY", PM_OPTFLAG_LONG_ONLY);
    dict_add(dict, "PM_OPTFLAG_BOUNDARIES", PM_OPTFLAG_BOUNDARIES);
    dict_add(dict, "PM_OPTFLAG_STDOUT_TZ", PM_OPTFLAG_STDOUT_TZ);
    dict_add(dict, "PM_OPTFLAG_NOFLUSH", PM_OPTFLAG_NOFLUSH);
    dict_add(dict, "PM_OPTFLAG_QUIET", PM_OPTFLAG_QUIET);

    dict_add(dict, "PM_EVENT_FLAG_POINT",  PM_EVENT_FLAG_POINT);
    dict_add(dict, "PM_EVENT_FLAG_START",  PM_EVENT_FLAG_START);
    dict_add(dict, "PM_EVENT_FLAG_END",    PM_EVENT_FLAG_END);
    dict_add(dict, "PM_EVENT_FLAG_ID",     PM_EVENT_FLAG_ID);
    dict_add(dict, "PM_EVENT_FLAG_PARENT", PM_EVENT_FLAG_PARENT);
    dict_add(dict, "PM_EVENT_FLAG_MISSED", PM_EVENT_FLAG_MISSED);

    /*
     * for ease of maintenance make the order of the error codes
     * here the same as the output from pmerr -l
     */
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
    edict_add(dict, edict, "PM_ERR_TYPE", PM_ERR_TYPE);
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
    edict_add(dict, edict, "PM_ERR_LOGFILE", PM_ERR_LOGFILE);
    edict_add(dict, edict, "PM_ERR_NOTARCHIVE", PM_ERR_NOTARCHIVE);
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
    edict_add(dict, edict, "PM_ERR_PMDAREADY", PM_ERR_PMDAREADY);
    edict_add(dict, edict, "PM_ERR_PMDANOTREADY", PM_ERR_PMDANOTREADY);
    edict_add(dict, edict, "PM_ERR_TOOSMALL", PM_ERR_TOOSMALL);
    edict_add(dict, edict, "PM_ERR_TOOBIG", PM_ERR_TOOBIG);
    edict_add(dict, edict, "PM_ERR_THREAD", PM_ERR_THREAD);
    edict_add(dict, edict, "PM_ERR_FAULT", PM_ERR_FAULT);
    edict_add(dict, edict, "PM_ERR_NOCONTAINER", PM_ERR_NOCONTAINER);
    edict_add(dict, edict, "PM_ERR_BADSTORE", PM_ERR_BADSTORE);
    edict_add(dict, edict, "PM_ERR_NYI", PM_ERR_NYI);

    return MOD_SUCCESS_VAL(module);
}
