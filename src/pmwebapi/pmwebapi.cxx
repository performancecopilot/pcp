/*
 * JSON web bridge for PMAPI.
 *
 * Copyright (c) 2011-2017 Red Hat.
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

/*
 * Note on "name" parameter following changes to allow duplicate names
 * in the PMNS.  Refer to the two pmNameID() calls in the code below.
 *
 * The "name" parameter should really return a set of names, not just
 * "one of the names" for the associated metric.
 *
 * On 04/02/15 02:23, Frank Ch. Eigler wrote:
 *
 * A possible step would be to pass back all names for pmid-metadata, one
 * could add a JSON vector subfield like
 *      "names": ["foo.bar", "foo.bar2"]
 * to the /pmapi/NNNNN/_metric query.
 *
 * A likely necessary step would be to tweak the /_fetch code, so that
 * the result "name":"NAME*" fields match up with the /_fetch?name=NAME
 * requests.  This would mean teaching pmwebapi_respond_metric_fetch() to
 * store not just resolved "pmID *metrics;" but a vector of pre-resolved
 * "metric-names".
 * 
 * - Ken 4 Feb 2015
 */

#include "pmwebapi.h"

#include <map>
#include <iostream>
#include <sstream>
#include <algorithm>


using namespace std;

/* ------------------------------------------------------------------------ */

struct webcontext {
    /* __pmHashNode key is     unique randomized web context key, [1,INT_MAX] */
    /* XXX: optionally bind session to a particular IP address? */
    string userid;		/* "" or username */
    string password;		/* "" or password */
    unsigned mypolltimeout;
    time_t expires;		/* poll timeout, 0 if never expires */
    int context;			/* PMAPI context handle; owned */
    string spec;

    map <string, pmID> metric_id_cache;
    map <pmID, string> metric_name_cache;
    map <pmID, pmDesc> metric_desc_cache;
    map <pmID, string> metric_text_cache;
    map <pmID, map<int, string> > metric_inst_cache;
    ~webcontext ();
};


typedef map <unsigned long, webcontext *>context_map;
static context_map contexts;	// map from webcontext#




/* Check whether any contexts have been unpolled so long that they
   should be considered abandoned.  If so, close 'em, free 'em, yak
   'em, smack 'em.  Return the number of seconds to the next good time
   to check for garbage. */
unsigned
pmwebapi_gc ()
{
    time_t now;
    (void) time (&now);
    time_t soonest = 0;

    for (context_map::iterator it = contexts.begin (); it != contexts.end (); /* null */) {

        if (it->second->expires == 0) {
            // permanent
            it++;
            continue;
        }

        if (it->second->expires < now) {
            if (verbosity) {
                timestamp (clog) << "context (web" << it->first << "=pm" << it->second->context <<
                                 ") expired." << endl;
            }

            delete it->second;
            context_map::iterator it2 = it++;
            contexts.erase (it2);
        } else {
            if (soonest == 0) {
                // first
                soonest = it->second->expires;
            } else if (soonest > it->second->expires) {
                // not earliest
                soonest = it->second->expires;
            }
            it++;
        }
    }

    return soonest ? (unsigned) (soonest - now) : maxtimeout;
}


webcontext::~webcontext ()
{
    if (this->context >= 0) {
        int sts = pmDestroyContext (this->context);
        if (sts) {
            timestamp (cerr) << "pmDestroyContext pm" << this->context << "failed, rc=" << sts <<
                             endl;
        }
    }
}


void
pmwebapi_deallocate_all ()
{
    for (context_map::iterator it = contexts.begin (); it != contexts.end (); it++) {
        if (verbosity) {
            timestamp (clog) << "context pm" << it->second->context << "deleted." << endl;
        }
        delete it->second;
    }

}


/* Allocate an zeroed webcontext structure, and enroll it in the
   hash table with the given context#. */
static int
webcontext_allocate (int webapi_ctx, struct webcontext **wc)
{
    if (contexts.find (webapi_ctx) != contexts.end ()) {
        return -EEXIST;
    }
    /* Allocate & clear our webapi context. */
    assert (wc);
    contexts[webapi_ctx] = *wc = new webcontext ();
    return 0;
}


int
pmwebapi_bind_permanent (int webapi_ctx, int pcp_context, string spec)
{
    struct webcontext *c;
    int rc = webcontext_allocate (webapi_ctx, &c);
    if (rc < 0) {
        return rc;
    }
    assert (c);
    assert (pcp_context >= 0);
    c->context = pcp_context;
    c->mypolltimeout = ~0;
    c->expires = 0;
    c->spec = spec;
    return 0;
}


