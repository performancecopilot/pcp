/*
 * PMWEBD graphite-api emulation
 *
 * Copyright (c) 2014 Red Hat Inc.
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

#include <algorithm>
#include <iostream>
#include <sstream>

using namespace std;

extern "C"
{
#include <ctype.h>
#include <math.h>

#ifdef HAVE_CAIRO
#include <cairo/cairo.h>
#endif
};


/*
 * We need a reversible encoding from arbitrary non-empty strings
 * (such as archive path names, pcp metric names, pcp instance names)
 * to the dot-separated components of graphite metric names.  We try
 * to preserve safe characters and encode the rest.  (The resulting
 * strings may well be url-encoded eventually for transport across GET
 * or POST parameters etc., but that's none of our concern.)
 */
string
pmgraphite_metric_encode (const string & foo)
{
    string output;
    static const char hex[] = "0123456789ABCDEF";

    assert (foo.size () > 0);
    for (unsigned i = 0; i < foo.size (); i++) {
        char c = foo[i];
        // pass through enough characters to make the metric names relatively
        // human-readable in the javascript guis
        if (isalnum (c) || (c == '_')) {
            output += c;
        } else {
            output += "-" + hex[ (c >> 4) & 15] + hex[ (c >> 0) & 15] + string ("-");
        }
    }
    return output;
}

// NB: decoding failure is possible (could arise from mischevious URLs
// being fed to us) and is signalled with an empty return string.
string
pmgraphite_metric_decode (const string & foo)
{
    string output;
    static const char hex[] = "0123456789ABCDEF";
    for (unsigned i = 0; i < foo.size (); i++) {
        char c = foo[i];
        if (c == '-') {
            if (i + 3 >= foo.size ()) {
                return "";
            }
            if (foo[i + 3] != '-') {
                return "";
            }
            const char *p = lower_bound (hex, hex + 16, foo[i + 1]);
            if (*p != foo[i + 1]) {
                return "";
            }
            const char *q = lower_bound (hex, hex + 16, foo[i + 2]);
            if (*q != foo[i + 2]) {
                return "";
            }
            output += (char) (((p - hex) << 4) | (q - hex));
            i += 3;		// skip over hex bytes too
        } else {
            output += c;
        }
    }
    return output;

}


struct timestamped_float {
    // a POD for graphite
    struct timeval when;
    float what;
};


