#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <python2.7/Python.h>

// Usage: pmNewContext()
// Description: This must be called to do setup prior to any other calls.

typedef enum {TUPLE, STRING, LONG, NONE} PYTHON_TYPES;

static PyObject* 
pypmNewContext(PyObject* self)
{
  char local[64];
  char *host;
  int sts;
  int type;

  type = PM_CONTEXT_HOST;
  (void)gethostname(local, MAXHOSTNAMELEN);
  local[MAXHOSTNAMELEN-1] = '\0';
  host = local;
  sts = pmNewContext(type, host);
  if (sts < 0)
    {
      PyErr_SetString(PyExc_RuntimeError, "PCP is not running");
      return 0;
    }
  return Py_BuildValue("i", sts);
}

// Usage: python_type(PyObject)
// Description: Returns a string describing the type of python object

static PYTHON_TYPES
python_type (PyObject* this)
{
  PyObject *pytupleobject = PyTuple_New(0);
  PyObject *pystringobject = PyString_FromString("");
  PyObject *pylongobject = PyLong_FromLong(0L);
  
  if (this->ob_type == pytupleobject->ob_type)
    return TUPLE;
  else if (this->ob_type == pystringobject->ob_type)
    return STRING;
  else if (this->ob_type == pylongobject->ob_type)
    return LONG;
  return NONE;
}

// Python Usage: pcp.pmLookupName ("metric") OR (("metric1", "metric2"..))
// Description: Equivalent to pmLookupName.  Arg can be a string or tuple.
// Returns: tuple of name descriptors

static PyObject* pypmLookupName(PyObject* self, PyObject* args)
{
  int sts;

  PyObject* names = PyTuple_GetItem(args, 0);
  int pm_size;
  if (python_type (names) == STRING)
    pm_size = 1;
  else if (python_type (names) == TUPLE)
    pm_size = PyTuple_Size (names);
  {
    char *pmclient[pm_size];
    pmID pmidlist[pm_size];
    int i;

    if (python_type (names) == STRING)
      pmclient[0] = PyString_AsString (names);
    else if (python_type (names) == TUPLE)
      for (i = 0; i < pm_size; i++)
	pmclient[i] = PyString_AsString (PyTuple_GetItem(names, i));
    
    if ((sts = pmLookupName (pm_size, pmclient, pmidlist)) < 0)
      return Py_BuildValue("i", sts);
    
    else
      {
	PyObject *pypmidlist = PyTuple_New (pm_size);
	for (i = 0; i < pm_size; i++)
	  PyTuple_SetItem (pypmidlist, i, PyLong_FromLong ((long)pmidlist[i]));
	
	return pypmidlist;
      }
  }
}

// Python Usage: pcp.pmIdStr (ID) OR ((ID1, ID2..))
// Description: Equivalent to pmIdStr.  Arg can be an id or tuple of ids 
// as returned by pmLookupName
// Returns: tuple of name string descriptions

static PyObject* pypmIdStr(PyObject* self, PyObject* args)
{
  PyObject *pmids = PyTuple_GetItem(args, 0);
  int pm_size;
  if (python_type (pmids) == LONG)
    pm_size = 1;
  else if (python_type (pmids) == TUPLE)
    pm_size = PyTuple_Size (pmids);

  {
    long pmid[pm_size];
    int i;

    if (python_type (pmids) == LONG)
      pmid[0] = PyLong_AsLong (pmids);
    else if (python_type (pmids) == TUPLE)
      for (i = 0; i < pm_size; i++)
	pmid[i] = PyLong_AsLong (PyTuple_GetItem(pmids, i));
    
    PyObject *pypmidstringlist = PyTuple_New (pm_size);
    
    for (i = 0; i < pm_size; i++)
      PyTuple_SetItem (pypmidstringlist, i, PyString_FromString (pmIDStr (pmid[i])));
    return pypmidstringlist;
  }
}

// Python Usage: pcp.pmLookupDesc (pmName) OR ((pmName1, pmName2..))
// Description: Equivalent to pmLookupDesc.  The arguments are returned 
// by pcp.pmLookupName
// Returns: tuple of metric descriptors

