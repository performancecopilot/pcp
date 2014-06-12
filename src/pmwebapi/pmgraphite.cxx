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

#define _XOPEN_SOURCE 600

#include "pmwebapi.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <set>

using namespace std;

extern "C"
{
#include <ctype.h>
#include <math.h>
#include <fts.h>
#include <fnmatch.h>

#ifdef HAVE_CAIRO
#include <cairo/cairo.h>
#endif
};


/*
 * We need a reversible encoding from arbitrary non-empty strings
 * (such as archive path names, pcp metric name components (?), pcp
 * instance names) to the dot-separated components of graphite metric
 * names.  We try to preserve safe characters and encode the rest.
 * (The resulting strings may well be url-encoded eventually for
 * transport across GET or POST parameters etc., but that's none of
 * our concern.)
 */
string
pmgraphite_metric_encode (const string & foo)
{
    stringstream output;
    static const char hex[] = "0123456789ABCDEF";

    assert (foo.size () > 0);
    for (unsigned i = 0; i < foo.size (); i++) {
        char c = foo[i];
        // Pass through enough characters to make the metric names relatively
        // human-readable in the javascript guis
        if (isalnum (c) || (c == '_') || (c == ' ')) {
            output << c;
        } else {
            output << "-" << hex[ (c >> 4) & 15] << hex[ (c >> 0) & 15] << "-";
        }
    }
    return output.str ();
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
            const char *p = lower_bound (hex, hex + 16, foo[i + 1]);
            if (*p != foo[i + 1]) {
                return "";
            }
            const char *q = lower_bound (hex, hex + 16, foo[i + 2]);
            if (*q != foo[i + 2]) {
                return "";
            }
            if ('-' != foo[i + 3]) {
                return "";
            }
            output += (char) (((p - hex) << 4) | (q - hex));
            i += 3;		// skip over hex bytes
        } else {
            output += c;
        }
    }
    return output;
}


// ------------------------------------------------------------------------


struct pmg_enum_context {
    vector <string> *patterns;
    vector <string> *output;
    string archivepart;
};



// Callback from pmTraversePMNS_r.  We have a working archive, we just received
// a working metric name.  All we need now is to enumerate
void
pmg_enumerate_pmns (const char *name, void *cls)
{
    pmg_enum_context *c = (pmg_enum_context *) cls;
    string metricpart = name;

    if (exit_p) {
        return;
    }

    // Filter out mismatches of the metric name components
    vector <string> metric_parts = split (metricpart, '.');
    for (unsigned i = 0; i < metric_parts.size (); i++) {
        if (c->patterns->size () > i + 1) {
            // patterns[0] was already used for the archive name
            const string & metricpart = metric_parts[i];
            const string & pattern = (*c->patterns)[i + 1];
            if (fnmatch (pattern.c_str (), metricpart.c_str (), FNM_NOESCAPE) != 0) {
                return;
            }
        }
    }

    string final_metric_name = c->archivepart + "." + metricpart;

    // look up the metric to make sure it exists; fan out to instance domains while at it
    char *namelist[1];
    pmID pmidlist[1];
    namelist[0] = (char *) name;
    int sts = pmLookupName (1, namelist, pmidlist);
    if (sts != 1) {
        return;
    }

    pmID pmid = pmidlist[0];
    pmDesc pmd;
    sts = pmLookupDesc (pmid, &pmd);
    if (sts != 0) {
        return;    // should not happen
    }

    if (pmd.indom == PM_INDOM_NULL) { // no more
        c->output->push_back (final_metric_name);
    } else { // nesting
        int *instlist;
        char **namelist;
        sts = pmGetInDomArchive (pmd.indom, &instlist, &namelist);
        if (sts >= 1) {
            for (int i=0; i<sts; i++) {
                string instance_part = pmgraphite_metric_encode (namelist[i]);
                // must filter out mismatches here too!
                if (c->patterns->size () > metric_parts.size ()+1) {
                    const string & pattern = (*c->patterns)[metric_parts.size ()+1];
                    if (fnmatch (pattern.c_str (), instance_part.c_str (), FNM_NOESCAPE) != 0) {
                        continue;
                    }
                }
                c->output->push_back (final_metric_name + "." + instance_part);
            }
            free (instlist);
            free (namelist);
        } else {
            // should not happen
        }
    }
}