// Heavy lifter.  Parse graphite "target" name into archive
// file/directory, metric name, and (if appropriate) instance within
// metric indom; fetch all the data values interpolated between given
// inclusive-end time points, and assemble them into single
// series-of-numbers for rendering.
//
// A lot can go wrong, but is signalled only with a stderr message and
// an empty vector.  (As a matter of security, we prefer not to give too
// much information to a remote web user about the exact error.)  Occasional
// missing metric values are represented as floating-point NaN values.
vector <timestamped_float> pmgraphite_fetch_series (struct MHD_Connection *connection,
        const string & target, time_t t_start, time_t t_end, time_t t_step)	// time bounds
{
    vector <timestamped_float> output;
    int sts;
    string last_component;
    string metric_name;
    int pmc;
    string archive;
    string archive_part;
    unsigned entries_good, entries;
    // ^^^ several of these declarations are here (instead of at
    // point-of-use) only because we jump to an exit point, and may
    // not leap over an object ctor site.

    // XXX: in future, parse graphite functions-of-metrics
    // XXX: in future, parse target-component wildcards
    // XXX: in future, cache the pmid/pmdesc/inst# -> pcp-context

    vector <string> target_tok = split (target, '.');
    if (target_tok.size () < 2) {
        connstamp (cerr, connection) << "not enough target components" << endl;
        goto out0;
    }
    for (unsigned i = 0; i < target_tok.size (); i++)
        if (target_tok[i] == "") {
            connstamp (cerr, connection) << "empty target components" << endl;
            goto out0;
        }
    // Extract the archive file/directory name
    archive_part = pmgraphite_metric_decode (target_tok[0]);
    if (archive_part == "") {
        connstamp (cerr, connection) << "undecodeable archive-path " << target_tok[0] << endl;
        goto out0;
    }
    if (__pmAbsolutePath ((char *) archive_part.c_str ())) {
        // accept absolute paths too
        archive = archive_part;
    } else {
        archive = archivesdir + (char) __pmPathSeparator () + archive_part;
    }

    if (cursed_path_p (archivesdir, archive)) {
        connstamp (cerr, connection) << "invalid archive path " << archive << endl;
        goto out0;
    }
    // Open the bad boy.
    // XXX: if it's a directory, redirect to the newest entry
    pmc = pmNewContext (PM_CONTEXT_ARCHIVE, archive.c_str ());
    if (pmc < 0) {
        // error already noted
        goto out0;
    }
    if (verbosity > 2) {
        connstamp (clog, connection) << "opened archive " << archive << endl;
    }

    // NB: past this point, exit via 'goto out;' to release pmc

    // We need to decide whether the next dotted components represent
    // a metric name, or whether there is an instance name squished at
    // the end.
    metric_name = "";
    for (unsigned i = 1; i < target_tok.size () - 1; i++) {
        const string & piece = target_tok[i];
        if (i > 1) {
            metric_name += '.';
        }
        metric_name += piece;
    }
    last_component = target_tok[target_tok.size () - 1];

    pmID pmid;			// as yet unknown
    pmDesc pmd;

    {
        char *namelist[1];
        pmID pmidlist[1];
        namelist[0] = (char *) metric_name.c_str ();
        int sts = pmLookupName (1, namelist, pmidlist);

        if (sts == 1) {
            // found ... last name must be instance domain name
            pmid = pmidlist[0];
            sts = pmLookupDesc (pmid, &pmd);
            if (sts != 0) {
                connstamp (clog, connection) << "cannot find metric descriptor " << metric_name << endl;
                goto out;
            }
            // check that there is an instance domain, in order to use that last component
            if (pmd.indom == PM_INDOM_NULL) {
                connstamp (clog, connection) << "metric " << metric_name << " lacks expected indom " <<
                                             last_component << endl;
                goto out;

            }
            // look up that instance name
            string instance_name = pmgraphite_metric_decode (last_component);
            int inst = pmLookupInDomArchive (pmd.indom,
                                             (char *) instance_name.c_str ());	// XXX: why not pmLookupInDom?
            if (inst < 0) {
                connstamp (clog, connection) << "metric " << metric_name << " lacks recognized indom " <<
                                             last_component << endl;
                goto out;
            }
            // activate only that instance name in the profile
            sts = pmDelProfile (pmd.indom, 0, NULL);
            sts |= pmAddProfile (pmd.indom, 1, &inst);
            if (sts != 0) {
                connstamp (clog, connection) << "metric " << metric_name <<
                                             " cannot set unitary instance profile " << inst << endl;
                goto out;
            }
        } else {
            // not found ... ok, try again with that last component
            metric_name = metric_name + '.' + last_component;
            namelist[0] = (char *) metric_name.c_str ();
            int sts = pmLookupName (1, namelist, pmidlist);
            if (sts != 1) {
                // still not found .. give up
                connstamp (clog, connection) << "cannot find metric name " << metric_name << endl;
                goto out;
            }

            pmid = pmidlist[0];
            sts = pmLookupDesc (pmid, &pmd);
            if (sts != 0) {
                connstamp (clog, connection) << "cannot find metric descriptor " << metric_name << endl;
                goto out;
            }
            // check that there is no instance domain
            if (pmd.indom != PM_INDOM_NULL) {
                connstamp (clog, connection) << "metric " << metric_name << " has unexpected indom " <<
                                             pmd.indom << endl;
                goto out;
            }
        }
    }

    // OK, to recap, if we got this far, we have an open pmcontext to the archive,
    // a looked-up pmID and its pmDesc, and validated instance-profile.

    // Check that the pmDesc type is numeric
    switch (pmd.type) {
    case PM_TYPE_32:
    case PM_TYPE_U32:
    case PM_TYPE_64:
    case PM_TYPE_U64:
    case PM_TYPE_FLOAT:
    case PM_TYPE_DOUBLE:
        break;
    default:
        connstamp (clog, connection) << "metric " << metric_name << " has unsupported type " <<
                                     pmd.type << endl;
        goto out;
    }

    // Time to iterate across time and space, and get us some tasty values

    struct timeval start_timeval;
    start_timeval.tv_sec = t_start;
    start_timeval.tv_usec = 0;
    sts = pmSetMode (PM_MODE_INTERP | PM_XTB_SET (PM_TIME_SEC), &start_timeval, t_step);
    if (sts != 0) {
        connstamp (clog, connection) << "cannot set time mode origin/delta" << endl;
        goto out;
    }

    // inclusive iteration from t_start to t_end
    entries_good = entries = 0;
    for (time_t iteration_time = t_start; iteration_time <= t_end; iteration_time += t_step) {
        pmID pmidlist[1];
        pmidlist[0] = pmid;
        pmResult *result;

        entries++;

        timestamped_float x;
        x.when.tv_sec = iteration_time;
        x.when.tv_usec = 0;
        x.what = nanf ("");	// initialize to a NaN

        int sts = pmFetch (1, pmidlist, &result);
        if (sts == 0 && result->vset[0]->numval == 1) {
            pmAtomValue value;
            sts = pmExtractValue (result->vset[0]->valfmt,
                                  &result->vset[0]->vlist[0],	// we know: one pmid, one instance
                                  pmd.type, &value, PM_TYPE_FLOAT);
            if (sts == 0) {
                x.when = result->timestamp;	// should generally match iteration_time
                x.what = value.f;
                entries_good++;
            }

            pmFreeResult (result);
        }

        output.push_back (x);
    }

    if (verbosity > 2) {
        connstamp (clog, connection) << "returned " << entries_good << "/" << entries <<
                                     " data values, metric " << metric_name << endl;
    }

    // Done!

out:
    pmDestroyContext (pmc);
out0:
    return output;
}