static PyObject* pypmLookupDesc(PyObject* self, PyObject* args)
{
  int sts;

  PyObject *pmids = PyTuple_GetItem(args, 0);
  int pm_size;
  if (python_type (pmids) == LONG)
    pm_size = 1;
  else if (python_type (pmids) == TUPLE)
    pm_size = PyTuple_Size (pmids);

  {
    pmDesc (*pmdesclist)[pm_size];
    long pmid[pm_size];
    int i;

    pmdesclist = malloc (sizeof(pmDesc) * pm_size);
    if (python_type (pmids) == LONG)
      pmid[0] = PyLong_AsLong (pmids);
    else if (python_type (pmids) == TUPLE)
      for (i = 0; i < pm_size; i++)
	pmid[i] = PyLong_AsLong (PyTuple_GetItem(pmids, i));
    
    for (i = 0; i < pm_size; i++)
      if ((sts = pmLookupDesc (pmid[i], &((*pmdesclist)[i]))) < 0)
	return Py_BuildValue("i", sts);
    
    PyObject *pypmdesclist = PyTuple_New (pm_size);
    for (i = 0; i < pm_size; i++)
      {
	long desc = (long)(&(*pmdesclist)[i]);
	PyTuple_SetItem (pypmdesclist, i, PyLong_FromLong (desc));
      }
    return pypmdesclist;
  }
}

// Python Usage: pcp.pmFetch (pmName) OR ((pmName1, pmName2..))
// Description: Equivalent to pmFetch.  The arguments are returned 
// by pcp.pmLookupName
// Returns: Pointer to metric result struct

pmResult *pmresult;

static PyObject* pypmFetch(PyObject* self, PyObject* args)
{
  int sts;

  PyObject *pmids = PyTuple_GetItem(args, 0);
  int pm_size;
  if (python_type (pmids) == LONG)
    pm_size = 1;
  else if (python_type (pmids) == TUPLE)
    pm_size = PyTuple_Size (pmids);

  {
    long pmid[pm_size];
    int i;

    if (python_type (pmids) == LONG)
      pmid[0] = PyLong_AsLong (pmids);
    else if (python_type (pmids) == TUPLE)
      for (i = 0; i < pm_size; i++)
	pmid[i] = PyLong_AsLong (PyTuple_GetItem(pmids, i));
    
    if ((sts = pmFetch (pm_size, (pmID*)pmid, &pmresult)) < 0)
      return Py_BuildValue("i", sts);

    PyObject *pylongobject = PyLong_FromLong((long)pmresult);
    return pylongobject;
  }
}

// Python Usage: pcp.pmExtractValue (pmResult, pmDesc, pmName[I], pmDescIdx, 
// 				     PYTHON_TYPES)
// Description: Roughly equivalent to pmExtractValue.
// pmResult is result of pcp.pmFetch
// pmDesc is result of pcp.pmLookupDesc
// pmName[I] is the metric being extracted
// pmDescIdx is the index of the desired pmDesc
// PYTHON_TYPES is the type of the metric, "PM_TYPE_U32" or "PM_TYPE_FLOAT"
// Returns: metric result scalar

static PyObject* 
pypmExtractValue(PyObject* self, PyObject* args)
{
  long result;
  int result_idx;
  int vlist_idx;
  char *type;
  PyObject *desc;
  pmResult *pmresult;
  pmAtomValue atom;

  result = PyLong_AsLong (PyTuple_GetItem(args, 0));
  desc = (PyObject*)PyTuple_GetItem(args, 1);
  result_idx = PyLong_AsLong (PyTuple_GetItem(args, 2));
  vlist_idx = PyLong_AsLong (PyTuple_GetItem(args, 3));
  type = PyString_AsString (PyTuple_GetItem(args, 4));

  pmresult = (pmResult*)result;
  int i;

   if (strcmp (type, "PM_TYPE_U32") == 0)
    {
      for (i = 0; i < pmresult->numpmid; i++)
	if (pmresult->vset[i]->pmid == result_idx)
	  pmExtractValue(pmresult->vset[i]->valfmt,
			 &pmresult->vset[i]->vlist[vlist_idx],
			 ((pmDesc*)(PyLong_AsLong(PyTuple_GetItem (desc, i))))->type,
			 &atom, PM_TYPE_U32);
      return Py_BuildValue("l", atom.ul); /* XXX build tuple */
    }
  else if (strcmp (type, "PM_TYPE_FLOAT") == 0)
    {
      for (i = 0; i < pmresult->numpmid; i++)
	if (pmresult->vset[i]->pmid == result_idx)
	  pmExtractValue(pmresult->vset[i]->valfmt,
			 &pmresult->vset[i]->vlist[vlist_idx],
			 ((pmDesc*)(PyLong_AsLong(PyTuple_GetItem (desc, i))))->type,
			 &atom, PM_TYPE_FLOAT);
      return Py_BuildValue("f", atom.f); /* XXX build tuple */
    }
  return NULL;
}