static int
pmwebapi_respond_new_context (struct MHD_Connection *connection,
                              const http_params & params)
{
    /* Create a context. */
    int rc = 0;
    int context = -EINVAL;
    char http_response[30];
    string context_description;
    unsigned polltimeout;
    struct MHD_Response *resp;
    int webapi_ctx;
    int iterations = 0;
    string userid;
    string password;

    string val = params["hostspec"];
    if (val == "") {
        /* backward compatibility alias */
        val = params["hostname"];
    }
    if (val != "") {
        pmHostSpec *hostSpec;
        int hostSpecCount;
        __pmHashCtl hostAttrs;
        char *hostAttrsError;
        __pmHashInit (&hostAttrs);

        if ((strstr(val.c_str (), "unix:") != NULL ||
            strstr(val.c_str (), "local:") != NULL) &&
            !permissive) {
            connstamp (cerr, connection) << "local mode requested, denied" << endl;
            rc = -EPERM;
            goto out;
        }

        rc = __pmParseHostAttrsSpec (val.c_str (), &hostSpec, &hostSpecCount, &hostAttrs,
                                     &hostAttrsError);
        if (rc == 0) {
            __pmHashNode *node;
            node = __pmHashSearch (PCP_ATTR_USERNAME, &hostAttrs);	/* XXX: PCP_ATTR_AUTHNAME? */
            if (node) {
                userid = string ((char *) node->data);
            }
            node = __pmHashSearch (PCP_ATTR_PASSWORD, &hostAttrs);
            if (node) {
                password = string ((char *) node->data);
            }
            __pmFreeHostAttrsSpec (hostSpec, hostSpecCount, &hostAttrs);
            __pmHashClear (&hostAttrs);
        } else {
            /* Ignore the parse error at this stage; pmNewContext will give it to us. */
            free (hostAttrsError);
        }

        if (__pmServerHasFeature(PM_SERVER_FEATURE_CREDS_REQD) &&
	    (userid == "" || password == "")) {
            static char creds_reqd_msg[] = "credentials required";
            struct MHD_Response *resp;

            resp = MHD_create_response_from_buffer (strlen (creds_reqd_msg), creds_reqd_msg,
                                                    MHD_RESPMEM_PERSISTENT);
            if (!resp) {
                rc = -ENOMEM;
                goto out;
            }
            /* We need the user to resubmit this with authentication info. */
            rc = MHD_queue_response (connection, MHD_HTTP_FORBIDDEN, resp);
            MHD_destroy_response (resp);
            if (rc != MHD_YES) {
                rc = -ENOMEM;
                goto out;
            }
            return MHD_YES;
        }

        context = pmNewContext (PM_CONTEXT_HOST, val.c_str ());	/* XXX: limit access */
        context_description = string ("PM_CONTEXT_HOST ") + val;
    } else {
        string archivefile = params["archivefile"];
        if (archivefile != "") {
            if (!__pmAbsolutePath ((char *) archivefile.c_str ())) {
                archivefile = archivesdir + (char) __pmPathSeparator () + archivefile;
            }

            if (cursed_path_p (archivesdir, archivefile)) {
                connstamp (cerr, connection) << "suspicious archive path " << archivefile << endl;
                rc = -EINVAL;
                goto out;
            }

            context = pmNewContext (PM_CONTEXT_ARCHIVE, archivefile.c_str ());
            context_description = string ("PM_CONTEXT_ARCHIVE ") + archivefile;
        } else if (MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "local")) {
            if (!permissive) {
                connstamp (cerr, connection) << "local context requested, denied" << endl;
                rc = -EPERM;
                goto out;
            }
            /* Note we need to use a dummy parameter to local=FOO,
               since the MHD_lookup* API does not differentiate
               between an absent argument vs. an argument given
               without a parameter value. */
            context = pmNewContext (PM_CONTEXT_LOCAL, NULL);
            context_description = string ("PM_CONTEXT_LOCAL");
        } else {
            /* context remains -something */
        }
    }

    if (context < 0) {
        char pmmsg[PM_MAXERRMSGLEN];
        connstamp (cerr, connection) << "new context failed: "
                                     << pmErrStr_r (context, pmmsg, sizeof (pmmsg)) << endl;
        rc = context;
        goto out;
    }

    /* Process optional ?polltimeout=SECONDS field.  If broken/missing, assume maxtimeout. */
    val = params["polltimeout"];
    if (val != "") {
        long pt;
        char *endptr;
        errno = 0;
        pt = strtol (val.c_str (), &endptr, 0);
        if (errno != 0 || *endptr != '\0' || pt <= 0 || pt > (long) maxtimeout) {
            polltimeout = maxtimeout;
        } else {
            polltimeout = (unsigned) pt;
        }
    } else {
        polltimeout = maxtimeout;
    }


    {
        struct webcontext *c = NULL;
        /* Create a new context key for the webapi.  We just use a random integer within
           a reasonable range: 1..INT_MAX */
        while (1) {
            /* Preclude infinite looping here, for example due to a badly behaving
               random(3) implementation. */
            iterations++;
            if (iterations > 100) {
                connstamp (cerr, connection) << "webapi_ctx allocation failed" << endl;
                pmDestroyContext (context);
                rc = -EMFILE;
                goto out;
            }

            webapi_ctx = random ();	/* we hope RAND_MAX is large enough */
            if (webapi_ctx <= 0) {
                continue;
            }
            rc = webcontext_allocate (webapi_ctx, &c);
            if (rc == 0) {
                break;
            }

            /* This may already exist.  We loop in case the key id already exists. */
        }

        assert (c);
        c->context = context;
        time (&c->expires);
        c->mypolltimeout = polltimeout;
        c->expires += c->mypolltimeout;
        c->userid = userid;		/* may be empty */
        c->password = password;	/* ditto */
        /* Errors beyond this point don't require instant cleanup; the
           periodic context GC will do it all. */
    }

    if (verbosity) {
        char hostname[MAXHOSTNAMELEN];
        char *context_hostname = pmGetContextHostName_r (context, hostname, sizeof(hostname));

        timestamp (clog) << "context " << context_description << (context_hostname ? " (" : "")
                         << (context_hostname ? context_hostname : "") << (context_hostname ? ")" : "") << " (web"
                         << webapi_ctx << "=pm" << context << ") " << (userid != "" ? " (user=" : "") <<
                         (userid != "" ? userid : "") << (userid != "" ? ")" : "") << "created, expires after " <<
                         polltimeout << "s" << endl;
    }

    rc = pmsprintf (http_response, sizeof (http_response), "{ \"context\": %d }", webapi_ctx);
    assert (rc >= 0 && rc < (int) sizeof (http_response));
    resp = MHD_create_response_from_buffer (strlen (http_response), http_response,
                                            MHD_RESPMEM_MUST_COPY);
    if (resp == NULL) {
        connstamp (cerr, connection) << "MHD_create_response_from_buffer failed" << endl;
        rc = -ENOMEM;
        goto out;
    }

    rc = MHD_add_response_header (resp, "Content-Type", "application/json");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header CT failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    rc = MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header ACAO failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
    MHD_destroy_response (resp);
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_queue_response failed" << endl;
        rc = -ENOMEM;
        goto out;
    }

    return MHD_YES;

out1:
    MHD_destroy_response (resp);
out:
    return mhd_notify_error (connection, rc);
}


/* ------------------------------------------------------------------------ */


struct metric_list_traverse_closure {
    struct MHD_Connection *connection;
    struct webcontext *c;
    ostringstream *mhdb;
    unsigned num_metrics;
};


static void
metric_list_traverse (const char *metric, void *closure)
{
    struct metric_list_traverse_closure *mltc = (struct metric_list_traverse_closure *)
            closure;
    pmID metric_id;
    pmDesc metric_desc;
    int rc;
    char buffer[64];	/* sufficient for any type/units strings */
    char *metric_text;
    char *metrics[1] = {
        (char *) metric
    };
    assert (mltc != NULL);
    rc = pmLookupName (1, metrics, &metric_id);
    if (rc != 1) {
        /* Quietly skip this metric. */
        return;
    }
    assert (metric_id != PM_ID_NULL);
    rc = pmLookupDesc (metric_id, &metric_desc);
    if (rc != 0) {
        /* Quietly skip this metric. */
        return;
    }

    if (mltc->num_metrics > 0) {
        *mltc->mhdb << ",\n";
    }

    *mltc->mhdb << "{";

    json_key_value (*mltc->mhdb, "name", string (metric), ",");
    rc = pmLookupText (metric_id, PM_TEXT_ONELINE, &metric_text);
    if (rc == 0) {
        json_key_value (*mltc->mhdb, "text-oneline", string (metric_text), ",");
        free (metric_text);
    }
    rc = pmLookupText (metric_id, PM_TEXT_HELP, &metric_text);
    if (rc == 0) {
        json_key_value (*mltc->mhdb, "text-help", string (metric_text), ",");
        free (metric_text);
    }
    json_key_value (*mltc->mhdb, "pmid", (unsigned long) metric_id, ",");
    if (metric_desc.indom != PM_INDOM_NULL) {
        json_key_value (*mltc->mhdb, "indom", (unsigned long) metric_desc.indom, ",");
    }
    json_key_value (*mltc->mhdb, "sem",
                    string (metric_desc.sem == PM_SEM_COUNTER ? "counter" : metric_desc.sem == PM_SEM_INSTANT
                            ? "instant" : metric_desc.sem == PM_SEM_DISCRETE ? "discrete" : "unknown"), ",");
    json_key_value (*mltc->mhdb, "units", string (pmUnitsStr_r (&metric_desc.units, buffer, sizeof (buffer))), ",");
    json_key_value (*mltc->mhdb, "type", string (pmTypeStr_r (metric_desc.type, buffer, sizeof (buffer))), "");

    *mltc->mhdb << "}";
    mltc->num_metrics++;
}