/* ------------------------------------------------------------------------ */


// Attempt to parse a graphite time-specification value, such as the parameter
// to the /graphite/rawdata/from=*&until=* parameters.  Negative values are
// relative to "now"; non-negative values are considered absolute.
//
// Return the absolute seconds value, or "now" in case of error.

time_t
pmgraphite_parse_timespec (struct MHD_Connection * connection, string value)
{
    // just delegate to __pmParseTime()
    struct timeval now;
    (void) gettimeofday (&now, NULL);
    struct timeval result;
    char *errmsg = NULL;

    if (value == "") {
        connstamp (cerr, connection) << "empty graphite timespec" << endl;
        return now.tv_sec;
    }
    if (value[0] != '-') {
        // nonnegative?  lead __pmParseTime to interpret it as absolute
        value = string ("@") + value;
    }

    int sts = __pmParseTime (value.c_str (), &now, &now, &result, &errmsg);
    if (sts != 0) {
        connstamp (cerr, connection) << "unparseable graphite timespec " << value << ": " <<
                                     errmsg << endl;
        free (errmsg);
        return now.tv_sec;
    }

    return result.tv_sec;
}



/* ------------------------------------------------------------------------ */




/* ------------------------------------------------------------------------ */

int
pmgraphite_respond (struct MHD_Connection *connection, const vector <string> &url)
{
    int rc;
    struct MHD_Response *resp;

    string url1 = (url.size () >= 2) ? url[1] : "";
    assert (url1 == "graphite");
    string url2 = (url.size () >= 3) ? url[2] : "";
    string url3 = (url.size () >= 4) ? url[3] : "";

    if (url2 == "rawdata") {
        // as used by graphlot

        // XXX: multiple &target=FOOBAR possible
        const char *target = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND,
                             "target");
        if (target == NULL) {
            goto out1;
        }
        // same defaults as python graphite/graphlot/views.py
        const char *from = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND,
                           "from");
        if (from == NULL) {
            from = "-24hour";
        }
        const char *until = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND,
                            "until");
        if (until == NULL) {
            until = "-0hour";
        }

        time_t t_start = pmgraphite_parse_timespec (connection, from);
        time_t t_end = pmgraphite_parse_timespec (connection, until);
        time_t t_step = 60;	// XXX: hard-coded?
        vector <timestamped_float> results = pmgraphite_fetch_series (connection, target, t_start,
                                             t_end, t_step);

        stringstream output;
        output << "[";
        output << "{";
        json_key_value (output, "start", t_start, ",");
        json_key_value (output, "step", t_step, ",");
        json_key_value (output, "end", t_end, ",");
        json_key_value (output, "name", string (target), ",");
        output << " \"data\":[";
        for (unsigned i = 0; i < results.size (); i++) {
            if (i > 0) {
                output << ",";
            }
            if (isnan (results[i].what)) {
                output << "null";
            } else {
                output << results[i].what;
            }
        }
        output << "]}]";

        // wrap it up in mhd response ribbons
        string s = output.str ();
        resp = MHD_create_response_from_buffer (s.length (), (void *) s.c_str (),
                                                MHD_RESPMEM_MUST_COPY);
        if (resp == NULL) {
            connstamp (cerr, connection) << "MHD_create_response_from_buffer failed" << endl;
            rc = -ENOMEM;
            goto out;
        }
#if 0
        /* https://developer.mozilla.org/en-US/docs/HTTP/Access_control_CORS */
        rc = MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");
        if (rc != MHD_YES) {
            connstamp (cerr, connection) << "MHD_add_response_header ACAO failed" << endl;
            rc = -ENOMEM;
            goto out1;
        }
#endif
        rc = MHD_add_response_header (resp, "Content-Type", "application/json");
        if (rc != MHD_YES) {
            connstamp (cerr, connection) << "MHD_add_response_header CT failed" << endl;
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
    }

out1:
    rc = -EINVAL;

out:
    return mhd_notify_error (connection, rc);
}