// Python Usage: pcp.pmFreeResult (R) OR ((R1, R2..))
// Description: Equivalent to pmFreeResult.

static PyObject* pypmFreeResult(PyObject* self, PyObject* args)
{
  PyObject *pmresults = PyTuple_GetItem(args, 0);
  int pm_size;
  if (python_type (pmresults) == LONG)
    pm_size = 1;
  else if (python_type (pmresults) == TUPLE)
    pm_size = PyTuple_Size (pmresults);

  {
    int i;

    if (python_type (pmresults) == LONG)
      pmFreeResult ((pmResult*)PyLong_AsLong (pmresults));
    else if (python_type (pmresults) == TUPLE)
      for (i = 0; i < pm_size; i++)
	pmFreeResult ((pmResult*)PyLong_AsLong (PyTuple_GetItem(pmresults, i)));

    return NULL;
  }
}


// Python Usage: pcp.pmIdStr (ID)
// Description: Equivalent to pmErrStr.  Arg is an error code

static PyObject* 
pypmErrStr(PyObject* self, PyObject* args)
{
  return PyString_FromString (pmErrStr(PyLong_AsLong(PyTuple_GetItem(args, 0))));
}

// Python Usage: pcp.pmAddProfile (D, I) OR ((D1, D2..), I)
// Description: Equivalent to pmAddProfile.  Arg1 can be a descriptor or tuple
// of descriptors as returned by pcp.pmLookupDesc.  Arg2 is an instance as
// returned by pcp.pmLookupInDom

static PyObject*
pypmAddProfile(PyObject* self, PyObject* args)
{
  pmDesc *desc = (pmDesc*)PyTuple_GetItem(args, 0);
  PyObject *inst = (PyObject*)PyTuple_GetItem(args, 1);
  int i;
  int pm_size;

  if (python_type (inst) == TUPLE)
    {
      pm_size = PyTuple_Size (inst);

      for (i = 0; i < pm_size; i++)
	pmAddProfile (desc->indom, pm_size, (int*)PyLong_AsLong (PyTuple_GetItem(inst, i)));
    }
  
  return NULL;
}

// Python Usage: pcp.pmDelProfile (D, I) OR ((D1, D2..), I)
// Description: Equivalent to pmDelProfile.  Arg1 can be a descriptor or tuple
// of descriptors as returned by pcp.pmLookupDesc.  Arg2 is an instance as
// returned by pcp.pmLookupInDom

static PyObject*
pypmDelProfile(PyObject* self, PyObject* args)
{
  pmDesc *desc = (pmDesc*)PyTuple_GetItem(args, 0);
  PyObject *inst = (PyObject*)PyTuple_GetItem(args, 1);
  int i;
  int pm_size;

  if (python_type (inst) == TUPLE)
    {
      pm_size = PyTuple_Size (inst);

      for (i = 0; i < pm_size; i++)
	pmDelProfile (desc->indom, pm_size, (int*)PyLong_AsLong (PyTuple_GetItem(inst, i)));
    }
  
  return NULL;
}

// Python Usage: pcp.pmLookupInDom (D, I) OR ((D1, D2..), I)
// Description: Equivalent to pmAddProfile.  Arg1 is a descriptor.
// Arg2 is a string instance description
// Returned is the instance id

