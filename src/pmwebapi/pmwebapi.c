/*
 * JSON web bridge for PMAPI.
 *
 * Copyright (c) 2011-2013 Red Hat Inc.
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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "pmwebapi.h"
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <ctype.h>


/* ------------------------------------------------------------------------ */

struct webcontext {
    /* __pmHashNode key is     unique randomized web context key, [1,INT_MAX] */
    /* XXX: optionally bind session to a particular IP address? */
    char      *userid;      /* NULL, or strdup'd username */
    char      *password;    /* NULL, or strdup'd password */
    unsigned  mypolltimeout;
    time_t    expires;      /* poll timeout, 0 if never expires */
    int       context;      /* PMAPI context handle, 0 if deleted/free */
};

/* if-threaded: pthread_mutex_t context_lock; */
static __pmHashCtl contexts;



struct web_gc_iteration {
    time_t now;
    time_t soonest; /* 0 => no context pending gc */
};


static __pmHashWalkState
pmwebapi_gc_fn (const __pmHashNode *kv, void *cdata)
{
    const struct webcontext *value = kv->data;
    struct web_gc_iteration *t = (struct web_gc_iteration *) cdata;

    if (! value->expires)
        return PM_HASH_WALK_NEXT;

    /* Expired. */
    if (value->expires < t->now)
        {
            int rc;
            if (verbosity)
                __pmNotifyErr (LOG_INFO, "context (web%d=pm%d) expired.\n", kv->key, value->context);
            rc = pmDestroyContext (value->context);
            if (rc)
                __pmNotifyErr (LOG_ERR, "pmDestroyContext (%d) failed: %d\n",
                               value->context, rc);

            free (value->userid);
            free (value->password);
    
            return PM_HASH_WALK_DELETE_NEXT;
        }

    /* Expiring soon? */
    if (t->soonest == 0) 
        t->soonest = value->expires;
    else if (t->soonest > value->expires)
        t->soonest = value->expires;

    return PM_HASH_WALK_NEXT;
}


/* Check whether any contexts have been unpolled so long that they
   should be considered abandoned.  If so, close 'em, free 'em, yak
   'em, smack 'em.  Return the number of seconds to the next good time
   to check for garbage. */
unsigned pmwebapi_gc ()
{
    struct web_gc_iteration t;
    (void) time (& t.now);
    t.soonest = 0;

    /* if-multithread: Lock contexts. */
    __pmHashWalkCB (pmwebapi_gc_fn, & t, & contexts);
    /* if-multithread: Unlock contexts. */

    return t.soonest ? (unsigned)(t.soonest - t.now) : maxtimeout;
}



static __pmHashWalkState
pmwebapi_deallocate_all_fn (const __pmHashNode *kv, void *cdata)
{
    struct webcontext *value = kv->data;
    int rc;
    (void) cdata;

    if (verbosity)
        __pmNotifyErr (LOG_INFO, "context (web%d=pm%d) deleted.\n", kv->key, value->context);
    rc = pmDestroyContext (value->context);
    if (rc)
        __pmNotifyErr (LOG_ERR, "pmDestroyContext (%d) failed: %d\n",
                       value->context, rc);
    free (value->userid);
    free (value->password);
    free (value);
    return PM_HASH_WALK_DELETE_NEXT;
}


void pmwebapi_deallocate_all ()
{
    /* if-multithread: Lock contexts. */
    __pmHashWalkCB (pmwebapi_deallocate_all_fn, NULL, & contexts);
    /* if-multithread: Unlock contexts. */
}


/* Allocate an zeroed webcontext structure, and enroll it in the
   hash table with the given context#. */ 
static int webcontext_allocate (int webapi_ctx, struct webcontext** wc)
{
    if (__pmHashSearch (webapi_ctx, & contexts) != NULL)
        return -EEXIST;
  
    /* Allocate & clear our webapi context. */
    assert (wc);
    *wc = calloc(1, sizeof(struct webcontext));
    if (! *wc)
        return -ENOMEM;

    int rc = __pmHashAdd(webapi_ctx, *wc, & contexts);
    if (rc < 0) {
        free (*wc);
        return rc;
    }

    return 0;
}


int pmwebapi_bind_permanent (int webapi_ctx, int pcp_context)
{
    struct webcontext* c;
    int rc = webcontext_allocate (webapi_ctx, &c);

    if (rc < 0)
        return rc;

    assert (c);
    assert (pcp_context >= 0);
    c->context = pcp_context;
    c->mypolltimeout = ~0;
    c->expires = 0;

    return 0;
}


/* Print a best-effort message as a plain-text http response; anything
   to avoid a dreaded MHD_NO, which results in a 500 return code.
   That in turn could be interpreted by a web client as an invitation
   to try, try again. */
static int pmwebapi_notify_error (struct MHD_Connection *connection, int rc)
{
    char error_message [1000];
    char pmmsg [PM_MAXERRMSGLEN];
    struct MHD_Response *resp;

    (void) pmErrStr_r (rc, pmmsg, sizeof(pmmsg));
    (void) snprintf (error_message, sizeof(error_message),
                     "PMWEBAPI error, code %d: %s", rc, pmmsg);
    resp = MHD_create_response_from_buffer (strlen(error_message), error_message,
                                            MHD_RESPMEM_MUST_COPY);
    if (resp == NULL) {
        pmweb_notify (LOG_ERR, connection, "MHD_create_response_from_buffer failed\n");
        return MHD_NO;
    }

    (void) MHD_add_response_header (resp, "Content-Type", "text/plain");

    rc = MHD_queue_response (connection, MHD_HTTP_BAD_REQUEST, resp);
    MHD_destroy_response (resp);

    if (rc != MHD_YES) {
        pmweb_notify (LOG_ERR, connection, "MHD_queue_response failed\n");
        return MHD_NO;
    }

    return MHD_YES;
}


