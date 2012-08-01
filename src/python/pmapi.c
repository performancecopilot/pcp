
/****************************************************************************\
**                                                                          **
** pmapi.c                                                                  **
**                                                                          **
** Copyright (C) 2009-2012 Michael T. Werner
**                                                                          **
** This file is part of pcp, the python extensions for SGI's Performance    **
** Co-Pilot. Pcp is free software: you can redistribute it and/or modify    **
** it under the terms of the GNU Lesser General Public License as published **
** by the Free Software Foundation, either version 3 of the License, or     **
** (at your option) any later version.                                      **
**                                                                          **
** Pcp is distributed in the hope that it will be useful, but WITHOUT ANY   **
** WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS**
** FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for **
** more details. You should have received a copy of the GNU Lesser General  **
** Public License along with pcp. If not, see <http://www.gnu.org/licenses/>**
**                                                                          **
**             -----------------------------------------------              **
**                                                                          **
** This C extension module mainly serves the purpose of loading constants   **
** from <pcp/pmapi.h> into the module dictionary. The PMAPI functions and   **
** data structures are wrapped in pcp.py, using ctypes.                     **
**                                                                          **
** The following constants and macros have not been wrapped.               **
**    - PM_XTB_FLAG                                                         **
**    - PM_XTB_SET()                                                        **
**    - PM_XTB_GET()                                                        **
**                                                                          **
\****************************************************************************/


#include <Python.h>
#include <pcp/pmapi.h>
#include <pcp/pmafm.h>

typedef union {
    int i;
    unsigned int u;
    void *v; /* for values passed in as NULL */
} intu;

#define INT_T 0
#define UNS_T 1

void dict_add( PyObject *dict, char *sym, intu val, int type, PyObject *revD )
{
    PyObject *pySym=NULL, *pyVal=NULL;

    if( type == INT_T ) {
        pyVal = PyInt_FromLong( (long) val.i );
    }

    if( type == UNS_T ) {
        pyVal = PyLong_FromUnsignedLong( (unsigned long) val.u );
    }

    if( ! pyVal ) {
        /* need some manner of appropriate error handling */
        PyErr_Clear();
    }

    if( ! PyDict_SetItemString( dict, sym, pyVal ) ) {
        /* need some manner of appropriate error handling */
        PyErr_Clear();
    }

    if( revD ) {
        pySym = PyString_FromString( sym );
        if( ! pySym ) {
            /* need some manner of appropriate error handling */
            PyErr_Clear();
        }
        if( ! PyDict_SetItem( revD, pyVal, pySym ) ) {
            /* need some manner of appropriate error handling */
            PyErr_Clear();
        }
    }

    if( pySym ) {
        Py_XDECREF(pySym);
    }
    if( pyVal ) {
        Py_XDECREF(pyVal);
    }
} 

/* module initializer */
void initpmapi( void );

static PyMethodDef methods[] = {
    {NULL, NULL}
};


/* This function is called when the module is initialized. */ 