static int
pmwebapi_respond_metric_list (struct MHD_Connection *connection,
                              const http_params & params, struct webcontext *c)
{
    struct metric_list_traverse_closure mltc;
    string val;
    struct MHD_Response *resp;
    int rc;
    mltc.connection = connection;
    mltc.c = c;
    mltc.num_metrics = 0;
    mltc.mhdb = new ostringstream ();

    *mltc.mhdb << "{ \"metrics\":[\n";

    val = params["prefix"];
    rc = pmTraversePMNS_r (val.c_str (), &metric_list_traverse, &mltc);	/* cannot fail */
    if (rc == PM_ERR_IPC) {
	pmDestroyContext (c->context);
	mltc.c->context = c->context = -1;
    }
    /* XXX: also handle pmids=... */
    /* XXX: also handle names=... */

    *mltc.mhdb << "]}";

    string s = mltc.mhdb->str ();
    delete mltc.mhdb;
    mltc.mhdb = 0;
    resp = NOTMHD_compressible_response (connection, s);
    if (resp == NULL) {
        connstamp (cerr, connection) << "MHD_create_response_from_buffer failed" << endl;
        rc = -ENOMEM;
        goto out;
    }

    rc = MHD_add_response_header (resp, "Content-Type", "application/json");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    rc = MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header ACAO failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_queue_response failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }
    MHD_destroy_response (resp);
    return MHD_YES;
out1:
    MHD_destroy_response (resp);
out:
    return mhd_notify_error (connection, rc);
}


/* ------------------------------------------------------------------------ */

/* Print "value":<RENDERING>, possibly nested for PM_TYPE_EVENT.
   Upon failure to decode, print less and return non-0. */

static int
pmwebapi_format_value (ostream & output, pmDesc * desc, pmValueSet * pvs, int vsidx)
{
    pmValue *value = &pvs->vlist[vsidx];
    pmAtomValue a;
    int rc;
    /* unpack, as per pmevent.c:myeventdump */
    if (desc->type == PM_TYPE_EVENT) {
        pmResult **res;
        int numres, i;
        numres = pmUnpackEventRecords (pvs, vsidx, &res);
        if (numres < 0) {
            return numres;
        }

        output << "\"events\":[";
        for (i = 0; i < numres; i++) {
            int j;
            if (i > 0) {
                output << ",";
            }

            output << "{" << "\"timestamp\":{";
            json_key_value (output, "s", res[i]->timestamp.tv_sec, ",");
            json_key_value (output, "us", res[i]->timestamp.tv_usec);
            output << "}" << ", \"fields\":[\n";

            for (j = 0; j < res[i]->numpmid; j++) {
                pmValueSet *fieldvsp = res[i]->vset[j];
                pmDesc fielddesc;
                char *fieldname;
                if (j > 0) {
                    output << ",";
                }

                output << "\n\t{";

                /* recurse */
                rc = pmLookupDesc (fieldvsp->pmid, &fielddesc);
                if (rc == 0) {
                    rc = pmwebapi_format_value (output, &fielddesc, fieldvsp, 0);
                }
                if (rc == 0) {
                    /* printer value: ... etc. ? */
                    output << ",";
                }
                /* XXX: handle value: for event.flags / event.missed */
                rc = pmNameID (fieldvsp->pmid, &fieldname);
                if (rc == 0) {
                    json_key_value (output, "name", string(fieldname));
                    free (fieldname);
                } else {
                    char pmidstr[20];
                    json_key_value (output, "name", string(pmIDStr_r (fieldvsp->pmid, pmidstr, sizeof(pmidstr))));
                }

                output << "}\n";
            }
            output << "]}\n"; // fields
        }
        output << "]"; // events

        pmFreeEventResult (res);
        return 0;
    }

    rc = pmExtractValue (pvs->valfmt, value, desc->type, &a, desc->type);
    if (rc != 0) {
        return rc;
    }
    switch (desc->type) {
    case PM_TYPE_32:
        json_key_value (output, "value", a.l);
        break;
    case PM_TYPE_U32:
        json_key_value (output, "value", a.ul);
        break;
    case PM_TYPE_64:
        json_key_value (output, "value", a.ll);
        break;
    case PM_TYPE_U64:
        json_key_value (output, "value", a.ull);
        break;
    case PM_TYPE_FLOAT:
        json_key_value (output, "value", a.f);
        break;
    case PM_TYPE_DOUBLE:
        json_key_value (output, "value", a.d);
        break;
    case PM_TYPE_STRING:
        json_key_value (output, "value", string(a.cp));
        free (a.cp);		// NB: required by pmapi
        break;
    case PM_TYPE_AGGREGATE:
    case PM_TYPE_AGGREGATE_STATIC: {
        /* base64-encode binary data */
        const char *p_bytes = &value->value.pval->vbuf[0];	/* from pmevent.c:mydump() */
        unsigned p_size = value->value.pval->vlen - PM_VAL_HDR_SIZE;
        unsigned i;
        const char base64_encoding_table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        if (value->value.pval->vlen < PM_VAL_HDR_SIZE) {
            /* less than zero size? */
            return -EINVAL;
        }
        output << "\"value\":\"";
        for (i = 0; i < p_size; /* */) {
            unsigned char byte_0 = i < p_size ? p_bytes[i++] : 0;
            unsigned char byte_1 = i < p_size ? p_bytes[i++] : 0;
            unsigned char byte_2 = i < p_size ? p_bytes[i++] : 0;
            unsigned int triple = (byte_0 << 16) | (byte_1 << 8) | byte_2;
            output << base64_encoding_table[ (triple >> 3 * 6) & 63];
            output << base64_encoding_table[ (triple >> 2 * 6) & 63];
            output << base64_encoding_table[ (triple >> 1 * 6) & 63];
            output << base64_encoding_table[ (triple >> 0 * 6) & 63];
        }
        switch (p_size % 3) {
        case 0:		/*mhdb_printf (output, ""); */
            break;
        case 1:
            output << "==";
            break;
        case 2:
            output << "=";
            break;
        }
        output << "\"";
        if (desc->type != PM_TYPE_AGGREGATE_STATIC) {
            /* XXX: correct? */
            free (a.vbp);	// NB: required by pmapi
        }
        break;
    }
    default:
        /* ... just a complete unknown ... */
        return -EINVAL;
    }

    return 0;
}