static int pmwebapi_respond_new_context (struct MHD_Connection *connection)
{
    /* Create a context. */
    const char *val;
    int rc = 0;
    int context = -EINVAL;
    char http_response [30];
    char context_description [512] = "<none>"; /* for logging */
    unsigned polltimeout;
    struct MHD_Response *resp;
    int webapi_ctx;
    int iterations = 0;
    char *userid = NULL;
    char *password = NULL;

    val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "hostspec");
    if (val == NULL) /* backward compatibility alias */
        val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "hostname");
    if (val) {
        pmHostSpec *hostSpec;
        int hostSpecCount;
        __pmHashCtl hostAttrs;
        char *hostAttrsError;

	__pmHashInit (& hostAttrs);
        rc = __pmParseHostAttrsSpec (val, & hostSpec, & hostSpecCount,
                                     & hostAttrs, & hostAttrsError);
        if (rc == 0) {
            __pmHashNode *node;
            node = __pmHashSearch (PCP_ATTR_USERNAME, & hostAttrs); /* XXX: PCP_ATTR_AUTHNAME? */
            if (node) userid = strdup (node->data);
            node = __pmHashSearch (PCP_ATTR_PASSWORD, & hostAttrs);
            if (node) password = strdup (node->data);
            __pmFreeHostAttrsSpec (hostSpec, hostSpecCount, & hostAttrs);
	    __pmHashClear (& hostAttrs);
        } else {
            /* Ignore the parse error at this stage; pmNewContext will give it to us. */
	    free (hostAttrsError);
        }
        
        context = pmNewContext (PM_CONTEXT_HOST, val); /* XXX: limit access */
        snprintf (context_description, sizeof(context_description), "PM_CONTEXT_HOST %s", val);
    } else {
        val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "archivefile");
        if (val) {
            char archive_fullpath[PATH_MAX];

            snprintf(archive_fullpath, sizeof(archive_fullpath),
                     "%s%c%s", archivesdir, __pmPathSeparator(), val);

            /* Block some basic ways of escaping archive_dir */ 
            if (NULL != strstr (archive_fullpath, "/../")) {
                pmweb_notify (LOG_ERR, connection, "pmwebapi suspicious archive path %s\n", 
                              archive_fullpath);
                rc = -EINVAL;
                goto out;
            }

            context = pmNewContext (PM_CONTEXT_ARCHIVE, archive_fullpath);
            snprintf (context_description, sizeof(context_description), "PM_CONTEXT_ARCHIVE %s", val);
        } else if (MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "local")) {
            /* Note we need to use a dummy parameter to local=FOO,
               since the MHD_lookup* API does not differentiate
               between an absent argument vs. an argument given
               without a parameter value. */
            context = pmNewContext (PM_CONTEXT_LOCAL, NULL);
            snprintf (context_description, sizeof(context_description), "PM_CONTEXT_LOCAL");
        } else {
            /* context remains -something */
        }
    }

    if (context < 0) {
        pmweb_notify (LOG_ERR, connection, "new context failed (%s)\n", pmErrStr (context));
        rc = context;
        goto out;
    }

    /* Process optional ?polltimeout=SECONDS field.  If broken/missing, assume maxtimeout. */
    val = MHD_lookup_connection_value (connection,  MHD_GET_ARGUMENT_KIND, "polltimeout");
    if (val) {
        long pt;
        char *endptr;
        errno = 0;
        pt = strtol(val, &endptr, 0);
        if (errno != 0 || *endptr != '\0' || pt <= 0 || pt > (long)maxtimeout) {
            polltimeout = maxtimeout;
        }
        else
            polltimeout = (unsigned) pt;
    } else {
        polltimeout = maxtimeout;
    }


    struct webcontext* c = NULL;
    /* Create a new context key for the webapi.  We just use a random integer within
       a reasonable range: 1..INT_MAX */
    while (1) {
        /* Preclude infinite looping here, for example due to a badly behaving
           random(3) implementation. */
        iterations ++;
        if (iterations > 100)
            {
                pmweb_notify (LOG_ERR, connection, "webapi_ctx allocation failed\n");
                pmDestroyContext (context);
                rc = -EMFILE;
                goto out;
            }
    
        webapi_ctx = random(); /* we hope RAND_MAX is large enough */
        if (webapi_ctx <= 0)
            continue;

        rc = webcontext_allocate (webapi_ctx, & c);
        if (rc == 0)
            break;
        /* This may already exist.  We loop in case the key id already exists. */
    }

    assert (c);
    c->context = context;
    time (& c->expires);
    c->mypolltimeout = polltimeout;
    c->expires += c->mypolltimeout;
    c->userid = userid; /* may be NULL; otherwise, will be freed at context release. */
    c->password = password; /* ditto */

    /* Errors beyond this point don't require instant cleanup; the
       periodic context GC will do it all. */
  
    if (verbosity) {
        const char* context_hostname = pmGetContextHostName (context);
        pmweb_notify (LOG_INFO, connection, "context %s%s%s%s (web%d=pm%d) created%s%s, expires in %us.\n",
                      context_description, 
                      context_hostname ? " (" : "",
                      context_hostname ? context_hostname : "",
                      context_hostname ? ")" : "",
                      webapi_ctx, context, 
                      userid ? ", user=" : "",
                      userid ? userid : "",
                      polltimeout);
    }
  
    rc = snprintf (http_response, sizeof(http_response), "{ \"context\": %d }", webapi_ctx);
    assert (rc >= 0 && rc < (int)sizeof(http_response));
    resp = MHD_create_response_from_buffer (strlen(http_response), http_response,
                                            MHD_RESPMEM_MUST_COPY);
    if (resp == NULL) {
        pmweb_notify (LOG_ERR, connection, "MHD_create_response_from_buffer failed\n");
        rc = -ENOMEM;
        goto out;
    }
    rc = MHD_add_response_header (resp, "Content-Type", "application/json");
    if (rc != MHD_YES) {
        MHD_destroy_response (resp);
        rc = -ENOMEM;
        pmweb_notify (LOG_ERR, connection, "MHD_add_response_header failed\n");
        goto out;
    }

    rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
    MHD_destroy_response (resp);
    if (rc != MHD_YES) {
        pmweb_notify (LOG_ERR, connection, "MHD_queue_response failed\n");
        rc = -ENOMEM;
        goto out;
    }

    return MHD_YES;

 out:
    return pmwebapi_notify_error (connection, rc);
}