// Heavy lifter.  Enumerate all archives, all metrics, all instances.
// This is not unbearably slow, since it involves only a scan of
// directories & metadata.

vector <string> pmgraphite_enumerate_metrics (struct MHD_Connection * connection,
        const string & pattern)
{
    vector <string> output;

    // The javascript guis may feed us wildcardy partial metric names.  We
    // apply them (via componentwise fnsearch(3)) as an optimization.
    vector <string> patterns_tok = split (pattern, '.');

    // We build up our graphite metric namespace from a couple of nested loops.

    // First up, archive name, as identified from an fts(3) search.  (It'd be
    // mighty handy to have a pmDiscoverServices("archive") kind of thing.)

    if (verbosity > 2) {
        connstamp (clog, connection) << "Searching for archives under " << archivesdir << endl;
    }

    char *fts_argv[2];
    fts_argv[0] = (char *) archivesdir.c_str ();
    fts_argv[1] = NULL;
    FTS *f = fts_open (fts_argv, (FTS_NOCHDIR | FTS_LOGICAL /* resolve symlinks */), NULL);
    if (f == NULL) {
        connstamp (cerr, connection) << "cannot fts_open " << archivesdir << endl;
        goto out;
    }
    for (FTSENT * ent = fts_read (f); ent != NULL; ent = fts_read (f)) {
        if (exit_p) {
            goto out;
        }

        if (ent->fts_info == FTS_SL) {
            // follow symlinks (unlikely)
            (void) fts_set (f, ent, FTS_FOLLOW);
        }

        if (fnmatch ("*.meta", ent->fts_path, FNM_NOESCAPE) != 0) {
            continue;
        }
        string archive = string (ent->fts_path);
        if (cursed_path_p (archivesdir, archive)) {
            continue;
        }

        // Abbrevate archive to clip off the archivesdir prefix (if
        // it's there).
        string archivepart = archive;
        if (archivepart.substr (0, archivesdir.size () + 1) == (archivesdir +
                (char) __pmPathSeparator ())) {
            archivepart = archivepart.substr (archivesdir.size () + 1);
        }
        archivepart = pmgraphite_metric_encode (archivepart);

        // Filter out mismatches of the first pattern component.
        // (note that this applies after _metric_encode().)
        if (patterns_tok.size () >= 1 &&	// have -some- specification
                ((patterns_tok[0] != archivepart) &&	// not identical
                 (fnmatch (patterns_tok[0].c_str (), archivepart.c_str (), FNM_NOESCAPE) != 0))) {
            // mismatches?
            continue;
        }

        int ctx = pmNewContext (PM_CONTEXT_ARCHIVE, archive.c_str ());
        if (ctx < 0) {
            continue;
        }

        // Wondertastic.  We have an archive.  Let's open 'er up and
        // enumerate them metrics.
        pmg_enum_context c;
        c.patterns = &patterns_tok;
        c.output = &output;
        c.archivepart = archivepart;

        (void) pmTraversePMNS_r ("", &pmg_enumerate_pmns, &c);

        pmDestroyContext (ctx);
    }
    fts_close (f);

out:
    if (verbosity > 2) {
        connstamp (clog, connection) << "enumerated " << output.size () << " metrics" << endl;
    }

    // As a service to the user, alpha-sort the returned list of metrics.
    sort (output.begin (), output.end ());

    return output;
}


// ------------------------------------------------------------------------


// This query traverses the metric tree one level at a time.  Incoming queries
// look like "foo.*", and we're expected to return "foo.bar", "foo.baz", etc.,
// differentiating leaf nodes from subtrees.
// We also handle the "format=completer" option.

