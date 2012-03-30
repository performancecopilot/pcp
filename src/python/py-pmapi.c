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
  char *metric_name;
  int sts;

  PyObject* names = PyTuple_GetItem(args, 0);
  PyObject* name = NULL;
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

// Python Usage: pcp.pmLookupDesc (pmName) OR ((pmName1, pmName2..))
// Description: Equivalent to pmLookupDesc.  The arguments are returned 
// by pcp.pmLookupName
// Returns: tuple of metric descriptors

static PyObject* pypmLookupDesc(PyObject* self, PyObject* args)
{
  int sts;

  PyObject *pmids = PyTuple_GetItem(args, 0);
  PyObject *pmid = NULL;
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
  PyObject *pmid = NULL;
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

// Python Usage: pcp.pmExtractValue (pmResult, pmDesc, pmResultIdx, pmDescIdx, 
// 				     PYTHON_TYPES)
// Description: Roughly equivalent to pmExtractValue.
// Returns: metric result scalar

static PyObject* 
pypmExtractValue(PyObject* self, PyObject* args)
{
  long result;
  long desc;
  int result_idx;
  int vlist_idx;
  char *type;
  pmDesc	*pmdesc;	/* XXX allow tuple */
  pmResult	*pmresult;
  pmAtomValue   atom;
  
  if (!PyArg_ParseTuple (args, "lliis", &result, &desc, &result_idx,
			 &vlist_idx, &type))
    return NULL;

  pmresult = (pmResult*)result;
  int i;
  pmdesc = (pmDesc*)desc;

  if (strcmp (type, "PM_TYPE_U32") == 0)
    {
      pmExtractValue(pmresult->vset[result_idx]->valfmt,
		     &pmresult->vset[result_idx]->vlist[vlist_idx],
		     pmdesc->type,
		     &atom, PM_TYPE_U32);
      return Py_BuildValue("l", atom.ul); /* XXX build tuple */
    }
  else if (strcmp (type, "PM_TYPE_FLOAT") == 0)
    {
      pmExtractValue(pmresult->vset[result_idx]->valfmt,
		     &pmresult->vset[result_idx]->vlist[vlist_idx],
		     pmdesc->type,
		     &atom, PM_TYPE_FLOAT);
      return Py_BuildValue("f", atom.f); /* XXX build tuple */
    }
}

static char pcp_docs[] =
    "pcp( ): PCP ABI extension module\n";

static PyMethodDef pcp_funcs[] = {
    {"pmNewContext", (PyCFunction)pypmNewContext, METH_VARARGS, pcp_docs},
    {"pmLookupName", (PyCFunction)pypmLookupName, METH_VARARGS, pcp_docs},
    {"pmLookupDesc", (PyCFunction)pypmLookupDesc, METH_VARARGS, pcp_docs},
    {"pmFetch", (PyCFunction)pypmFetch, METH_VARARGS, pcp_docs},
    {"pmExtractValue", (PyCFunction)pypmExtractValue, METH_VARARGS, pcp_docs},
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