/* ------------------------------------------------------------------------ */

/* Buffer for building a MHD_Response incrementally.  libmicrohttpd does not
   provide such a facility natively. */
struct mhdb {
    size_t buf_size;
    size_t buf_used;
    char *buf;   /* malloc/realloc()'d.  If NULL, mhdbuf is a loser: 
                    Attempt no landings there. */
};


/* Create a MHD response buffer of given initial size.  Upon failure,
   leave the buffer unallocated, which will block any mhdb_printfs,
   and cause mhdb_fini_response to fail. */
static void mhdb_init (struct mhdb *md, size_t size)
{
    md->buf_size = size;
    md->buf_used = 0;
    md->buf = malloc (md->buf_size); /* may be NULL => loser */
}


/* Add a given formatted string to the end of the MHD response buffer.
   Extend/realloc the buffer if needed.  If unable, free the buffer,
   which will block any further mhdb_vsprintfs, and cause the
   mhdb_fini_response to fail. */
static void mhdb_printf(struct mhdb *md, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void mhdb_printf(struct mhdb *md, const char *fmt, ...)
{
    va_list vl;
    int n;
    int buf_remaining = md->buf_size - md->buf_used;

    if (md->buf == NULL) return; /* reject loser */

    va_start (vl, fmt);
    n = vsnprintf (md->buf + md->buf_used, buf_remaining, fmt, vl);
    va_end (vl);

    if (n >= buf_remaining) { /* complex case: buffer overflow */
        void *nbuf;

        md->buf_size += n*128; /* pad it to discourage reoffense */
        buf_remaining += n*128;
        nbuf = realloc (md->buf, md->buf_size); 

        if (nbuf == NULL) { /* ENOMEM */
            free (md->buf); /* realloc left it alone */
            md->buf = NULL; /* tag loser */
        } else { /* success */
            md->buf = nbuf;
            /* Try vsnprintf again into the new buffer. */
            va_start (vl, fmt);
            n = vsnprintf (md->buf + md->buf_used, buf_remaining, fmt, vl);
            va_end (vl);
            assert (n >= 0 && n < buf_remaining); /* should not fail */
            md->buf_used += n;
        }
    } else if (n < 0) { /* simple case: other vsprintf error */
        free (md->buf);
        md->buf = NULL; /* tag loser */
    } else { /* normal case: vsprintf success */
        md->buf_used += n;
    }

}


/* Print a string with JSON quoting.  Replace non-ASCII characters
   with \uFFFD "REPLACEMENT CHARACTER". */
static void mhdb_print_qstring(struct mhdb *md, const char *value)
{
    const char *c;

    mhdb_printf(md, "\"");
    for (c = value; *c; c++) {
        if (! isascii(*c))
            mhdb_printf(md, "\\uFFFD");
        else if (isalnum(*c))
            mhdb_printf(md, "%c", *c);
        else if (ispunct(*c) && !iscntrl(*c) && (*c != '\\' && *c != '\"'))
            mhdb_printf(md, "%c", *c);
        else if (*c == ' ')
            mhdb_printf(md, "%c", *c);
        else
            mhdb_printf(md, "\\u00%02x", *c);
    }
    mhdb_printf(md, "\"");
}


/* A convenience function to print a vanilla-ascii key and an
   unknown ancestry value as a JSON pair.  Add given suffix,
   which is likely to be a comma or a \n. */
static void mhdb_print_key_value(struct mhdb *md, const char *key, const char *value,
                          const char *suffix)
{
    mhdb_printf(md, "\"%s\":", key);
    mhdb_print_qstring(md, value);
    mhdb_printf(md, "%s", suffix);
}


#if 0 /* unused */
/* Ensure that any recent mhdb_printfs are canceled, and that the
   final mhdb_fini_response will fail. */
static void mhdb_unprintf(struct mhdb *md)
{
    if (md->buf) {
        free (md->buf);
        md->buf = NULL;
    }
}
#endif


/* Create a MHD_Response from the mhdb, now that we're done with it.
   If the buffer overflowed earlier (was unable to be extended), or if
   the MHD_create_response* function fails, return NULL.  Ensure that
   the buffer will be freed either by us here, or later by a
   successful MHD_create_response* function. */
static struct MHD_Response* mhdb_fini_response(struct mhdb *md)
{
    struct MHD_Response *r;

    if (md->buf == NULL) return NULL; /* reject loser */
    r = MHD_create_response_from_buffer (md->buf_used, md->buf, MHD_RESPMEM_MUST_FREE);
    if (r == NULL) {
        free (md->buf); /* we need to free it ourselves */
        /* fall through */
    }
    return r;
}




/* ------------------------------------------------------------------------ */

struct metric_list_traverse_closure {
    struct MHD_Connection *connection;
    struct webcontext *c;
    struct mhdb mhdb;
    unsigned num_metrics;
};



static void metric_list_traverse (const char* metric, void *closure)
{
    struct metric_list_traverse_closure *mltc = closure;
    pmID metric_id;
    pmDesc metric_desc;
    int rc;
    char *metric_text;
    char *metrics[1] = { (char*) metric };

    assert (mltc != NULL);

    rc = pmLookupName (1, metrics, & metric_id);
    if (rc != 1) {
        /* Quietly skip this metric. */
        return;
    }
    assert (metric_id != PM_ID_NULL);

    rc = pmLookupDesc (metric_id, & metric_desc);
    if (rc != 0) {
        /* Quietly skip this metric. */
        return;
    }
  
    if (mltc->num_metrics > 0)
        mhdb_printf (& mltc->mhdb, ",\n");
  
    mhdb_printf (& mltc->mhdb, "{ ");
    mhdb_print_key_value (& mltc->mhdb, "name", metric, ",");
    rc = pmLookupText (metric_id, PM_TEXT_ONELINE, & metric_text);
    if (rc == 0) {
        mhdb_print_key_value (& mltc->mhdb, "text-oneline", metric_text, ",");
        free (metric_text);
    }
    rc = pmLookupText (metric_id, PM_TEXT_HELP, & metric_text);
    if (rc == 0) {
        mhdb_print_key_value (& mltc->mhdb, "text-help", metric_text, ",");
        free (metric_text);
    }
    mhdb_printf (& mltc->mhdb, "\"pmid\":%lu, ", (unsigned long) metric_id);
    if (metric_desc.indom != PM_INDOM_NULL)
        mhdb_printf (& mltc->mhdb, "\"indom\":%lu, ", (unsigned long) metric_desc.indom);
    mhdb_print_key_value (& mltc->mhdb, "sem",
                          (metric_desc.sem == PM_SEM_COUNTER ? "counter" :
                           metric_desc.sem == PM_SEM_INSTANT ? "instant" :
                           metric_desc.sem == PM_SEM_DISCRETE ? "discrete" :
                           "unknown"),
                          ",");
    mhdb_print_key_value (& mltc->mhdb, "units", 
                          pmUnitsStr (& metric_desc.units), ",");
    mhdb_print_key_value (& mltc->mhdb, "type",
                          pmTypeStr (metric_desc.type), "");
    mhdb_printf (& mltc->mhdb, "}");
  
    mltc->num_metrics ++;
}


static int pmwebapi_respond_metric_list (struct MHD_Connection *connection,
                                         struct webcontext *c)
{
    struct metric_list_traverse_closure mltc;
    const char* val;
    struct MHD_Response* resp;
    int rc;

    mltc.connection = connection;
    mltc.c = c;
    mltc.num_metrics = 0;

    /* We need to construct a copy of the entire JSON metric metadata string,
       in one long malloc()'d buffer.  We size it generously to avoid having
       to realloc the bad boy and cause copies. */
    mhdb_init (& mltc.mhdb, 300000); /* 1000 pmns entries * 300 bytes each */
    mhdb_printf(& mltc.mhdb, "{ \"metrics\":[\n");

    val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "prefix");
    if (val == NULL) val = "";
    (void) pmTraversePMNS_r (val, & metric_list_traverse, & mltc); /* cannot fail */
    /* XXX: also handle pmids=... */
    /* XXX: also handle names=... */

    mhdb_printf(&mltc.mhdb, "] }");
    resp = mhdb_fini_response (& mltc.mhdb);
    if (resp == NULL) {
        pmweb_notify (LOG_ERR, connection, "mhdb_response failed\n");
        rc = -ENOMEM;
        goto out;
    }
    rc = MHD_add_response_header (resp, "Content-Type", "application/json");
    if (rc != MHD_YES) {
        pmweb_notify (LOG_ERR, connection, "MHD_add_response_header failed\n");
        rc = -ENOMEM;
        goto out1;
    }
    rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
    if (rc != MHD_YES) {
        pmweb_notify (LOG_ERR, connection, "MHD_queue_response failed\n");
        rc = -ENOMEM;
        goto out1;
    }

    MHD_destroy_response (resp);
    return MHD_YES;

 out1:
    MHD_destroy_response (resp);
 out:
    return pmwebapi_notify_error (connection, rc);
}