static int
pmwebapi_respond_metric_fetch (struct MHD_Connection *connection,
                               const http_params & /*params*/, struct webcontext *c)
{
    const char *val_pmids;
    const char *val_names;
    struct MHD_Response *resp;
    int rc = 0;
    int max_num_metrics;
    int num_metrics;
    ostringstream output;
    int printed_metrics;		/* exclude skipped ones */
    pmID *metrics;
    pmResult *results;
    int i;

    (void) c;
    val_pmids = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "pmids");
    if (val_pmids == NULL) {
        val_pmids = "";
    }
    val_names = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "names");
    if (val_names == NULL) {
        val_names = "";
    }
    /* Pessimistically overestimate maximum number of pmID elements
       we'll need, to allocate the metrics[] array just once, and not have
       to range-check. */
    max_num_metrics = strlen (val_pmids) + strlen (val_names);
    /* The real minimum is actually closer to strlen()/2, to account
       for commas. */
    num_metrics = 0;
    metrics = (pmID *) calloc ((size_t) max_num_metrics, sizeof (pmID));
    if (metrics == NULL) {
        connstamp (cerr, connection) << "calloc pmIDs[" << max_num_metrics << "] failed" << endl;
        rc = -ENOMEM;
        goto out;
    }

    /* Loop over names= names in val_names, collect them in metrics[]. */
    while (*val_names != '\0') {
        char *name;
        const char *name_end = strchr (val_names, ',');
        char *names[1];
        pmID found_pmid;
        int num;
        /* Ignore plain "," XXX: elsewhere too? */
        if (*val_names == ',') {
            val_names++;
            continue;
        }
        // XXX: c++ify
        /* Copy just this name piece. */
        if (name_end) {
            name = strndup (val_names, (name_end - val_names));
            val_names = name_end + 1;	/* skip past , */
        } else {
            name = strdup (val_names);
            val_names += strlen (val_names);	/* skip onto \0 */
        }
        names[0] = name;
	num = pmLookupName (1, names, &found_pmid);
	if (rc == PM_ERR_IPC) {
	    pmDestroyContext (c->context);
	    c->context = -1;
	}
        free (name);
        if (num == 1) {
            assert (num_metrics < max_num_metrics);
            metrics[num_metrics++] = found_pmid;
        }
    }

    /* Loop over pmids= numbers in val_pmids, append them to metrics[]. */
    while (*val_pmids) {
        char *numend;
        unsigned long pmid = strtoul (val_pmids, &numend, 0); // accept hex too
        if (numend == val_pmids) {
            break;		/* invalid contents */
        }
        assert (num_metrics < max_num_metrics);
        metrics[num_metrics++] = pmid;
        if (*numend == '\0') {
            break;		/* end of string */
        }
        val_pmids = numend+1; // advance to next string
    }

    /* Time to fetch the metric values. */
    /* num_metrics=0 ==> PM_ERR_TOOSMALL */
    rc = pmFetch (num_metrics, metrics, &results);
    free (metrics);		/* don't need any more */
    if (rc < 0) {
        char pmmsg[PM_MAXERRMSGLEN];
        connstamp (cerr, connection) << "pmFetch failed: " << pmErrStr_r (rc, pmmsg, sizeof (pmmsg)) << endl;
        goto out;
    }
    /* NB: we don't care about the possibility of PMCD_*_AGENT bits
       being set, so rc > 0. */

    output << "{" << "\"timestamp\":{";
    json_key_value (output, "s", results->timestamp.tv_sec, ",");
    json_key_value (output, "us", results->timestamp.tv_usec);
    output << "}" << ", \"values\":[";

    assert (results->numpmid == num_metrics);
    printed_metrics = 0;
    for (i = 0; i < results->numpmid; i++) {
        int j;
        pmValueSet *pvs = results->vset[i];
        char *metric_name;
        pmDesc desc;
        if (pvs->numval <= 0) {
            continue;		/* error code; skip metric */
        }
        rc = pmLookupDesc (pvs->pmid, &desc);	/* need to find desc.type only */
        if (rc < 0) {
            continue;		/* quietly skip it */
        }
        if (printed_metrics >= 1) {
            output << ",\n";
        }


        output << "{";
        json_key_value (output, "pmid", pvs->pmid, ",");

        rc = pmNameID (pvs->pmid, &metric_name);
        if (rc == 0) {
            json_key_value (output, "name", string (metric_name), ",");
            free (metric_name);
        }
        output << "\"instances\":[\n";
        for (j = 0; j < pvs->numval; j++) {
            pmValue *val = &pvs->vlist[j];
            output << "{";
            json_key_value (output, "instance", val->inst, ", ");
            pmwebapi_format_value (output, &desc, pvs, j);
            output << "}";
            if (j + 1 < pvs->numval) {
                output << ",";
            }
        }
        output << "]}";		// iteration over instances
        printed_metrics++;	/* comma separation at beginning of loop */
    }
    output << "]}";		// iteration over metrics
    pmFreeResult (results);	/* not needed any more */

    {
        string s = output.str ();
        resp = NOTMHD_compressible_response (connection, s);
    }
    if (resp == NULL) {
        connstamp (cerr, connection) << "MHDB_create_response_from_buffer failed" << endl;
        rc = -ENOMEM;
        goto out;
    }

    rc = MHD_add_response_header (resp, "Content-Type", "application/json");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    rc = MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header ACAO failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_queue_response failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }
    MHD_destroy_response (resp);
    return MHD_YES;
out1:
    MHD_destroy_response (resp);
out:
    return mhd_notify_error (connection, rc);
}


/* ------------------------------------------------------------------------ */


static int
pmwebapi_respond_instance_list (struct MHD_Connection *connection,
                                const http_params & /*params*/,
                                struct webcontext *c)
{
    const char *val_indom;
    const char *val_name;
    const char *val_instance;
    const char *val_iname;
    struct MHD_Response *resp;
    int rc = 0;
    int max_num_instances;
    int num_instances;
    int printed_instances;
    int *instances;
    pmID metric_id;
    pmDesc metric_desc;
    pmInDom inDom;
    int i;
    int *instlist;
    char **namelist = NULL;
    ostringstream output;