int
pmgraphite_respond_metrics_find (struct MHD_Connection *connection,
                                 const http_params & params, const vector <string> &url)
{
    int rc;
    struct MHD_Response *resp;

    string query = params["query"];
    if (query == "") {
        return mhd_notify_error (connection, -EINVAL);
    }

    string format = params["format"];
    if (format == "") {
        format = "treejson";    // as per grafana
    }

    vector <string> query_tok = split (query, '.');
    unsigned path_match = query_tok.size () - 1;

    vector <string> metrics = pmgraphite_enumerate_metrics (connection, query);
    if (exit_p) {
        return MHD_NO;
    }

    // Classify the next piece of each metric name (at the
    // [path_match] position) as leafs and/or non-leafs.
    set <string> nodes;
    set <string> subtrees;
    set <string> leaves;

    // We still need the prior pieces though, for "id"="foo" purposes.
    string common_prefix;

    for (unsigned i = 0; i < metrics.size (); i++) {
        if (exit_p) {
            return MHD_NO;
        }

        vector <string> pieces = split (metrics[i], '.');

        // XXX: check that metrics[i] is a proper subtree of query
        // i.e., check pieces[0..path_match-1] -fnmatches- query_tok[0..path_match-1]
        if (pieces.size () <= path_match) {
            continue;		// should not happen
        }

        if (path_match > 0 && common_prefix == "") {
            // not yet computed
            // NB: an early piece can hypothetically include a wildcard; we can
            // propagate that through to the common_prefix, as graphite does
            for (unsigned j = 0; j < path_match; j++) {
                common_prefix += pieces[j] + ".";
            }
        }

        string piece = pieces[path_match];

        // NB: the same piece can be hypothetically listed in both
        // subtrees and leaves (e.g. "bar", if a pcp
        // metric.foo.bar.baz as well metric.foo.bar were to exist).
        if (pieces.size () > path_match+1) {
            subtrees.insert (piece);
        }
        if (pieces.size () == path_match+1) {
            leaves.insert (piece);
        }

        nodes.insert (piece);
    }

    // OK, time to generate some output.

    stringstream output;
    unsigned num_nodes = 0;

    if (format == "completer") {
        output << "{ \"metrics\": ";
    }

    output << "[";
    for (set <string>::iterator it = nodes.begin (); it != nodes.end ();
            it ++) {
        const string & node = *it;

        // NB: both of these could be true in principle
        bool leaved_p = leaves.find (node) != leaves.end ();
        bool subtreed_p = subtrees.find (node) != subtrees.end ();

        if (num_nodes++ > 0) {
            output << ",";
        }

        if (format == "completer") {
            output << "{";
            json_key_value (output, "name", (string) node, ",");
            json_key_value (output, "path", (string) (common_prefix + node), ",");
            json_key_value (output, "is_leaf", leaved_p);
            output << "}";
        } else {
            output << "{";
            json_key_value (output, "text", (string) node, ",");
            json_key_value (output, "id", (string) (common_prefix + node), ",");
            json_key_value (output, "leaf", leaved_p, ",");
            json_key_value (output, "expandable", subtreed_p, ",");
            json_key_value (output, "allowChildren", subtreed_p);
            output << "}";
        }
    }
    output << "]";

    if (format == "completer") {
        output << "}";
    }


    // wrap it up in mhd response ribbons
    string s = output.str ();
    resp = MHD_create_response_from_buffer (s.length (), (void *) s.c_str (),
                                            MHD_RESPMEM_MUST_COPY);
    if (resp == NULL) {
        connstamp (cerr, connection) << "MHD_create_response_from_buffer failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    /* https://developer.mozilla.org/en-US/docs/HTTP/Access_control_CORS */
    rc = MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header ACAO failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

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

out1:
    return mhd_notify_error (connection, rc);
}



// ------------------------------------------------------------------------


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
    string instance_name;
    unsigned entries_good, entries;
    struct timeval start;
    (void) gettimeofday (&start, NULL);

    // ^^^ several of these declarations are here (instead of at
    // point-of-use) only because we jump to an exit point, and may
    // not leap over an object ctor site.

    // XXX: in future, parse graphite functions-of-metrics
    // http://graphite.readthedocs.org/en/latest/functions.html
    //
    // XXX: in future, parse target-component wildcards
    //
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
            instance_name = pmgraphite_metric_decode (last_component);
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

        if (exit_p) {
            break;
        }

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

    // Rate conversion for COUNTER semantics values; perhaps should be a libpcp feature.
    // XXX: make this optional
    if (pmd.sem == PM_SEM_COUNTER && output.size () > 0) {
        // go backward, so we can do the calculation in one pass
        for (unsigned i = output.size () - 1; i > 0; i--) {
            float this_value = output[i].what;
            float last_value = output[i - 1].what;

            if (exit_p) {
                break;
            }

            if (this_value < last_value) { // suspected counter overflow
                output[i].what = nanf ("");
                continue;
            }

            // truncate time at seconds; we can't accurately subtract two large integers
            // when represented as floating point anyways
            time_t this_time = output[i].when.tv_sec;
            time_t last_time = output[i - 1].when.tv_sec;
            time_t delta = this_time - last_time;
            if (delta == 0) {
                delta = 1;    // some token protection against div-by-zero
            }

            if (isnanf (last_value) || isnanf (this_value)) {
                output[i].what = nanf ("");
            } else 
            {
                // avoid loss of significance risk of naively calculating
                // (this_v-last_v)/(this_t-last_t)
                output[i].what = (this_value / delta) - (last_value / delta);
            }
        }

        // we have nothing to rate-convert the first value to, so we nuke it
        output[0].what = nanf ("");
    }

    if (verbosity > 2) {
        struct timeval finish;
        (void) gettimeofday (&finish, NULL);

        string instance_spec;
        if (instance_name != "") {
            instance_spec = string ("[") + instance_name + string ("]");
        }
        connstamp (clog, connection) << "returned " << entries_good << "/" << entries
                                     << " data values"
                                     << " in " << __pmtimevalSub (&finish,&start)*1000 << "ms"
                                     << ", metric " << metric_name << instance_spec
                                     << ", timespan [" << t_start << "-" << t_end <<
                                     "] by " << t_step << "s" << endl;
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
// The exact syntax permitted is tricky.  It's only partially documented
// http://graphite.readthedocs.org/en/latest/render_api.html#data-display-formats
// whereas implementation in graphite/render/attime.py is concrete but too much
// to duplicate here.
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
    char *end;
    struct tm parsed;

    if (value == "") {
        connstamp (cerr, connection) << "empty graphite timespec" << endl;
        return now.tv_sec;
    }

    // detect the HH:MM_YYYYMMDD absolute-time format emitted by grafana
    memset (&parsed, 0, sizeof (parsed));
    end = strptime (value.c_str (), "%H:%M_%Y%m%d", &parsed);
    if (end != NULL && *end == '\0') {
        // success
        return mktime (&parsed);
    }

    // likewise YYYYMMDD
    memset (&parsed, 0, sizeof (parsed));
    end = strptime (value.c_str (), "%Y%m%d", &parsed);
    if (end != NULL && *end == '\0') {
        // success
        return mktime (&parsed);
    }


    if (value[0] != '-') {
        // nonnegative?  lead __pmParseTime to interpret it as absolute
        // (this is why we take string value instead of const string& parameter)
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

// Decode graphite URL pieces toward data gathering: specifically enough
// to identify validated metrics and time bounds.

int
pmgraphite_gather_data (struct MHD_Connection *connection, const http_params & params,
                        const vector <string> &url,
                        vector<string>& targets,
                        time_t& t_start,
                        time_t& t_end,
                        time_t& t_step)
{
    int rc = 0;

    vector <string> target_patterns = params.find_all ("target");

    // The patterns may have wildcards; expand the bad boys.
    for (unsigned i=0; i<target_patterns.size (); i++) {
        unsigned pattern_length = count (target_patterns[i].begin (), target_patterns[i].end (), '.');
        vector <string> metrics = pmgraphite_enumerate_metrics (connection, target_patterns[i]);
        if (exit_p) {
            break;
        }

        // NB: the entries in enumerated metrics[] may be wider than
        // the incoming pattern, for example for a wildcard like *.*
        // We need to filter out those enumerated ones that are longer
        // (have more dot-components) than the incoming pattern.

        for (unsigned i=0; i<metrics.size (); i++)
            if (pattern_length == count (metrics[i].begin (), metrics[i].end (), '.')) {
                targets.push_back (metrics[i]);
            }
    }

    // same defaults as python graphite/graphlot/views.py
    string from = params["from"];
    if (from == "") {
        from = "-24hour";
    }
    string until = params["until"];
    if (until == "") {
        until = "-0hour";
    }

    t_start = pmgraphite_parse_timespec (connection, from);
    t_end = pmgraphite_parse_timespec (connection, until);

    // We could hard-code t_step = 60 as in the /rawdata case, since that is the
    // typical sampling rate for graphite as well as pcp.  But maybe a graphite
    // webapp (grafana) can't handle as many as that.
    int maxdatapt = atoi (params["maxDataPoints"].c_str ());	// ignore failures
    if (maxdatapt <= 0) {
        maxdatapt = 1024;		// a sensible upper limit?
    }

    t_step = 60;		// a default, but ...
    if (((t_end - t_start) / t_step) > maxdatapt) {
        // make it larger if needed
        t_step = ((t_end - t_start) / maxdatapt) + 1;
    }

    return rc;
}


/* ------------------------------------------------------------------------ */


cairo_status_t cairo_write_to_string(void *cls, const unsigned char* data, unsigned int length)
{
    string* s = (string *) cls;
    (*s) += string((const char*) data, (size_t) length); // XXX: might throw
    return CAIRO_STATUS_SUCCESS;
}


void cairo_parse_color (const string& name, double& r, double& g, double& b)
{
    r = (random() % 256) / 255.0;
    g = (random() % 256) / 255.0;
    b = (random() % 256) / 255.0;
}



// Render archive data to an image.  It doesn't need to be as pretty
// as the version built into the graphite server-side code, just
// serviceable.

int
pmgraphite_respond_render_gfx (struct MHD_Connection *connection,
                               const http_params & params, const vector <string> &url)
{
    int rc;
    struct MHD_Response *resp;
    string imgbuf;
    cairo_status_t cst;
    cairo_surface_t *sfc;
    cairo_t *cr;
    vector< vector<timestamped_float> > all_results;

    string format = params["format"];
    if (format == "")
        format = "png";
    
    // SVG? etc?
    if (format != "png")
        return mhd_notify_error (connection, -EINVAL);        

    vector <string> targets;
    time_t t_start, t_end, t_step;
    rc = pmgraphite_gather_data (connection, params, url, targets, t_start, t_end, t_step);
    if (rc) {
        return mhd_notify_error (connection, rc);
    }

    int width = atoi (params["width"].c_str());
    if (width <= 0) width = 640;
    int height = atoi (params["height"].c_str()); 
    if (height <= 0) height = 480;

    sfc = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    if (sfc == NULL) {
        rc = -ENOMEM;
        goto out1;
    }
        
    cr = cairo_create (sfc);
    if (cr == NULL) {
        rc = -ENOMEM;
        goto out2;
    }

    double r, g, b;
    cairo_parse_color (params["bgcolor"], r, g, b);
    cairo_save(cr);
    cairo_set_source_rgb (cr, r, g, b);
    cairo_rectangle (cr, 0.0, 0.0, width, height);
    cairo_fill(cr);
    cairo_restore(cr);

    // Gather up all the data.  We need several passes over it, so gather it into a vector<vector<>>.
    for (unsigned k = 0; k < targets.size (); k++) {
        if (exit_p) {
            break;
        }
        all_results.push_back (pmgraphite_fetch_series (connection, targets[k], t_start, t_end, t_step));
    }


    // Compute vertical bounds.
    float ymin;
    if (params["yMin"] != "")
        ymin = atof (params["yMin"].c_str());
    else {
        ymin = nanf("");
        for (unsigned i=0; i<all_results.size(); i++)
            for (unsigned j=0; j<all_results[i].size(); j++)
                ymin = isnanf(ymin) ? all_results[i][j].what : min(ymin, all_results[i][j].what);
    }
    float ymax;
    if (params["yMax"] != "")
        ymax = atof (params["yMax"].c_str());
    else {
        ymax = nanf("");
        for (unsigned i=0; i<all_results.size(); i++)
            for (unsigned j=0; j<all_results[i].size(); j++)
                ymax = isnanf(ymax) ? all_results[i][j].what : max(ymax, all_results[i][j].what);
    }

    if (verbosity)
        connstamp(clog, connection) << "Rendering " << all_results.size() << " metrics" 
                                    << ", ymin=" << ymin << ", ymax=" << ymax << endl;


    // Any data to show?
    if (isnanf(ymin) || isnanf(ymax) || all_results.empty())
        {
        cairo_text_extents_t ext;
        string message = "no data in range";
        cairo_select_font_face (cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size (cr, 32.0);
        cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
        cairo_text_extents (cr, message.c_str(), &ext);
        cairo_move_to (cr,
                       width/2.0 - (ext.width/2 + ext.x_bearing),
                       height/2.0 - (ext.height/2 + ext.y_bearing));
        cairo_show_text (cr, message.c_str());
    }

    // Because we're going for basic acceptable rendering only, for
    // purposes of graphite-builder previews, we hard-code a simple
    // layout scheme:
    //
    // outer 10% of area: labels
    // inner 80% of area: graph lines
    //
    // As a mnemonic, double typed variables are used to track
    // coordinates in graphics space, and float (with nan) for pcp
    // metric space.

    // Draw the grid

    // Draw the labels

    // Draw the curves
    for (unsigned i=0; i<all_results.size(); i++) {
        const vector<timestamped_float>& f = all_results[i];
        const string& metric_name = targets[i];

        double r,g,b;
        cairo_save(cr);
        cairo_parse_color (metric_name, r, g, b);
        cairo_set_source_rgb(cr, r, g, b);

        double lastx = nan("");
        double lasty = nan("");
        for (unsigned j=0; j<f.size(); j++) {
            double line_width = 2.5;
            cairo_set_line_width (cr, line_width);
            
            if (isnanf(f[j].what)) {
                // This data slot is missing, so put a circle at the previous end, if
                // possible, to indicate the discontinuity
                if (! isnan(lastx) && ! isnan(lasty)) {
                    cairo_move_to (cr, lastx, lasty);
                    cairo_arc (cr, lastx, lasty, line_width*0.5, 0., 2*M_PI);
                    cairo_stroke(cr);
                    continue;
                }
            }

            float xdelta = (t_end - t_start);
            float ydelta = (ymax - ymin);
            double relx = ((double)(f[j].when.tv_sec + f[j].when.tv_usec/1000000.0)/xdelta)  -
                ((double)t_start/xdelta);
            double rely = (double)ymax/ydelta - (double)f[j].what/ydelta;

            double x = width*0.10 + width*0.80*relx; // scaled into graphics grid area
            double y = height*0.10 + height*0.80*rely;

            cairo_move_to (cr, x, y);
            if (! isnan(lastx) && ! isnan(lasty)) {
                // draw it as a line
                cairo_line_to (cr, lastx, lasty);
            } else {
                // draw it as a circle
                cairo_arc (cr, x, y, line_width*0.5, 0., 2*M_PI);
            }
            cairo_stroke(cr);

            lastx = x;
            lasty = y;
        }

        cairo_restore(cr);
    }
    
    
    // Need to get the data out of cairo and into microhttpd.  We use
    // a c++ string as a temporary, which can carry binary data.

    cst = cairo_surface_write_to_png_stream (sfc, & cairo_write_to_string, (void *) & imgbuf);
    if (cst != CAIRO_STATUS_SUCCESS || cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        rc = -EIO;
        goto out3;
    }

    resp = MHD_create_response_from_buffer (imgbuf.size(), (void*) imgbuf.c_str(), MHD_RESPMEM_MUST_COPY);
    if (resp == NULL) {
        connstamp (cerr, connection) << "MHD_create_response_from_buffer failed" << endl;
        rc = -ENOMEM;
        goto out3;
    }

    /* https://developer.mozilla.org/en-US/docs/HTTP/Access_control_CORS */
    rc = MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header ACAO failed" << endl;
        rc = -ENOMEM;
        goto out4;
    }

    rc = MHD_add_response_header (resp, "Content-Type", "image/png");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header CT failed" << endl;
        rc = -ENOMEM;
        goto out4;
    }
    rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_queue_response failed" << endl;
        rc = -ENOMEM;
        goto out4;
    }

    rc = 0;

 out4:
    MHD_destroy_response (resp);
 out3:
    cairo_destroy (cr);
 out2:
    cairo_surface_destroy (sfc);
 out1:
    if (rc)
        return mhd_notify_error (connection, rc);
    else
        return MHD_YES;
}


/* ------------------------------------------------------------------------ */


// Render raw archive data in JSON form.
int
pmgraphite_respond_render_json (struct MHD_Connection *connection,
                                const http_params & params, const vector <string> &url,
                                bool rawdata_flavour_p)
{
    int rc;
    struct MHD_Response *resp;

    vector <string> targets;
    time_t t_start, t_end, t_step;
    rc = pmgraphite_gather_data (connection, params, url, targets, t_start, t_end, t_step);
    if (rc) {
        return mhd_notify_error (connection, rc);
    }

    stringstream output;
    output << "[";
    for (unsigned k = 0; k < targets.size (); k++) {
        string target = targets[k];

        if (exit_p) {
            break;
        }

        vector <timestamped_float> results = pmgraphite_fetch_series (connection, target, t_start,
                                             t_end, t_step);

        if (k > 0) {
            output << ",";
        }

        if (rawdata_flavour_p) {
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
                if (! isnormal (results[i].what)) {
                    output << "null";
                } else {
                    output << results[i].what;
                }
            }
            output << "]}";

        } else {
            output << "{";
            json_key_value (output, "target", string (target), ",");
            output << " \"datapoints\":[";
            for (unsigned i = 0; i < results.size (); i++) {
                if (i > 0) {
                    output << ",";
                }
                output << "[";
                if (isnanf (results[i].what)) {
                    output << "null";
                } else {
                    output << results[i].what;
                }
                output << ", " << results[i].when.tv_sec << "]";
            }
            output << "]}";
        }
    }
    output << "]";

    // wrap it up in mhd response ribbons
    string s = output.str ();
    resp = MHD_create_response_from_buffer (s.length (), (void *) s.c_str (),
                                            MHD_RESPMEM_MUST_COPY);
    if (resp == NULL) {
        connstamp (cerr, connection) << "MHD_create_response_from_buffer failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

    /* https://developer.mozilla.org/en-US/docs/HTTP/Access_control_CORS */
    rc = MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");
    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_add_response_header ACAO failed" << endl;
        rc = -ENOMEM;
        goto out1;
    }

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

out1:
    return mhd_notify_error (connection, rc);
}