/* ------------------------------------------------------------------------ */

/* Print "value":<RENDERING>, possibly nested for PM_TYPE_EVENT.
   Upon failure to decode, print less and return non-0. */

static int pmwebapi_format_value (struct mhdb* output,
                                  pmDesc* desc,
                                  pmValueSet* pvs,
                                  int vsidx)
{
    pmValue* value = & pvs->vlist[vsidx];
    pmAtomValue a;
    int rc;

    /* unpack, as per pmevent.c:myeventdump */
    if (desc->type == PM_TYPE_EVENT) {
        pmResult **res;
        int numres, i;
        
        numres = pmUnpackEventRecords (pvs, vsidx, &res);
        if (numres < 0)
            return numres;

        mhdb_printf (output, "\"events\":[");
        for (i=0; i<numres; i++) {
            int j;
            if (i>0)            
                mhdb_printf (output, ",");
            mhdb_printf (output, "{ \"timestamp\": { \"s\":%lu, \"us\":%lu } ", 
                        res[i]->timestamp.tv_sec,
                        res[i]->timestamp.tv_usec);

            mhdb_printf (output, ", \"fields\":[");
            for (j=0; j<res[i]->numpmid; j++) {
                pmValueSet *fieldvsp = res[i]->vset[j];
                pmDesc fielddesc;
                char *fieldname;

                if (j>0)            
                    mhdb_printf (output, ",");

                mhdb_printf (output, "{");

                /* recurse */
                rc = pmLookupDesc (fieldvsp->pmid, &fielddesc);
                if (rc == 0)
                    rc = pmwebapi_format_value (output, &fielddesc, fieldvsp, 0);

                if (rc == 0) /* printer value: ... etc. ? */
                    mhdb_printf (output, ", ");
                /* XXX: handle value: for event.flags / event.missed */

                rc = pmNameID (fieldvsp->pmid, & fieldname);
                if (rc == 0) {
                    mhdb_printf (output, "\"name\":\"%s\"", fieldname);
                    free (fieldname);
                } else {
                    mhdb_printf (output, "\"name\":\"%s\"", pmIDStr (fieldvsp->pmid));
                }

                mhdb_printf (output, "}");
            }
            mhdb_printf (output, "]");

            mhdb_printf (output, "}\n");
        }
        mhdb_printf (output, "]");
        pmFreeEventResult(res);
        return 0;
    }

    rc = pmExtractValue (pvs->valfmt, value, desc->type, &a, desc->type);
    if (rc != 0)
        return rc;

    switch (desc->type) {
    case PM_TYPE_32:
        mhdb_printf (output, "\"value\":%i", a.l);
        break;
    case PM_TYPE_U32:
        mhdb_printf (output, "\"value\":%u", a.ul);
        break;
    case PM_TYPE_64:
        mhdb_printf (output, "\"value\":%" PRIi64, a.ll);
        break;
    case PM_TYPE_U64:
        mhdb_printf (output, "\"value\":%" PRIu64, a.ull);
        break;
    case PM_TYPE_FLOAT:
        mhdb_printf (output, "\"value\":%g", (double)a.f);
        break;
    case PM_TYPE_DOUBLE:
        mhdb_printf (output, "\"value\":%g", a.d);
        break;
    case PM_TYPE_STRING:
        mhdb_print_key_value (output, "value", a.cp, "");
        free (a.cp);
        break;
    case PM_TYPE_AGGREGATE:
    case PM_TYPE_AGGREGATE_STATIC:
        {
            /* base64-encode binary data */
            const char* p_bytes = & value->value.pval->vbuf[0]; /* from pmevent.c:mydump() */
            unsigned p_size = value->value.pval->vlen - PM_VAL_HDR_SIZE;
            unsigned i;
            const char base64_encoding_table[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

            if (value->value.pval->vlen < PM_VAL_HDR_SIZE) /* less than zero size? */
                return -EINVAL;
            
            mhdb_printf (output, "\"value\":\"");
            for (i=0; i<p_size; /* */) {
                unsigned char byte_0 = i < p_size ? p_bytes[i++] : 0;
                unsigned char byte_1 = i < p_size ? p_bytes[i++] : 0;
                unsigned char byte_2 = i < p_size ? p_bytes[i++] : 0;
                unsigned int triple = (byte_0 << 16) | (byte_1 << 8) | byte_2;
                mhdb_printf (output, "%c", base64_encoding_table[(triple >> 3*6) & 63]);
                mhdb_printf (output, "%c", base64_encoding_table[(triple >> 2*6) & 63]);
                mhdb_printf (output, "%c", base64_encoding_table[(triple >> 1*6) & 63]);
                mhdb_printf (output, "%c", base64_encoding_table[(triple >> 0*6) & 63]);
            }
            switch (p_size % 3) {
            case 0: /*mhdb_printf (output, "");*/ break; 
            case 1: mhdb_printf (output, "=="); break; 
            case 2: mhdb_printf (output, "="); break; 
            }
            mhdb_printf (output, "\"");

            if (desc->type != PM_TYPE_AGGREGATE_STATIC) /* XXX: correct? */
                free (a.vbp);
            break;
        }
    default:
        /* ... just a complete unknown ... */
        return -EINVAL;
    }

    return 0;
}



static int pmwebapi_respond_metric_fetch (struct MHD_Connection *connection,
                                          struct webcontext *c)
{
    const char* val_pmids;
    const char* val_names;
    struct MHD_Response* resp;
    int rc = 0;
    int max_num_metrics;
    int num_metrics;
    int printed_metrics; /* exclude skipped ones */
    pmID *metrics;
    struct mhdb output;
    pmResult *results;
    int i;

    (void) c;

    val_pmids = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "pmids");
    if (val_pmids == NULL) val_pmids = "";
    val_names = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "names");
    if (val_names == NULL) val_names = "";

    /* Pessimistically overestimate maximum number of pmID elements
       we'll need, to allocate the metrics[] array just once, and not have
       to range-check. */
    max_num_metrics = strlen(val_pmids)+strlen(val_names);
    /* The real minimum is actually closer to strlen()/2, to account
       for commas. */

    num_metrics = 0;
    metrics = calloc ((size_t) max_num_metrics, sizeof(pmID));
    if (metrics == NULL) {
        pmweb_notify (LOG_ERR, connection, "calloc pmIDs[%d] oom\n", max_num_metrics);
        rc = -ENOMEM;
        goto out;
    }

    /* Loop over names= names in val_names, collect them in metrics[]. */
    while (*val_names != '\0') {
        char *name;
        char *name_end = strchr (val_names, ',');
        char *names[1];
        pmID found_pmid;
        int num;

        /* Ignore plain "," XXX: elsewhere too? */
        if (*val_names == ',') {
            val_names ++;
            continue;
        }
      
        /* Copy just this name piece. */
        if (name_end) {
            name = strndup (val_names, (name_end - val_names));
            val_names = name_end + 1; /* skip past , */
        } else {
            name = strdup (val_names);
            val_names += strlen (val_names); /* skip onto \0 */
        }
        names[0] = name;
        num = pmLookupName (1, names, & found_pmid);
        free(name);

        if (num == 1) {
            assert (num_metrics < max_num_metrics);
            metrics[num_metrics++] = found_pmid;
        }      
    }


    /* Loop over pmids= numbers in val_pmids, append them to metrics[]. */
    while (*val_pmids) {
        char *numend; 
        unsigned long pmid = strtoul (val_pmids, & numend, 10); /* matches pmid printing above */
        if (numend == val_pmids) break; /* invalid contents */

        assert (num_metrics < max_num_metrics);
        metrics[num_metrics++] = pmid;

        if (*numend == '\0') break; /* end of string */
        if (*numend == ',')
            val_pmids = numend+1; /* advance to next string */
    }  

    /* Time to fetch the metric values. */
    /* num_metrics=0 ==> PM_ERR_TOOSMALL */
    rc = pmFetch (num_metrics, metrics, & results);
    free (metrics); /* don't need any more */
    if (rc < 0) {
        pmweb_notify (LOG_ERR, connection, "pmFetch failed\n");
        goto out;
    }
    /* NB: we don't care about the possibility of PMCD_*_AGENT bits
       being set, so rc > 0. */

    /* We need to construct a copy of the entire JSON metric value
       string, in one long malloc()'d buffer.  We size it generously to
       avoid having to realloc the bad boy and cause copies. */
    mhdb_init (& output, num_metrics * 200); /* WAG: per-metric size */
    mhdb_printf(& output, "{ \"timestamp\": { \"s\":%lu, \"us\":%lu },\n", 
                results->timestamp.tv_sec,
                results->timestamp.tv_usec);

    mhdb_printf(& output, "\"values\": [\n");

    assert (results->numpmid == num_metrics);
    printed_metrics = 0;
    for (i=0; i<results->numpmid; i++) {
        int j;
        pmValueSet *pvs = results->vset[i];
        char *metric_name;
        pmDesc desc;

        if (pvs->numval <= 0) continue; /* error code; skip metric */

        rc = pmLookupDesc (pvs->pmid, &desc); /* need to find desc.type only */
        if (rc < 0) continue; /* quietly skip it */

        if (printed_metrics >= 1)
            mhdb_printf (& output, ",\n");
        mhdb_printf (& output, "{ ");

        mhdb_printf (& output, "\"pmid\":%lu, ", (unsigned long) pvs->pmid);
        rc = pmNameID (pvs->pmid, &metric_name);
        if (rc == 0) {
            mhdb_print_key_value (& output, "name", metric_name, ",");
            free (metric_name);
        }
        mhdb_printf (& output, "\"instances\": [\n");
        for (j=0; j<pvs->numval; j++) {
            pmValue* val = & pvs->vlist[j];
            int printed_value;

            mhdb_printf (& output, "{");

            printed_value = ! pmwebapi_format_value (& output, & desc, pvs, j);

            if (desc.indom != PM_INDOM_NULL)
                mhdb_printf (& output, "%s \"instance\":%d", 
                             printed_value ? ", " : "", /* comma separation */
                             val->inst);

            mhdb_printf (& output, "}");
            if (j+1 < pvs->numval)
                mhdb_printf (& output, ","); /* comma separation */
        }
        mhdb_printf(& output, "] }"); /* iteration over instances */
        printed_metrics ++; /* comma separation at beginning of loop */
    }
    mhdb_printf(& output, "] }"); /* iteration over metrics */

    pmFreeResult (results); /* not needed any more */

    resp = mhdb_fini_response (& output);
    if (resp == NULL) {
        pmweb_notify (LOG_ERR, connection, "mhdb_response failed\n");
        rc = -ENOMEM;
        goto out;
    }
    rc = MHD_add_response_header (resp, "Content-Type", "application/json");
    if (rc != MHD_YES) {
        pmweb_notify (LOG_ERR, connection, "MHD_add_response_header failed\n");
        rc = -ENOMEM;
        goto out1;
    }
    rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
    if (rc != MHD_YES) {
        pmweb_notify (LOG_ERR, connection, "MHD_queue_response failed\n");
        rc = -ENOMEM;
        goto out1;
    }
    MHD_destroy_response (resp);
    return MHD_YES;

 out1:
    MHD_destroy_response (resp);
 out:
    return pmwebapi_notify_error (connection, rc);
}