    (void) c;
    val_indom = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "indom");
    if (val_indom == NULL) {
        val_indom = "";
    }
    val_name = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "name");
    if (val_name == NULL) {
        val_name = "";
    }
    val_instance = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND,
                   "instance");
    if (val_instance == NULL) {
        val_instance = "";
    }
    val_iname = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "iname");
    if (val_iname == NULL) {
        val_iname = "";
    }
    /* Obtain the instance domain. */
    if (0 == strcmp (val_indom, "")) {
	rc = pmLookupName (1, (char **) &val_name, &metric_id);
	if (rc == PM_ERR_IPC) {
	    pmDestroyContext (c->context);
	    c->context = -1;
	}
        if (rc != 1) {
            connstamp (cerr, connection) << "failed to lookup metric " << val_name << endl;
            goto out;
        }
        assert (metric_id != PM_ID_NULL);
        rc = pmLookupDesc (metric_id, &metric_desc);
        if (rc != 0) {
            connstamp (cerr, connection) << "failed to lookup desc " << val_name << endl;
            goto out;
        }

        inDom = metric_desc.indom;
    } else {
        char *numend;
        inDom = strtoul (val_indom, &numend, 0);
        if (numend == val_indom) {
            connstamp (cerr, connection) << "failed to parse indom " << val_indom << endl;
            rc = -EINVAL;
            goto out;
        }
    }

    /* Pessimistically overestimate maximum number of instance IDs needed. */
    max_num_instances = strlen (val_instance) + strlen (val_iname);
    num_instances = 0;
    instances = (int *) calloc ((size_t) max_num_instances, sizeof (int));
    if (instances == NULL) {
        connstamp (cerr, connection) << "calloc instances[" << max_num_instances << "] oom" <<
                                     endl;
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
        int iid = (int) strtoul (val_instance, &numend, 0);
        if (numend == val_instance) {
            break;		/* invalid contents */
        }
        assert (num_instances < max_num_instances);
        instances[num_instances++] = iid;
        if (*numend == '\0') {
            break;		/* end of string */
        }
        val_instance = numend + 1;	/* advance to next string */
    }

    /* Loop over iname= names in val_iname, collect them in instances[]. */
    while (*val_iname != '\0') {
        char *iname;
        const char *iname_end = strchr (val_iname, ',');
        int iid;
        /* Ignore plain "," XXX: elsewhere too? */
        if (*val_iname == ',') {
            val_iname++;
            continue;
        }

        if (iname_end) {
            iname = strndup (val_iname, (iname_end - val_iname));
            val_iname = iname_end + 1;	/* skip past , */
        } else {
            iname = strdup (val_iname);
            val_iname += strlen (val_iname);	/* skip onto \0 */
        }

        iid = pmLookupInDom (inDom, iname);
        if (iid >= 0) {
            assert (num_instances < max_num_instances);
            instances[num_instances++] = iid;
        }
    }

    /* Time to fetch the instance info. */
    if (num_instances == 0) {
        free (instances);
	num_instances = pmGetInDom (inDom, &instlist, &namelist);
	if (num_instances == PM_ERR_IPC) {
	    pmDestroyContext (c->context);
	    c->context = -1;
	}
        if (num_instances < 1) {
            connstamp (cerr, connection) << "pmGetInDom failed" << endl;
            rc = num_instances;
            goto out;
        }
    } else {
        instlist = instances;
    }

    output << "{";
    json_key_value (output, "indom", inDom, ",");

    output << "\"instances\":[\n";
    printed_instances = 0;
    for (i = 0; i < num_instances; i++) {
        char *instance_name;
        if (namelist != NULL) {
            instance_name = namelist[i];
        } else {
            rc = pmNameInDom (inDom, instlist[i], &instance_name);
            if (rc != 0) {
                continue;		/* skip this instance quietly */
            }
        }

        if (printed_instances >= 1) {
            output << ",\n";
        }

        output << "{";
        json_key_value (output, "instance", instlist[i], ",");
        json_key_value (output, "name", string (instance_name));
        output << "}";
        if (namelist == NULL) {
            free (instance_name);
        }
        printed_instances++;	/* comma separation at beginning of loop */
    }
    output << "]}";		// iteration over instances

    /* Free no-longer-needed things: */
    free (instlist);
    if (namelist != NULL) {
        free (namelist);
    }
    {
        string s = output.str ();
        resp = NOTMHD_compressible_response (connection, s);
    }
    if (resp == NULL) {
        connstamp (cerr, connection) << "MHD_create_response_from_buffer failed" << endl;
        rc = -ENOMEM;
        goto out;
    }

    rc = MHD_add_response_header (resp, "Content-Type", "application/json");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    rc = MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header ACAO failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_queue_response failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }
    MHD_destroy_response (resp);
    return MHD_YES;
out1:
    MHD_destroy_response (resp);
out:
    return mhd_notify_error (connection, rc);
}


/* ------------------------------------------------------------------------ */

struct metric_prometheus_traverse_closure {
    struct MHD_Connection *connection;
    struct webcontext *c;
    ostringstream *output;
    vector<pmID> pmids;
    unsigned num_metrics_attempted;
    unsigned num_metrics_completed;
};


static void
metric_prometheus_traverse (const char *metric, void *closure)
{
    struct metric_prometheus_traverse_closure *mptc = (struct metric_prometheus_traverse_closure *)
            closure;
    assert (mptc != NULL);
    assert (mptc->connection != NULL);
    assert (mptc->c != NULL);
    assert (mptc->output != NULL);

    mptc->num_metrics_attempted++;

    int rc;
    char *metrics[1] = {
        (char *) metric
    };
    struct webcontext *c = mptc->c;
    pmID metric_id;

    if (c->metric_id_cache.find(metric) != c->metric_id_cache.end()) {
        metric_id = c->metric_id_cache.find(metric)->second;
    } else {
        rc = pmLookupName (1, metrics, &metric_id);
        if (rc != 1)
            return; // skip quietly
        c->metric_id_cache[metric] = metric_id;
        c->metric_name_cache[metric_id] = metric;
    }

    pmDesc metric_desc;
    assert (metric_id != PM_ID_NULL);

    if (c->metric_desc_cache.find(metric_id) == c->metric_desc_cache.end()) {
        rc = pmLookupDesc (metric_id, &metric_desc);
        if (rc != 0)
            return; // skip quietly
        c->metric_desc_cache[metric_id] = metric_desc;
    }

    char *help_text;
    if (c->metric_text_cache.find(metric_id) == c->metric_text_cache.end()) {
        rc = pmLookupText(metric_id, PM_TEXT_ONELINE, &help_text);
        if (rc != 0){
            return;
        }
        c->metric_text_cache[metric_id] = help_text;
    }

    mptc->pmids.push_back(metric_id);
}