void
initpmapi() {
    PyObject *module, *dict;
    PyObject *pmErrSymD;

    module = Py_InitModule( "pmapi", methods );

    pmErrSymD = PyDict_New();
    Py_INCREF( pmErrSymD ); 
    PyModule_AddObject( module, "pmErrSymD", pmErrSymD ); 

    dict = PyModule_GetDict( module );

    dict_add( dict, "PMAPI_VERSION_2",
              (intu) PMAPI_VERSION_2, INT_T, NULL );
    dict_add( dict, "PMAPI_VERSION",
              (intu) PMAPI_VERSION,   INT_T, NULL );

    dict_add( dict, "PM_ID_NULL",
              (intu) PM_ID_NULL,    UNS_T, NULL );
    dict_add( dict, "PM_INDOM_NULL",
              (intu) PM_INDOM_NULL, UNS_T, NULL );
    dict_add( dict, "PM_IN_NULL",
              (intu) PM_IN_NULL,    INT_T, NULL );

    dict_add( dict, "PM_NS_DEFAULT",
                     (intu) PM_NS_DEFAULT, UNS_T, NULL );

/* in pmapi.h, only LTOR is defined. RTOL is a gratuitous pedantic addition */
#ifdef HAVE_BITFIELDS_LTOR
    dict_add( dict, "HAVE_BITFIELDS_LTOR",
              (intu) HAVE_BITFIELDS_LTOR, INT_T, NULL );
    dict_add( dict, "HAVE_BITFIELDS_RTOL", (intu) 0, INT_T, NULL );
#else
    dict_add( dict, "HAVE_BITFIELDS_LTOR", (intu) 0, INT_T, NULL );
    dict_add( dict, "HAVE_BITFIELDS_RTOL", (intu) 1, INT_T, NULL );
#endif

    /* pmUnits.scaleSpace */
    dict_add( dict, "PM_SPACE_BYTE",  (intu) PM_SPACE_BYTE,  INT_T, NULL );
    dict_add( dict, "PM_SPACE_KBYTE", (intu) PM_SPACE_KBYTE, INT_T, NULL );
    dict_add( dict, "PM_SPACE_MBYTE", (intu) PM_SPACE_MBYTE, INT_T, NULL );
    dict_add( dict, "PM_SPACE_GBYTE", (intu) PM_SPACE_GBYTE, INT_T, NULL );
    dict_add( dict, "PM_SPACE_TBYTE", (intu) PM_SPACE_TBYTE, INT_T, NULL );
    dict_add( dict, "PM_SPACE_PBYTE", (intu) PM_SPACE_PBYTE, INT_T, NULL );
    dict_add( dict, "PM_SPACE_EBYTE", (intu) PM_SPACE_EBYTE, INT_T, NULL );
    /* pmUnits.scaleTime */
    dict_add( dict, "PM_TIME_NSEC", (intu) PM_TIME_NSEC, INT_T, NULL );
    dict_add( dict, "PM_TIME_USEC", (intu) PM_TIME_USEC, INT_T, NULL );
    dict_add( dict, "PM_TIME_MSEC", (intu) PM_TIME_MSEC, INT_T, NULL );
    dict_add( dict, "PM_TIME_SEC",  (intu) PM_TIME_SEC,  INT_T, NULL );
    dict_add( dict, "PM_TIME_MIN",  (intu) PM_TIME_MIN,  INT_T, NULL );
    dict_add( dict, "PM_TIME_HOUR", (intu) PM_TIME_HOUR, INT_T, NULL );
    /* pmUnits.countXXX */
    dict_add( dict, "PM_COUNT_ONE", (intu) PM_COUNT_ONE, INT_T, NULL );

    /* pmDesc.type */
    dict_add( dict, "PM_TYPE_NOSUPPORT",
              (intu) PM_TYPE_NOSUPPORT, INT_T, NULL );
    dict_add( dict, "PM_TYPE_32",     (intu) PM_TYPE_32,     INT_T, NULL );
    dict_add( dict, "PM_TYPE_U32",    (intu) PM_TYPE_U32,    INT_T, NULL );
    dict_add( dict, "PM_TYPE_64",     (intu) PM_TYPE_64,     INT_T, NULL );
    dict_add( dict, "PM_TYPE_U64",    (intu) PM_TYPE_U64,    INT_T, NULL );
    dict_add( dict, "PM_TYPE_FLOAT",  (intu) PM_TYPE_FLOAT,  INT_T, NULL );
    dict_add( dict, "PM_TYPE_DOUBLE", (intu) PM_TYPE_DOUBLE, INT_T, NULL );
    dict_add( dict, "PM_TYPE_STRING", (intu) PM_TYPE_STRING, INT_T, NULL );
    dict_add( dict, "PM_TYPE_AGGREGATE",
              (intu) PM_TYPE_AGGREGATE,        INT_T, NULL );
    dict_add( dict, "PM_TYPE_AGGREGATE_STATIC",
              (intu) PM_TYPE_AGGREGATE_STATIC, INT_T, NULL );
    dict_add( dict, "PM_TYPE_EVENT",  (intu) PM_TYPE_EVENT, INT_T, NULL );
    dict_add( dict, "PM_TYPE_UNKNOWN",
              (intu) PM_TYPE_UNKNOWN,          INT_T, NULL );

    /* pmDesc.sem */
    dict_add( dict, "PM_SEM_COUNTER",  (intu) PM_SEM_COUNTER,  INT_T, NULL );
    dict_add( dict, "PM_SEM_INSTANT",  (intu) PM_SEM_INSTANT,  INT_T, NULL );
    dict_add( dict, "PM_SEM_DISCRETE", (intu) PM_SEM_DISCRETE, INT_T, NULL );


    dict_add( dict, "PMNS_LOCAL",   (intu) PMNS_LOCAL,   INT_T, NULL );
    dict_add( dict, "PMNS_REMOTE",  (intu) PMNS_REMOTE,  INT_T, NULL );
    dict_add( dict, "PMNS_ARCHIVE", (intu) PMNS_ARCHIVE, INT_T, NULL );

    dict_add( dict, "PMNS_LEAF_STATUS",
              (intu) PMNS_LEAF_STATUS,    INT_T, NULL );
    dict_add( dict, "PMNS_NONLEAF_STATUS",
              (intu) PMNS_NONLEAF_STATUS, INT_T, NULL );

    /* context type */
    dict_add( dict, "PM_CONTEXT_ARCHIVE",
              (intu) PM_CONTEXT_ARCHIVE, INT_T, NULL );
    dict_add( dict, "PM_CONTEXT_LOCAL",
              (intu) PM_CONTEXT_LOCAL,   INT_T, NULL );
    dict_add( dict, "PM_CONTEXT_HOST",
              (intu) PM_CONTEXT_HOST,    INT_T, NULL );

    /* event type */
    dict_add( dict, "PM_EVENT_FLAG_POINT",  (intu) PM_EVENT_FLAG_POINT, INT_T, NULL );
    dict_add( dict, "PM_EVENT_FLAG_START",  (intu) PM_EVENT_FLAG_START, INT_T, NULL );
    dict_add( dict, "PM_EVENT_FLAG_END",    (intu) PM_EVENT_FLAG_END,   INT_T, NULL );
    dict_add( dict, "PM_EVENT_FLAG_ID",     (intu) PM_EVENT_FLAG_ID,    INT_T, NULL );
    dict_add( dict, "PM_EVENT_FLAG_PARENT", (intu) PM_EVENT_FLAG_PARENT, INT_T, NULL );
    dict_add( dict, "PM_EVENT_FLAG_MISSED", (intu) PM_EVENT_FLAG_MISSED, INT_T, NULL );


    dict_add( dict, "PM_VAL_HDR_SIZE", (intu) PM_VAL_HDR_SIZE, INT_T, NULL );
    dict_add( dict, "PM_VAL_VLEN_MAX", (intu) PM_VAL_VLEN_MAX, INT_T, NULL );

    /* values for valfmt in pmValueSet */
    dict_add( dict, "PM_VAL_INSITU", (intu) PM_VAL_INSITU, INT_T, NULL );
    dict_add( dict, "PM_VAL_DPTR",   (intu) PM_VAL_DPTR,   INT_T, NULL );
    dict_add( dict, "PM_VAL_SPTR",   (intu) PM_VAL_SPTR,   INT_T, NULL );

    dict_add( dict, "PMCD_NO_CHANGE",
              (intu) PMCD_NO_CHANGE,     INT_T, NULL );
    dict_add( dict, "PMCD_ADD_AGENT",
              (intu) PMCD_ADD_AGENT,     INT_T, NULL );
    dict_add( dict, "PMCD_RESTART_AGENT",
              (intu) PMCD_RESTART_AGENT, INT_T, NULL );
    dict_add( dict, "PMCD_DROP_AGENT",
              (intu) PMCD_DROP_AGENT,    INT_T, NULL );

    dict_add( dict, "PM_TZ_MAXLEN",
              (intu) PM_TZ_MAXLEN, INT_T, NULL );

    dict_add( dict, "PM_LOG_MAXHOSTLEN",
              (intu) PM_LOG_MAXHOSTLEN, INT_T, NULL );
    dict_add( dict, "PM_LOG_MAGIC",    (intu) PM_LOG_MAGIC,    INT_T, NULL );
    dict_add( dict, "PM_LOG_VERS02",   (intu) PM_LOG_VERS02,   INT_T, NULL );
    dict_add( dict, "PM_LOG_VOL_TI",   (intu) PM_LOG_VOL_TI,   INT_T, NULL );
    dict_add( dict, "PM_LOG_VOL_META", (intu) PM_LOG_VOL_META, INT_T, NULL );


    dict_add( dict, "PM_MODE_LIVE",   (intu) PM_MODE_LIVE,   INT_T, NULL );
    dict_add( dict, "PM_MODE_INTERP", (intu) PM_MODE_INTERP, INT_T, NULL );
    dict_add( dict, "PM_MODE_FORW",   (intu) PM_MODE_FORW,   INT_T, NULL );
    dict_add( dict, "PM_MODE_BACK",   (intu) PM_MODE_BACK,   INT_T, NULL );


    dict_add( dict, "PM_TEXT_ONELINE", (intu) PM_TEXT_ONELINE, INT_T, NULL );
    dict_add( dict, "PM_TEXT_HELP",    (intu) PM_TEXT_HELP,    INT_T, NULL );


    dict_add( dict, "PM_ERR_BASE2", (intu) PM_ERR_BASE2, INT_T, NULL );
    dict_add( dict, "PM_ERR_BASE",  (intu) PM_ERR_BASE,  INT_T, NULL );


    dict_add( dict, "PM_ERR_GENERIC",
              (intu) PM_ERR_GENERIC,      INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_PMNS",
              (intu) PM_ERR_PMNS,         INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_NOPMNS",
              (intu) PM_ERR_NOPMNS,       INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_DUPPMNS",
              (intu) PM_ERR_DUPPMNS,      INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_TEXT",
              (intu) PM_ERR_TEXT,         INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_APPVERSION",
              (intu) PM_ERR_APPVERSION,   INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_VALUE",
              (intu) PM_ERR_VALUE,        INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_TIMEOUT",
              (intu) PM_ERR_TIMEOUT,      INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_NODATA",
              (intu) PM_ERR_NODATA,       INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_RESET",
              (intu) PM_ERR_RESET,        INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_NAME",
              (intu) PM_ERR_NAME,         INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_PMID",
              (intu) PM_ERR_PMID,         INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_INDOM",
              (intu) PM_ERR_INDOM,        INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_INST",
              (intu) PM_ERR_INST,         INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_UNIT",
              (intu) PM_ERR_UNIT,         INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_CONV",
              (intu) PM_ERR_CONV,         INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_TRUNC",
              (intu) PM_ERR_TRUNC,        INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_SIGN",
              (intu) PM_ERR_SIGN,         INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_PROFILE",
              (intu) PM_ERR_PROFILE,      INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_IPC",
              (intu) PM_ERR_IPC,          INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_EOF",
              (intu) PM_ERR_EOF,          INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_NOTHOST",
              (intu) PM_ERR_NOTHOST,      INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_EOL",
              (intu) PM_ERR_EOL,          INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_MODE",
              (intu) PM_ERR_MODE,         INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_LABEL",
              (intu) PM_ERR_LABEL,        INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_LOGREC",
              (intu) PM_ERR_LOGREC,       INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_NOTARCHIVE",
              (intu) PM_ERR_NOTARCHIVE,   INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_LOGFILE",
              (intu) PM_ERR_LOGFILE,      INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_NOCONTEXT",
              (intu) PM_ERR_NOCONTEXT,    INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_PROFILESPEC",
              (intu) PM_ERR_PROFILESPEC,  INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_PMID_LOG",
              (intu) PM_ERR_PMID_LOG,     INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_INDOM_LOG",
              (intu) PM_ERR_INDOM_LOG,    INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_INST_LOG",
              (intu) PM_ERR_INST_LOG,     INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_NOPROFILE",
              (intu) PM_ERR_NOPROFILE,    INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_NOAGENT",
              (intu) PM_ERR_NOAGENT,      INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_PERMISSION",
              (intu) PM_ERR_PERMISSION,   INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_CONNLIMIT",
              (intu) PM_ERR_CONNLIMIT,    INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_AGAIN",
              (intu) PM_ERR_AGAIN,        INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_ISCONN",
              (intu) PM_ERR_ISCONN,       INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_NOTCONN",
              (intu) PM_ERR_NOTCONN,      INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_NEEDPORT",
              (intu) PM_ERR_NEEDPORT,     INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_NONLEAF",
              (intu) PM_ERR_NONLEAF,      INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_TYPE",
              (intu) PM_ERR_TYPE,         INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_THREAD",
              (intu) PM_ERR_THREAD,       INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_TOOSMALL",
              (intu) PM_ERR_TOOSMALL,     INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_TOOBIG",
              (intu) PM_ERR_TOOBIG,       INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_FAULT",
              (intu) PM_ERR_FAULT,        INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_PMDAREADY",
              (intu) PM_ERR_PMDAREADY,    INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_PMDANOTREADY",
              (intu) PM_ERR_PMDANOTREADY, INT_T, pmErrSymD );
    dict_add( dict, "PM_ERR_NYI",
              (intu) PM_ERR_NYI,          INT_T, pmErrSymD );

    /* pmapi.h */
    dict_add( dict, "PM_EVENT_FLAG_POINT",  (intu) PM_EVENT_FLAG_POINT, INT_T, NULL );
    dict_add( dict, "PM_REC_ON", (intu) PM_REC_ON, INT_T, NULL );
    dict_add( dict, "PM_REC_OFF", (intu) PM_REC_OFF, INT_T, NULL );
    dict_add( dict, "PM_REC_DETACH", (intu) PM_REC_DETACH, INT_T, NULL );
    dict_add( dict, "PM_REC_STATUS", (intu) PM_REC_STATUS, INT_T, NULL );
    dict_add( dict, "PM_REC_SETARG", (intu) PM_REC_SETARG, INT_T, NULL );

}