/* ------------------------------------------------------------------------ */



/* ------------------------------------------------------------------------ */


int
pmgraphite_respond (struct MHD_Connection *connection, const http_params & params,
                    const vector <string> &url)
{
    string url1 = (url.size () >= 2) ? url[1] : "";
    assert (url1 == "graphite");
    string url2 = (url.size () >= 3) ? url[2] : "";
    string url3 = (url.size () >= 4) ? url[3] : "";

    if (url2 == "rawdata") {
        // graphlot style
        return pmgraphite_respond_render_json (connection, params, url, true);
    } else if (url2 == "render" && params["format"] == "json") {
        // grafana style
        return pmgraphite_respond_render_json (connection, params, url, false);
    } else if (url2 == "metrics" && url3 == "find") {
        // grafana, graphite tree & auto-completer
        return pmgraphite_respond_metrics_find (connection, params, url);
    }
#if 0
    else if (url2 == "graphlot" && url3 == "findmetric") {
        // graphlot
        return pmgraphite_respond_metrics_findmetric (connection, params, url);
    } else if (url2 == "browser" && url3 == "search") {
        // graphite search
        return pmgraphite_respond_metrics_findmetric (connection, params, url);
    }
#endif
    else if (url2 == "render") {
        return pmgraphite_respond_render_gfx (connection, params, url);
    }

    return mhd_notify_error (connection, -EINVAL);
}