static void
metric_prometheus_batch_fetch(void *closure) {
    struct metric_prometheus_traverse_closure *mptc = (struct metric_prometheus_traverse_closure *)
            closure;
    int rc;
    struct webcontext *c = mptc->c;
    pmResult *result;
    rc = pmFetch (mptc->pmids.size(), &mptc->pmids[0], &result);
    if (rc < 0) {
        char pmmsg[PM_MAXERRMSGLEN];
        connstamp (cerr, mptc->connection) << "pmFetch failed: " << pmErrStr_r (rc, pmmsg, sizeof (pmmsg)) << endl;
        return;
    }

    assert (result != NULL);
    int j;
    for(j=0; j<result->numpmid; j++) {
        pmValueSet* pv = result->vset[j]; // slot dedicated to our metric
        pmID metric_id = pv->pmid;
        stringstream output;
        assert (pv != NULL);
        if (pv->numval < 0) {
            char pmmsg[PM_MAXERRMSGLEN];
            connstamp (cerr, mptc->connection) << "pmFetch value failed: " << pmErrStr_r (pv->numval,
                                                                                    pmmsg, sizeof (pmmsg)) << endl;
            continue;
        }
        pmDesc metric_desc = c->metric_desc_cache.find(metric_id)->second;
        const char *help_text = c->metric_text_cache.find(metric_id)->second.c_str();
        const char *metric = c->metric_name_cache.find(metric_id)->second.c_str();
        // Reject non-numeric types; we'll convert to DOUBLE for prometheus
        if (metric_desc.type != PM_TYPE_32 &&
            metric_desc.type != PM_TYPE_U32 &&
            metric_desc.type != PM_TYPE_64 &&
            metric_desc.type != PM_TYPE_U64 &&
            metric_desc.type != PM_TYPE_FLOAT &&
            metric_desc.type != PM_TYPE_DOUBLE)
            continue; // skip quietly

        // Map the pcp pmns metric name into prometheus name.  Prometheus
        // imposes tighter constraints (no dots to separate names).  Ideally,
        // it should be reversible with respect to pmdaprometheus.
        // See also https://github.com/performancecopilot/pcp/issues/319 .
        string pn = metric;
        replace (pn.begin(), pn.end(), '.', ':');

        // Append a metric_desc.units-based suffix, and compute an
        // pmConvScale vector to match conventions as per
        // https://prometheus.io/docs/practices/naming/
        // Reject metrics with pcp pmunits of dimensionality inexpressible
        // in prometheus.
        pmUnits oconv = metric_desc.units;
        if (metric_desc.units.dimSpace == 1 &&
            metric_desc.units.dimTime == 0 &&
            metric_desc.units.dimCount == 0) {
            pn += "_bytes";
            oconv.dimSpace = 1;
            oconv.scaleSpace = PM_SPACE_BYTE;
        } else if (metric_desc.units.dimSpace == 0 &&
                   metric_desc.units.dimTime == 1 &&
                   metric_desc.units.dimCount == 0) {
            pn += "_seconds";
            oconv.dimTime = 1;
            oconv.scaleTime = PM_TIME_SEC;
        } else if (metric_desc.units.dimSpace == 0 &&
                   metric_desc.units.dimTime == 0 &&
                   metric_desc.units.dimCount == 1) {
            pn += "_count";
            oconv.dimCount = 1;
            oconv.scaleCount = PM_COUNT_ONE;
        } else if (metric_desc.units.dimSpace == 0 &&
                   metric_desc.units.dimTime == 0 &&
                   metric_desc.units.dimCount == 0) {
            // non-dimensional value, such as kernel.all.load
        } else {
            continue; // skip quietly
        }

        // append pcp metadata snapshot: metric semantics units
	char sembuf[64], unitsbuf[64];
	pmSemStr_r(metric_desc.sem, sembuf, sizeof(sembuf));
	pmUnitsStr_r(&metric_desc.units, unitsbuf, sizeof(unitsbuf));
	if (strlen(unitsbuf) == 0)
	    strncpy(unitsbuf, "none", sizeof(unitsbuf));
        output << "# PCP " << metric << " " << sembuf << " " << unitsbuf << endl;
        
        // append help text
        output << "# HELP " << pn << " " << help_text << endl;

        // append semantics tag
        if (metric_desc.sem == PM_SEM_COUNTER) {
            pn += "_total";
            output << "# TYPE " << pn << " counter" << endl;
        } else {
            output << "# TYPE " << pn << " gauge" << endl; // DISCRETE or INSTANT
        }

        // Iterate over the instances
        int i;
        if (c->metric_inst_cache.find(metric_id) == c->metric_inst_cache.end())
            c->metric_inst_cache[metric_id] = map<int, string>();
        map<int, string> &inst_cache = c->metric_inst_cache[metric_id];
        for (i=0; i<pv->numval; i++) {
            const pmValue* v = & pv->vlist[i];

            pmAtomValue extracted;
            rc = pmExtractValue (pv->valfmt, v, metric_desc.type, & extracted, PM_TYPE_DOUBLE);
            if (rc < 0) continue; // skip just this one

            pmAtomValue scaled;
            rc = pmConvScale (PM_TYPE_DOUBLE, & extracted, & metric_desc.units, & scaled, & oconv);
            if (rc < 0) continue; // skip just this one

            // Compute the "label" string from the instance name
            int inst = v->inst;
            string labels;
            if (inst < 0) { // not an instanced metric
                ;
            } else if (inst_cache.find(inst) != inst_cache.end()){
                labels = inst_cache[inst];
            } else {
                char *name;
                string instance_string;
                rc = pmNameInDom (metric_desc.indom, inst, & name);
                if (rc < 0)
                    instance_string="";
                else {
                    instance_string= escapeString(name);
                    free (name);
                }
                labels = "{instance=\"" + instance_string + "\"}";
                inst_cache[inst] = labels;
            }

            // Finally, output the line
            output << pn << labels << " ";
            if (std::numeric_limits<double>::is_specialized) {
                // print more bits of precision for larger numeric types
                output.precision (std::numeric_limits<double>::digits10 + 2 /* for rounding? */);
                output << scaled.d;
            } else {
                output << scaled.d;
            }
            output << endl;

            // NB: skip the timestamp
        }
        // Only now, with everything collected, append our data to the prometheus output stream
        (*mptc->output) << output.str() << endl; // plus a blank line between metrics
        mptc->num_metrics_completed++;
    }
}


static int
pmwebapi_respond_prometheus (struct MHD_Connection *connection,
                             const http_params &params,
                             struct webcontext *c)
{
    vector<string> metrics = params.find_all ("target");
    ostringstream output;
    struct MHD_Response *resp;
    int rc;
    