/* ------------------------------------------------------------------------ */


static int pmwebapi_respond_instance_list (struct MHD_Connection *connection,
                                           struct webcontext *c)
{
    const char* val_indom;
    const char* val_name;
    const char* val_instance;
    const char* val_iname;
    struct MHD_Response* resp;
    int rc = 0;
    int max_num_instances;
    int num_instances;
    int printed_instances;
    int *instances;
    struct mhdb output;
    pmID metric_id;
    pmDesc metric_desc;
    pmInDom inDom;
    int i;

    (void) c;
    val_indom = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "indom");
    if (val_indom == NULL) val_indom = "";
    val_name = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "name");
    if (val_name == NULL) val_name = "";
    val_instance = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "instance");
    if (val_instance == NULL) val_instance = "";
    val_iname = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "iname");
    if (val_iname == NULL) val_iname = "";

    /* Obtain the instance domain. */
    if (0 == strcmp(val_indom, "")) {
        rc = pmLookupName (1, (char **)& val_name, & metric_id);
        if (rc != 1) {
            pmweb_notify (LOG_ERR, connection, "failed to lookup metric '%s'\n", val_name);
            goto out;
        }
        assert (metric_id != PM_ID_NULL);

        rc = pmLookupDesc (metric_id, & metric_desc);
        if (rc != 0) {
            pmweb_notify (LOG_ERR, connection, "failed to lookup metric '%s'\n", val_name);
            goto out;
        }

        inDom = metric_desc.indom;
    } else {
        char *numend;
        inDom = strtoul (val_indom, & numend, 10);
        if (numend == val_indom) {
            pmweb_notify (LOG_ERR, connection, "failed to parse indom '%s'\n", val_indom);
            rc = -EINVAL;
            goto out;
        }
    }

    /* Pessimistically overestimate maximum number of instance IDs needed. */
    max_num_instances = strlen(val_indom) + strlen(val_name);
    num_instances = 0;
    instances = calloc ((size_t) max_num_instances, sizeof(int));
    if (instances == NULL) {
        pmweb_notify (LOG_ERR, connection, "calloc instances[%d] oom\n", max_num_instances);
        rc = -ENOMEM;
        goto out;
    }

    /* In the case where neither val_instance nor val_iname are
       specified, pmGetInDom will allocate different arrays on our
       behalf, so we don't have to worry about accounting for how many
       instances are returned in that case. */

    /* Loop over instance= numbers in val_instance, collect them in instances[]. */
    while (*val_instance != '\0') {
        char *numend;
        int iid = (int) strtoul (val_instance, & numend, 10);
        if (numend == val_instance) break; /* invalid contents */
        assert (num_instances < max_num_instances);
        instances[num_instances++] = iid;
    
        if (*numend == '\0') break; /* end of string */
        if (*numend == ',')
            val_instance = numend+1; /* advance to next string */
    }

    /* Loop over iname= names in val_iname, collect them in instances[]. */
    while (*val_iname != '\0') {
        char *iname;
        char *iname_end = strchr (val_iname, ',');
        int iid;
    
        /* Ignore plain "," XXX: elsewhere too? */
        if (*val_iname == ',') {
            val_iname ++;
            continue;
        }

        if (iname_end) {
            iname = strndup (val_iname, (iname_end - val_iname));
            val_iname = iname_end + 1; /* skip past , */
        } else {
            iname = strdup (val_iname);
            val_iname += strlen (val_iname); /* skip onto \0 */
        }
    
        iid = pmLookupInDom(inDom, iname);

        if (iid > 0) {
            assert (num_instances < max_num_instances);
            instances[num_instances++] = iid;
        }
    }

    /* Time to fetch the instance info. */
    int *instlist;
    char **namelist = NULL;
    if (num_instances == 0) {
        free (instances);

        num_instances = pmGetInDom(inDom, & instlist, & namelist);
        if (num_instances < 1) {
            pmweb_notify (LOG_ERR, connection, "pmGetInDom failed\n");
            rc = num_instances;
            goto out;
        }
    } else {
        instlist = instances;
    }

    /* Build the response string all in one giant buffer: */
    mhdb_init (& output, num_instances * 200);
    mhdb_printf (& output, "{ \"indom\": %lu,\n", (unsigned long) inDom);
    mhdb_printf (& output, "\"instances\": [\n");

    printed_instances = 0;
    for (i=0; i<num_instances; i++) {
        char *instance_name;

        if (namelist != NULL) {
            instance_name = namelist[i];
        } else {
            rc = pmNameInDom (inDom, instlist[i], & instance_name);
            if (rc != 0) continue; /* skip this instance quietly */
        }

        if (printed_instances >= 1)
            mhdb_printf (& output, ",\n");
        mhdb_printf (& output, "{ \"instance\":%d,\n", instlist[i]);
        mhdb_print_key_value (& output, "name", instance_name, "\n");
        mhdb_printf (& output, "}");

        if (namelist == NULL) free (instance_name);

        printed_instances ++; /* comma separation at beginning of loop */
    }
    mhdb_printf(& output, "] }"); /* iteration over instances */

    /* Free no-longer-needed things: */
    free (instlist);
    if (namelist != NULL) free (namelist);

    resp = mhdb_fini_response (& output);
    if (resp == NULL) {
        pmweb_notify (LOG_ERR, connection, "mhdb_response failed\n");
        rc = -ENOMEM;
        goto out;
    }
    rc = MHD_add_response_header (resp, "Content-Type", "application/json");
    if (rc != MHD_YES) {
        pmweb_notify (LOG_ERR, connection, "MHD_add_response_header failed\n");
        rc = -ENOMEM;
        goto out1;
    }
    rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
    if (rc != MHD_YES) {
        pmweb_notify (LOG_ERR, connection, "MHD_queue_response failed\n");
        rc = -ENOMEM;
        goto out1;
    }
    MHD_destroy_response (resp);
    return MHD_YES;

 out1:
    MHD_destroy_response (resp);
 out:
    return pmwebapi_notify_error (connection, rc);
}