static PyObject*
pypmLookupInDom(PyObject* self, PyObject* args)
{
  pmDesc *desc = (pmDesc*)PyTuple_GetItem(args, 0);
  PyObject *inst = (PyObject*)PyTuple_GetItem(args, 1);

  if (python_type (inst) == TUPLE)
    {
      int this_inst = pmLookupInDom (desc->indom, PyString_AsString(inst));
      return Py_BuildValue("i", this_inst);
    }
  
  return NULL;
}

static PyObject*
pypmParseInterval(PyObject* self, PyObject* args)
{
  return NULL;
}

static PyObject*
pypmConvScale(PyObject* self, PyObject* args)
{
  return NULL;
}

static PyObject*
pypmLoadNameSpace(PyObject* self, PyObject* args)
{
  return NULL;
}

static PyObject*
pypmGetArchiveLabel(PyObject* self, PyObject* args)
{
  return NULL;
}

static PyObject*
pypmNewContextZone(PyObject* self, PyObject* args)
{
  return NULL;
}

static PyObject*
pypmNewZone(PyObject* self, PyObject* args)
{
  return NULL;
}

static PyObject*
pypmSetMode(PyObject* self, PyObject* args)
{
  return NULL;
}

static PyObject*
pypmCtime(PyObject* self, PyObject* args)
{
  return NULL;
}

static char pcp_docs[] =
    "pcp( ): PCP ABI extension module\n";

static PyMethodDef pcp_funcs[] = {
    {"pmNewContext", (PyCFunction)pypmNewContext, METH_VARARGS, pcp_docs},
    {"pmLookupName", (PyCFunction)pypmLookupName, METH_VARARGS, pcp_docs},
    {"pmLookupDesc", (PyCFunction)pypmLookupDesc, METH_VARARGS, pcp_docs},
    {"pmFetch", (PyCFunction)pypmFetch, METH_VARARGS, pcp_docs},
    {"pmExtractValue", (PyCFunction)pypmExtractValue, METH_VARARGS, pcp_docs},
    {"pmErrStr", (PyCFunction)pypmErrStr, METH_VARARGS, pcp_docs},
    {"pmIdStr", (PyCFunction)pypmIdStr, METH_VARARGS, pcp_docs},
    {"pmFreeResult", (PyCFunction)pypmFreeResult, METH_VARARGS, pcp_docs},
    {"pmDelProfile", (PyCFunction)pypmDelProfile, METH_VARARGS, pcp_docs},
    {"pmAddProfile", (PyCFunction)pypmAddProfile, METH_VARARGS, pcp_docs},
    {"pmLookupInDom", (PyCFunction)pypmLookupInDom, METH_VARARGS, pcp_docs},
    {"pmConvScale", (PyCFunction)pypmConvScale, METH_VARARGS, pcp_docs},
    {"pmParseInterval", (PyCFunction)pypmParseInterval, METH_VARARGS, pcp_docs},
    {"pmLoadNameSpace", (PyCFunction)pypmLoadNameSpace, METH_VARARGS, pcp_docs},
    {"pmGetArchiveLabel", (PyCFunction)pypmGetArchiveLabel, METH_VARARGS, pcp_docs},
    {"pmNewContextZone", (PyCFunction)pypmNewContextZone, METH_VARARGS, pcp_docs},
    {"pmNewZone", (PyCFunction)pypmNewZone, METH_VARARGS, pcp_docs},
    {"pmSetMode", (PyCFunction)pypmSetMode, METH_VARARGS, pcp_docs},
    {"pmCtime", (PyCFunction)pypmCtime, METH_VARARGS, pcp_docs},

    {NULL}
};

void initpcp(void)
{
    Py_InitModule3("pcp", pcp_funcs, "PCP ABI extension module");
}

/* Example Usage
import pcp
pcp.pmNewContext()
metrics = ("kernel.all.load", "kernel.percpu.cpu.user", "kernel.percpu.cpu.sys",
	   "mem.freemem", "disk.all.total")
metric_names = pcp.pmLookupName(metrics)
metric_descs = pcp.pmLookupDesc(metric_names)
metric_results = pcp.pmFetch(metric_names)
user_cpu = pcp.pmExtractValue(metric_results, metric_descs[1], 2, cpu, "PM_TYPE_FLOAT")
*/