    metric_prometheus_traverse_closure mptc;
    mptc.connection = connection;
    mptc.c = c;
    mptc.output = & output;
    mptc.num_metrics_attempted = 0;
    mptc.num_metrics_completed = 0;
    for (unsigned i=0; i<metrics.size(); i++) {
        const string& m = metrics[i];
	rc = pmTraversePMNS_r (m.c_str(), &metric_prometheus_traverse, &mptc); /* cannot fail */
	if (rc == PM_ERR_IPC) {
	    pmDestroyContext (c->context);
	    mptc.c->context = c->context = -1;
	}
    }
    (void) metric_prometheus_batch_fetch(&mptc);
    (*mptc.output) << endl
                   << "# number of metrics attempted: " << mptc.num_metrics_attempted << endl
                   << "# number of metrics completed: " << mptc.num_metrics_completed << endl;
    
    string s = mptc.output->str ();
    resp = NOTMHD_compressible_response (connection, s);
    if (resp == NULL) {
        connstamp (cerr, connection) << "MHD_create_response_from_buffer failed" << endl;
        rc = -ENOMEM;
        goto out;
    }

    rc = MHD_add_response_header (resp, "Content-Type", "text/plain");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    rc = MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header ACAO failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_queue_response failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }
    MHD_destroy_response (resp);
    return MHD_YES;
out1:
    MHD_destroy_response (resp);
out:
    return mhd_notify_error (connection, rc);
}


/* ------------------------------------------------------------------------ */


static int
pmwebapi_respond_metric_store (struct MHD_Connection *connection,
                                const http_params & /*params*/,
                                struct webcontext *c)
{
    const char *val_pmid;
    const char *val_name;
    const char *val_value;
    const char *val_instance;
    const char *val_iname;
    struct MHD_Response *resp;
    int rc = 0;
    int num_values = 0;
    int max_num_instances;
    int num_instances;
    int *instances;
    pmID metric_id = PM_ID_NULL;
    pmDesc metric_desc;
    pmResult *result;
    pmValueSet *vset;
    pmAtomValue atom;
    ostringstream output;

    (void) c;
    val_pmid = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "pmid");
    if (val_pmid == NULL) {
        val_pmid = "";
    }
    val_name = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "name");
    if (val_name == NULL) {
        val_name = "";
    }
    val_instance = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND,
                   "instance");
    if (val_instance == NULL) {
        val_instance = "";
    }
    val_iname = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "iname");
    if (val_iname == NULL) {
        val_iname = "";
    }
    val_value = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "value");
    if (val_value == NULL) {
        connstamp (cerr, connection) << "store with no metric value" << endl;
        rc = PM_ERR_VALUE;
        goto out;
    }

    /* Extract target metric identifier from name or pmid. */
    if (*val_name != '\0') {
	rc = pmLookupName (1, (char **) &val_name, &metric_id);
	if (rc == PM_ERR_IPC) {
	    pmDestroyContext (c->context);
	    c->context = -1;
	}
        if (rc != 1) {
            connstamp (cerr, connection) << "failed to lookup metric " << val_name << endl;
            goto out;
        }
    }
    if (*val_pmid != '\0') {
        char *numend;
        unsigned long pmid = strtoul (val_pmid, &numend, 0); // accept hex too
        if (numend != val_pmid) {
            metric_id = pmid;
        }
    }

    if (metric_id == PM_ID_NULL) {
        connstamp (cerr, connection) << "store with no metric given" << endl;
        rc = PM_ERR_TOOSMALL;
        goto out;
    }
    rc = pmLookupDesc (metric_id, &metric_desc);
    if (rc == PM_ERR_IPC) {
	pmDestroyContext (c->context);
	c->context = -1;
    }
    if (rc != 0) {
        connstamp (cerr, connection) << "store failed to lookup desc" << endl;
        goto out;
    }
    if (metric_desc.type == PM_TYPE_AGGREGATE ||
        metric_desc.type == PM_TYPE_AGGREGATE_STATIC ||
        metric_desc.type == PM_TYPE_EVENT ||
        metric_desc.type == PM_TYPE_HIGHRES_EVENT) {
        connstamp (cerr, connection) << "cannot store aggregate or event metrics" << endl;
        rc = PM_ERR_TYPE;
        goto out;
    }

    /* Extract the supplied string value based on type. */
    rc = __pmStringValue (val_value, &atom, metric_desc.type);
    if (rc != 0) {
        connstamp (cerr, connection) << "store value conversion failed " << val_value << endl;
        rc = -ENOMEM;
        goto out;
    }

    /* Pessimistically overestimate maximum number of instance IDs needed. */
    max_num_instances = strlen (val_instance) + strlen (val_iname);
    num_instances = 0;
    instances = (int *) calloc ((size_t) max_num_instances, sizeof (int));
    if (instances == NULL) {
        connstamp (cerr, connection) << "calloc instances[" << max_num_instances << "] oom" <<
                                     endl;
        rc = -ENOMEM;
        goto out;
    }

    /* Loop over instance= numbers in val_instance, collect them in instances[]. */
    while (*val_instance != '\0') {
        char *numend;
        int iid = (int) strtoul (val_instance, &numend, 0);
        if (numend == val_instance) {
            break;		/* invalid contents */
        }
        assert (num_instances < max_num_instances);
        instances[num_instances++] = iid;
        if (*numend == '\0') {
            break;		/* end of string */
        }
        val_instance = numend + 1;	/* advance to next string */
    }

    /* Loop over iname= names in val_iname, collect them in instances[]. */
    while (*val_iname != '\0') {
        char *iname;
        const char *iname_end = strchr (val_iname, ',');
        int iid;
        /* Ignore plain "," XXX: elsewhere too? */
        if (*val_iname == ',') {
            val_iname++;
            continue;
        }

        if (iname_end) {
            iname = strndup (val_iname, (iname_end - val_iname));
            val_iname = iname_end + 1;	/* skip past , */
        } else {
            iname = strdup (val_iname);
            val_iname += strlen (val_iname);	/* skip onto \0 */
        }

        iid = pmLookupInDom (metric_desc.indom, iname);
        if (iid >= 0) {
            assert (num_instances < max_num_instances);
            instances[num_instances++] = iid;
        }
        free (iname);
    }

    /* Restrict instances to just the given set. */
    if (num_instances != 0) {
        pmDelProfile (metric_desc.indom, 0, NULL);
        rc = pmAddProfile (metric_desc.indom, num_instances, instances);
        if (rc != 0) {
            connstamp (cerr, connection) << "store profile failed" << endl;
            goto out;
        }
        free (instances);
	num_values = num_instances;
    } else {
	num_values = 1;
    }

    /* Setup the result structure with new value(s) and send it */
    if ((vset = (pmValueSet *) calloc (1, sizeof(pmValueSet) + sizeof(pmValue) * (num_values - 1))) == NULL) {
        connstamp (cerr, connection) << "store vset allocation failed" << endl;
        rc = -ENOMEM;
        goto out;
    }
    if ((result = (pmResult *) calloc (1, sizeof(pmResult))) == NULL) {
        connstamp (cerr, connection) << "store result allocation failed" << endl;
        free (vset);
        rc = -ENOMEM;
        goto out;
    }
    result->vset[0] = vset;
    result->numpmid = 1;

    for (int vi = 0; vi < num_values; vi++) {
        rc = __pmStuffValue (&atom, &vset->vlist[vi], metric_desc.type);
        if (rc < 0) {
            connstamp (cerr, connection) << "stuff value failed " << endl;
            free (result);
            free (vset);
            goto out;
        }
    }
    vset->valfmt = rc;
    vset->numval = num_values;
    vset->pmid = metric_id;

    rc = pmStore (result);
    pmFreeResult (result);

    if (rc < 0) {
        connstamp (cerr, connection) << "store value failed: " << pmErrStr(rc) << endl;
        goto out;
    }

    output << "{";
    json_key_value (output, "success", true);
    output << "}";		// iteration over instances

    {
        string s = output.str ();
        resp = NOTMHD_compressible_response (connection, s);
    }
    if (resp == NULL) {
        connstamp (cerr, connection) << "MHD_create_response_from_buffer failed" << endl;
        rc = -ENOMEM;
        goto out;
    }

    rc = MHD_add_response_header (resp, "Content-Type", "application/json");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    rc = MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header ACAO failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_queue_response failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }
    MHD_destroy_response (resp);
    return MHD_YES;