/* ------------------------------------------------------------------------ */


int pmwebapi_respond (void *cls, struct MHD_Connection *connection,
                      const char* url,
                      const char* method, const char* upload_data, size_t *upload_data_size)
{
    /* XXX: emit CORS header, e.g.
       https://developer.mozilla.org/en-US/docs/HTTP/Access_control_CORS */

    /* NB: url is already edited to remove the /pmapi/ prefix. */
    long webapi_ctx;
    __pmHashNode *chn;
    struct webcontext *c;
    char *context_command;
    int rc = 0;

    (void) cls;
    (void) upload_data;
    (void) upload_data_size;

    /* Decode the calls to the web API. */

    /* -------------------------------------------------------------------- */
    /* context creation */
    /* if-multithreaded: write-lock contexts */
    if (0 == strcmp (url, "context") &&
        new_contexts_p && /* permitted */
        (0 == strcmp (method, "POST") || 0 == strcmp (method, "GET")))
        return pmwebapi_respond_new_context (connection);

    /* -------------------------------------------------------------------- */
    /* All other calls use $CTX/command, so we parse $CTX
       generally and map it to the webcontext* */
    if (! (0 == strcmp (method, "POST") || 0 == strcmp (method, "GET"))) {
        pmweb_notify (LOG_WARNING, connection, "unknown method %s\n", method);
        rc = -EINVAL;
        goto out;
    }
    errno = 0;
    webapi_ctx = strtol (url, & context_command, 10); /* matches %d above */
    if (errno != 0
        || webapi_ctx <= 0 /* range check, plus string-nonemptyness check */
        || webapi_ctx > INT_MAX /* matches random() loop above */
        || *context_command != '/') { /* parsed up to the next slash */
        pmweb_notify (LOG_WARNING, connection, "unrecognized %s url %s \n", method, url);
        rc = -EINVAL;
        goto out;
    }
    context_command ++; /* skip the / */

    /* if-multithreaded: read-lock contexts */
    chn = __pmHashSearch ((int)webapi_ctx, & contexts);
    if (chn == NULL) {
        pmweb_notify (LOG_WARNING, connection, "unknown web context #%ld\n", webapi_ctx);
        rc = PM_ERR_NOCONTEXT;
        goto out;
    }
    c = (struct webcontext *) chn->data;
    assert (c != NULL);

    /* Process HTTP Basic userid/password, if supplied.  Both returned strings
       need to be free(3)'d later.  */
    if (c->userid != NULL) { /* Did this context requires userid/password auth? */
        char *userid = NULL;
        char *password = NULL;
        userid = MHD_basic_auth_get_username_password (connection, &password);

        /* 401 */
        if (userid == NULL || password == NULL) {
            static char auth_req_msg[] = "authentication required";
            struct MHD_Response *resp;
            char auth_realm[40];

            free (userid);
            free (password);
            resp = MHD_create_response_from_buffer (strlen(auth_req_msg),
                                                    auth_req_msg,
                                                    MHD_RESPMEM_PERSISTENT);
            if (! resp) {
                rc = -ENOMEM;
                goto out;
            }

            /* We need the user to resubmit this with http
               authentication info, with a custom HTTP authentication
               realm for this context. */
            snprintf (auth_realm, sizeof(auth_realm),
                      "%s/%ld", uriprefix, webapi_ctx);
            rc = MHD_queue_basic_auth_fail_response (connection, auth_realm, resp);
            MHD_destroy_response (resp);
            if (rc != MHD_YES) {
                rc = -ENOMEM;
                goto out;
            }
            return MHD_YES;
        }
        /* 403 */
        if (strcmp (userid, c->userid) ||
            (c->password && strcmp (password, c->password))) {
            static char auth_failed_msg[] = "authentication failed";
            struct MHD_Response *resp;

            free (userid);
            free (password);
            resp = MHD_create_response_from_buffer (strlen(auth_failed_msg),
                                                    auth_failed_msg,
                                                    MHD_RESPMEM_PERSISTENT);
            if (! resp) {
                rc = -ENOMEM;
                goto out;
            }
            /* We need the user to resubmit this with http authentication info. */
            rc = MHD_queue_response (connection, MHD_HTTP_FORBIDDEN, resp);
            MHD_destroy_response (resp);
            if (rc != MHD_YES) {
                rc = -ENOMEM;
                goto out;
            }
            return MHD_YES;
        }
        /* FALLTHROUGH: authentication required & succeeded. */
        free (userid);
        free (password);
    }

    /* Update last-use of this connection. */
    if (c->expires != 0) {
        time (& c->expires);
        c->expires += c->mypolltimeout;
    }

    /* Switch to this context for subsequent operations. */
    /* if-multithreaded: watch out. */
    rc = pmUseContext (c->context);
    if (rc)
        goto out;

    /* -------------------------------------------------------------------- */
    /* metric enumeration: /context/$ID/_metric */
    if (0 == strcmp (context_command, "_metric") &&
        (0 == strcmp (method, "POST") || 0 == strcmp (method, "GET")))
        return pmwebapi_respond_metric_list (connection, c);

    /* -------------------------------------------------------------------- */
    /* metric instance metadata: /context/$ID/_indom */
    if (0 == strcmp (context_command, "_indom") &&
        (0 == strcmp (method, "POST") || 0 == strcmp (method, "GET")))
        return pmwebapi_respond_instance_list (connection, c);
    
    /* -------------------------------------------------------------------- */
    /* metric fetch: /context/$ID/_fetch */
    if (0 == strcmp (context_command, "_fetch") &&
        (0 == strcmp (method, "POST") || 0 == strcmp (method, "GET")))
        return pmwebapi_respond_metric_fetch (connection, c);

    pmweb_notify (LOG_WARNING, connection, "unrecognized %s context command %s \n", method, context_command);

 out:
    return pmwebapi_notify_error (connection, rc);
}