out1:
    MHD_destroy_response (resp);
out:
    return mhd_notify_error (connection, rc);
}


/* ------------------------------------------------------------------------ */


int
pmwebapi_respond (struct MHD_Connection *connection, const http_params & params,
                  const vector <string> &url)
{
    /* We emit CORS header for all successful json replies, namely:
       Access-Control-Access-Origin: *
       https://developer.mozilla.org/en-US/docs/HTTP/Access_control_CORS */

    /* NB: url is already edited to remove the /pmapi/ prefix. */
    unsigned long webapi_ctx;
    struct webcontext *c;
    char *context_end;
    string context_command;
    int rc = 0;
    context_map::iterator it;

    /* Decode the calls to the web API. */
    /* -------------------------------------------------------------------- */
    /* context creation */
    /* if-multithreaded: write-lock contexts */
    if (new_contexts_p &&		/* permitted */
            (url.size () == 3 && url[2] == "context")) {
        return pmwebapi_respond_new_context (connection, params);
    }

    /* -------------------------------------------------------------------- */
    /* All other calls use $CTX/command, so we parse $CTX
       generally and map it to the webcontext* */
    if (url.size () != 4) {
	connstamp (cerr, connection) << "url.size() " << url.size() << " not 4, url[2]=" << url[2] << ", new_contexts_p=" << new_contexts_p << endl;
        rc = -EINVAL;
        goto out;
    }

    errno = 0;
    webapi_ctx = strtoul (url[2].c_str (), &context_end, 10);	/* matches %d above */
    if (errno != 0 || webapi_ctx <= 0	/* range check, plus string-nonemptyness check */
            || webapi_ctx > INT_MAX	/* matches random() loop above */
            || *context_end != '\0') {
        /* fully parsed */
        connstamp (cerr, connection) << "unrecognized web context #" << url[2] << endl;
        rc = -EINVAL;
        goto out;
    }
    context_command = url[3];

    it = contexts.find (webapi_ctx);
    if (it == contexts.end ()) {
        connstamp (cerr, connection) << "unknown web context #" << webapi_ctx << endl;
        rc = PM_ERR_NOCONTEXT;
        goto out;
    }

    c = it->second;
    assert (c != NULL);
    /* Process HTTP Basic userid/password, if supplied.  Both returned strings
       need to be free(3)'d later.  */
    if (c->userid != "" || __pmServerHasFeature(PM_SERVER_FEATURE_CREDS_REQD)) {
        /* Did this context require userid/password auth? */
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
            resp = MHD_create_response_from_buffer (strlen (auth_req_msg), auth_req_msg,
                                                    MHD_RESPMEM_PERSISTENT);
            if (!resp) {
                rc = -ENOMEM;
                goto out;
            }

            /* We need the user to resubmit this with http
               authentication info, with a custom HTTP authentication
               realm for this context. */
            pmsprintf (auth_realm, sizeof (auth_realm), "%s/%ld", uriprefix.c_str (), webapi_ctx);
            rc = MHD_queue_basic_auth_fail_response (connection, auth_realm, resp);
            MHD_destroy_response (resp);
            if (rc != MHD_YES) {
                rc = -ENOMEM;
                goto out;
            }
            return MHD_YES;
        }
        /* 403 */
        if ((userid != c->userid) || (c->password != "" && (password != c->password))) {
            static char auth_failed_msg[] = "authentication failed";
            struct MHD_Response *resp;
            free (userid);
            free (password);
            resp = MHD_create_response_from_buffer (strlen (auth_failed_msg), auth_failed_msg,
                                                    MHD_RESPMEM_PERSISTENT);
            if (!resp) {
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
        time (&c->expires);
        c->expires += c->mypolltimeout;
    }

    /* Switch to this context for subsequent operations. */
    /* if-multithreaded: watch out. */
    if (c->context < 0)
	c->context = pmNewContext(PM_CONTEXT_HOST, c->spec.c_str());
    else
	rc = pmUseContext (c->context);
    if (rc) {
        char pmmsg[PM_MAXERRMSGLEN];
	connstamp (cerr, connection) << "pmUseContext(" << c->context << ") failed: " << pmErrStr_r (rc, pmmsg, sizeof (pmmsg)) << endl;
        goto out;
    }
    /* -------------------------------------------------------------------- */
    /* metric enumeration: /context/$ID/_metric */
    if (context_command == "_metric") {
        return pmwebapi_respond_metric_list (connection, params, c);
    }
    /* -------------------------------------------------------------------- */
    /* metric instance metadata: /context/$ID/_indom */
    else if (context_command == "_indom") {
        return pmwebapi_respond_instance_list (connection, params, c);
    }
    /* -------------------------------------------------------------------- */
    /* metric fetch: /context/$ID/_fetch */
    else if (context_command == "_fetch") {
        return pmwebapi_respond_metric_fetch (connection, params, c);
    }
    /* metric store: /context/$ID/_store */
    else if (context_command == "_store") {
        return pmwebapi_respond_metric_store (connection, params, c);
    }
    /* -------------------------------------------------------------------- */
    /* prometheus fetch: /context/$ID/metrics */
    else if (context_command == "metrics") {
        return pmwebapi_respond_prometheus (connection, params, c);
    }
    
    connstamp (cerr, connection) << "unknown context command " << context_command << endl;

    rc = -EINVAL;
out:
    return mhd_notify_error (connection, rc);
}
