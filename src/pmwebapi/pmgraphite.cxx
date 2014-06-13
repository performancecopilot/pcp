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
            } else {
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


#ifdef HAVE_CAIRO

cairo_status_t notcairo_write_to_string (void *cls, const unsigned char* data, unsigned int length)
{
    string* s = (string *) cls;
    (*s) += string ((const char*) data, (size_t) length); // XXX: might throw
    return CAIRO_STATUS_SUCCESS;
}


// Generate a color wheel, with entries for each of targets[].
//
// The first parameter is an optional graphite/render "colorList"
// style comma-separated set of names, which may well be shorter than
// than targets[].
vector<string> generate_colorlist (const vector<string>& colorList, const vector<string>& targets)
{
    vector<string> output;

    for (unsigned i=0; i<targets.size (); i++) {
        if (colorList.size () > 0) {
            output.push_back (colorList[i % colorList.size ()]);
        } else {
            output.push_back (targets[i]);
        }
    }

    return output;
}


// Parse given name (3- or 6-digit hex or CSS3 names) as a color,
// return cairo-style 0.0-1.0 r,g,b tuples.
//
// This is unpleasantly verbose, but cairo etc. appear to be of no
// help.
//
// As a bonus, if the string cannot be parsed the usual way,
// synthesize a random one based upon a hash of the name.  This way,
// metric names finding their way down here can get a persistent
// color.

struct rgb {
    double r;
    double g;
    double b;
    rgb (): r (0), g (0), b (0) {}
    rgb (short r, short g, short b): r (r/255.0), g (g/255.0), b (b/255.0) {}
};

void notcairo_parse_color (const string& name, double& r, double& g, double& b)
{
    // try to parse RRGGBB RGB #RRGGBB #RGB forms
    static const char hex[] = "0123456789ABCDEF";
    string name2;
    if ((name.size () == 4 || name.size () == 7) && (name[0] == '#')) {
        name2 = name.substr (1);    // clip off #
    } else {
        name2 = name;
    }

#define PARSE1(V,I) \
        const char *V = lower_bound (hex, hex+16, toupper(name2[I])); \
        if (*V != toupper(name2[I])) goto try_name;

    if (name2.size () == 6) { // try RRGGBB
        PARSE1 (r1,0);
        PARSE1 (r2,1);
        PARSE1 (g1,2);
        PARSE1 (g2,3);
        PARSE1 (b1,4);
        PARSE1 (b2,5);
        r = (r1 - hex) / 16.0 + (r2 - hex) / 16.0 / 15.0;
        g = (g1 - hex) / 16.0 + (g2 - hex) / 16.0 / 15.0;
        b = (b1 - hex) / 16.0 + (b2 - hex) / 16.0 / 15.0;
        return;
    } else if (name2.size () == 3) { // try RGB
        PARSE1 (r1,0);
        PARSE1 (g1,1);
        PARSE1 (b1,2);
        r = (r1 - hex) / 15.0;
        g = (g1 - hex) / 15.0;
        b = (b1 - hex) / 15.0;
        return;
    }
#undef PARSE1


try_name:
    // try to look up the name in the Official(tm) CSS3/SVG color map
    static map<string,rgb> colormap;
    if (colormap.size () == 0) {
        // http://www.w3.org/TR/SVG/types.html#ColorKeywords
        colormap["aliceblue"] = rgb (240, 248, 255);
        colormap["antiquewhite"] = rgb (250, 235, 215);
        colormap["aqua"] = rgb (0, 255, 255);
        colormap["aquamarine"] = rgb (127, 255, 212);
        colormap["azure"] = rgb (240, 255, 255);
        colormap["beige"] = rgb (245, 245, 220);
        colormap["bisque"] = rgb (255, 228, 196);
        colormap["black"] = rgb (0, 0, 0);
        colormap["blanchedalmond"] = rgb (255, 235, 205);
        colormap["blue"] = rgb (0, 0, 255);
        colormap["blueviolet"] = rgb (138, 43, 226);
        colormap["brown"] = rgb (165, 42, 42);
        colormap["burlywood"] = rgb (222, 184, 135);
        colormap["cadetblue"] = rgb (95, 158, 160);
        colormap["chartreuse"] = rgb (127, 255, 0);
        colormap["chocolate"] = rgb (210, 105, 30);
        colormap["coral"] = rgb (255, 127, 80);
        colormap["cornflowerblue"] = rgb (100, 149, 237);
        colormap["cornsilk"] = rgb (255, 248, 220);
        colormap["crimson"] = rgb (220, 20, 60);
        colormap["cyan"] = rgb (0, 255, 255);
        colormap["darkblue"] = rgb (0, 0, 139);
        colormap["darkcyan"] = rgb (0, 139, 139);
        colormap["darkgoldenrod"] = rgb (184, 134, 11);
        colormap["darkgray"] = rgb (169, 169, 169);
        colormap["darkgreen"] = rgb (0, 100, 0);
        colormap["darkgrey"] = rgb (169, 169, 169);
        colormap["darkkhaki"] = rgb (189, 183, 107);
        colormap["darkmagenta"] = rgb (139, 0, 139);
        colormap["darkolivegreen"] = rgb (85, 107, 47);
        colormap["darkorange"] = rgb (255, 140, 0);
        colormap["darkorchid"] = rgb (153, 50, 204);
        colormap["darkred"] = rgb (139, 0, 0);
        colormap["darksalmon"] = rgb (233, 150, 122);
        colormap["darkseagreen"] = rgb (143, 188, 143);
        colormap["darkslateblue"] = rgb (72, 61, 139);
        colormap["darkslategray"] = rgb (47, 79, 79);
        colormap["darkslategrey"] = rgb (47, 79, 79);
        colormap["darkturquoise"] = rgb (0, 206, 209);
        colormap["darkviolet"] = rgb (148, 0, 211);
        colormap["deeppink"] = rgb (255, 20, 147);
        colormap["deepskyblue"] = rgb (0, 191, 255);
        colormap["dimgray"] = rgb (105, 105, 105);
        colormap["dimgrey"] = rgb (105, 105, 105);
        colormap["dodgerblue"] = rgb (30, 144, 255);
        colormap["firebrick"] = rgb (178, 34, 34);
        colormap["floralwhite"] = rgb (255, 250, 240);
        colormap["forestgreen"] = rgb (34, 139, 34);
        colormap["fuchsia"] = rgb (255, 0, 255);
        colormap["gainsboro"] = rgb (220, 220, 220);
        colormap["ghostwhite"] = rgb (248, 248, 255);
        colormap["gold"] = rgb (255, 215, 0);
        colormap["goldenrod"] = rgb (218, 165, 32);
        colormap["gray"] = rgb (128, 128, 128);
        colormap["grey"] = rgb (128, 128, 128);
        colormap["green"] = rgb (0, 128, 0);
        colormap["greenyellow"] = rgb (173, 255, 47);
        colormap["honeydew"] = rgb (240, 255, 240);
        colormap["hotpink"] = rgb (255, 105, 180);
        colormap["indianred"] = rgb (205, 92, 92);
        colormap["indigo"] = rgb (75, 0, 130);
        colormap["ivory"] = rgb (255, 255, 240);
        colormap["khaki"] = rgb (240, 230, 140);
        colormap["lavender"] = rgb (230, 230, 250);
        colormap["lavenderblush"] = rgb (255, 240, 245);
        colormap["lawngreen"] = rgb (124, 252, 0);
        colormap["lemonchiffon"] = rgb (255, 250, 205);
        colormap["lightblue"] = rgb (173, 216, 230);
        colormap["lightcoral"] = rgb (240, 128, 128);
        colormap["lightcyan"] = rgb (224, 255, 255);
        colormap["lightgoldenrodyellow"] = rgb (250, 250, 210);
        colormap["lightgray"] = rgb (211, 211, 211);
        colormap["lightgreen"] = rgb (144, 238, 144);
        colormap["lightgrey"] = rgb (211, 211, 211);
        colormap["lightpink"] = rgb (255, 182, 193);
        colormap["lightsalmon"] = rgb (255, 160, 122);
        colormap["lightseagreen"] = rgb (32, 178, 170);
        colormap["lightskyblue"] = rgb (135, 206, 250);
        colormap["lightslategray"] = rgb (119, 136, 153);
        colormap["lightslategrey"] = rgb (119, 136, 153);
        colormap["lightsteelblue"] = rgb (176, 196, 222);
        colormap["lightyellow"] = rgb (255, 255, 224);
        colormap["lime"] = rgb (0, 255, 0);
        colormap["limegreen"] = rgb (50, 205, 50);
        colormap["linen"] = rgb (250, 240, 230);
        colormap["magenta"] = rgb (255, 0, 255);
        colormap["maroon"] = rgb (128, 0, 0);
        colormap["mediumaquamarine"] = rgb (102, 205, 170);
        colormap["mediumblue"] = rgb (0, 0, 205);
        colormap["mediumorchid"] = rgb (186, 85, 211);
        colormap["mediumpurple"] = rgb (147, 112, 219);
        colormap["mediumseagreen"] = rgb (60, 179, 113);
        colormap["mediumslateblue"] = rgb (123, 104, 238);
        colormap["mediumspringgreen"] = rgb (0, 250, 154);
        colormap["mediumturquoise"] = rgb (72, 209, 204);
        colormap["mediumvioletred"] = rgb (199, 21, 133);
        colormap["midnightblue"] = rgb (25, 25, 112);
        colormap["mintcream"] = rgb (245, 255, 250);
        colormap["mistyrose"] = rgb (255, 228, 225);
        colormap["moccasin"] = rgb (255, 228, 181);
        colormap["navajowhite"] = rgb (255, 222, 173);
        colormap["navy"] = rgb (0, 0, 128);
        colormap["oldlace"] = rgb (253, 245, 230);
        colormap["olive"] = rgb (128, 128, 0);
        colormap["olivedrab"] = rgb (107, 142, 35);
        colormap["orange"] = rgb (255, 165, 0);
        colormap["orangered"] = rgb (255, 69, 0);
        colormap["orchid"] = rgb (218, 112, 214);
        colormap["palegoldenrod"] = rgb (238, 232, 170);
        colormap["palegreen"] = rgb (152, 251, 152);
        colormap["paleturquoise"] = rgb (175, 238, 238);
        colormap["palevioletred"] = rgb (219, 112, 147);
        colormap["papayawhip"] = rgb (255, 239, 213);
        colormap["peachpuff"] = rgb (255, 218, 185);
        colormap["peru"] = rgb (205, 133, 63);
        colormap["pink"] = rgb (255, 192, 203);
        colormap["plum"] = rgb (221, 160, 221);
        colormap["powderblue"] = rgb (176, 224, 230);
        colormap["purple"] = rgb (128, 0, 128);
        colormap["red"] = rgb (255, 0, 0);
        colormap["rose"] = rgb (200,150,200); // a graphite-special
        colormap["rosybrown"] = rgb (188, 143, 143);
        colormap["royalblue"] = rgb (65, 105, 225);
        colormap["saddlebrown"] = rgb (139, 69, 19);
        colormap["salmon"] = rgb (250, 128, 114);
        colormap["sandybrown"] = rgb (244, 164, 96);
        colormap["seagreen"] = rgb (46, 139, 87);
        colormap["seashell"] = rgb (255, 245, 238);
        colormap["sienna"] = rgb (160, 82, 45);
        colormap["silver"] = rgb (192, 192, 192);
        colormap["skyblue"] = rgb (135, 206, 235);
        colormap["slateblue"] = rgb (106, 90, 205);
        colormap["slategray"] = rgb (112, 128, 144);
        colormap["slategrey"] = rgb (112, 128, 144);
        colormap["snow"] = rgb (255, 250, 250);
        colormap["springgreen"] = rgb (0, 255, 127);
        colormap["steelblue"] = rgb (70, 130, 180);
        colormap["tan"] = rgb (210, 180, 140);
        colormap["teal"] = rgb (0, 128, 128);
        colormap["thistle"] = rgb (216, 191, 216);
        colormap["tomato"] = rgb (255, 99, 71);
        colormap["turquoise"] = rgb (64, 224, 208);
        colormap["violet"] = rgb (238, 130, 238);
        colormap["wheat"] = rgb (245, 222, 179);
        colormap["white"] = rgb (255, 255, 255);
        colormap["whitesmoke"] = rgb (245, 245, 245);
        colormap["yellow"] = rgb (255, 255, 0);
        colormap["yellowgreen"] = rgb (154, 205, 50);
    }

    map<string,rgb>::iterator it = colormap.find (name);
    if (it != colormap.end ()) {
        r = it->second.r;
        g = it->second.g;
        b = it->second.b;
        return;
    }

    // XXX: generate a random color
    r = (random () % 256) / 255.0;
    g = (random () % 256) / 255.0;
    b = (random () % 256) / 255.0;
}


// Heuristically compute some reasonably rounded minimum/maximum
// values and major tick lines for the vertical scale.  
//
// Algorithm based on Label.c / Paul Heckbert / "Graphics Gems",
// Academic Press, 1990
//
// EULA: The Graphics Gems code is copyright-protected. In other
// words, you cannot claim the text of the code as your own and resell
// it. Using the code is permitted in any program, product, or
// library, non-commercial or commercial. Giving credit is not
// required, though is a nice gesture.

float nicenum(float x, bool round_p)
{
    int expv;/* exponent of x */
    double f;/* fractional part of x */
    double nf;/* nice, rounded fraction */

    expv = floor(log10f(x));
    f = x/exp10f(expv);/* between 1 and 10 */
    if (round_p)
        if (f<1.5) nf = 1.;
        else if (f<3.) nf = 2.;
        else if (f<7.) nf = 5.;
        else nf = 10.;
    else
        if (f<=1.) nf = 1.;
        else if (f<=2.) nf = 2.;
        else if (f<=5.) nf = 5.;
        else nf = 10.;
    return nf*exp10f(expv);
}

vector<float> round_linear (float& ymin, float& ymax, unsigned nticks)
{
    vector<float> ticks;

    // make some space between min & max
    float epsilon = 0.5;
    if ((ymax - ymin) < epsilon) {
        ymin -= epsilon;
        ymax += epsilon;
    }

    if (nticks <= 1)
        nticks = 3;

    float range = nicenum(ymax-ymin, false);
    float d = nicenum(range/(nticks-1), true);
    ymin = floorf(ymin/d)*d;
    ymax = ceilf(ymax/d)*d;    

    for (float x = ymin; x <= ymax; x+= d)
        ticks.push_back(x);

    return ticks;
}



time_t nicetime(time_t x, bool round_p)
{
    static const time_t powers[] = {1, // seconds
                                    60, 60*5, 60*10, 60*30, 60*60, // minutes
                                    60*60*2, 60*60*4, 60*60*6, 60*60*12, 60*60*24, // hours
                                    60*60*24*7, 60*60*24*7*4,  60*60*24*7*52 }; // weeks
    unsigned npowers = sizeof(powers)/sizeof(powers[0]);
    time_t ex;
    for (int i=npowers-1; i>=0; i--) {
        ex = powers[i];
        if (ex <= x)
            break;
    }

    if (round_p)
        return ((x + ex - 1) / ex) * ex;
    else
        return (x / ex) * ex;
}



vector<time_t> round_time (time_t xmin, time_t xmax, unsigned nticks)
{
    vector<time_t> ticks;

    // make some space between min & max
    time_t epsilon = 1;
    if ((xmax - xmin) < epsilon) {
        xmin -= epsilon;
        xmax += epsilon;
    }

    if (nticks <= 1)
        nticks = 3;

    time_t range = nicetime(xmax-xmin, false);
    time_t d = nicetime(range/(nticks+1), true);
    xmin = ((xmin + d - 1)/ d) * d;
    xmax = ((xmax + d - 1)/ d) * d;

    for (time_t x = xmin; x < xmax; x += d)
        ticks.push_back(x);

    return ticks;
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
    vector<vector<timestamped_float> > all_results;
    string colorList;
    vector<string> colors;
    string bgcolor;
    double graphxlow, graphxhigh, graphylow, graphyhigh;
    vector<float> yticks;
    vector<time_t> xticks;

    string format = params["format"];
    if (format == "") {
        format = "png";
    }

    // SVG? etc?
    if (format != "png") {
        return mhd_notify_error (connection, -EINVAL);
    }

    vector <string> targets;
    time_t t_start, t_end, t_step;
    rc = pmgraphite_gather_data (connection, params, url, targets, t_start, t_end, t_step);
    if (rc) {
        return mhd_notify_error (connection, rc);
    }

    int width = atoi (params["width"].c_str ());
    if (width <= 0) {
        width = 640;
    }
    int height = atoi (params["height"].c_str ());
    if (height <= 0) {
        height = 480;
    }

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
    bgcolor = params["bgcolor"];
    if (bgcolor == "")
        // as per graphite render/glyph.py defaultGraphOptions
    {
        bgcolor="white";
    }
    notcairo_parse_color (bgcolor, r, g, b);
    cairo_save (cr);
    cairo_set_source_rgb (cr, r, g, b);
    cairo_rectangle (cr, 0.0, 0.0, width, height);
    cairo_fill (cr);
    cairo_restore (cr);

    // Gather up all the data.  We need several passes over it, so gather it into a vector<vector<>>.
    for (unsigned k = 0; k < targets.size (); k++) {
        if (exit_p) {
            break;
        }
        all_results.push_back (pmgraphite_fetch_series (connection, targets[k], t_start, t_end, t_step));
    }


    // Compute vertical bounds.
    float ymin;
    if (params["yMin"] != "") {
        ymin = atof (params["yMin"].c_str ());
    } else {
        ymin = nanf ("");
        for (unsigned i=0; i<all_results.size (); i++)
            for (unsigned j=0; j<all_results[i].size (); j++) {
                if (isnanf (all_results[i][j].what)) {
                    continue;
                }
                if (isnanf (ymin)) {
                    ymin = all_results[i][j].what;
                } else {
                    ymin = min (ymin, all_results[i][j].what);
                }
            }
    }
    float ymax;
    if (params["yMax"] != "") {
        ymax = atof (params["yMax"].c_str ());
    } else {
        ymax = nanf ("");
        for (unsigned i=0; i<all_results.size (); i++)
            for (unsigned j=0; j<all_results[i].size (); j++) {
                if (isnanf (all_results[i][j].what)) {
                    continue;
                }
                if (isnanf (ymax)) {
                    ymax = all_results[i][j].what;
                } else {
                    ymax = max (ymax, all_results[i][j].what);
                }
            }
    }

    // Any data to show?
    if (isnanf (ymin) || isnanf (ymax) || all_results.empty ()) {
        cairo_text_extents_t ext;
        string message = "no data in range";
        cairo_save (cr);
        cairo_select_font_face (cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size (cr, 32.0);
        cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
        cairo_text_extents (cr, message.c_str (), &ext);
        cairo_move_to (cr,
                       width/2.0 - (ext.width/2 + ext.x_bearing),
                       height/2.0 - (ext.height/2 + ext.y_bearing));
        cairo_show_text (cr, message.c_str ());
        cairo_restore (cr);
        goto render_done;
    }


    // What makes us tick?
    yticks = round_linear (ymin, ymax,
                           (unsigned)(0.3 * sqrt(height))); // flot heuristic
    xticks = round_time (t_start, t_end,
                         (unsigned)(0.25 * sqrt(width))); // fewer due to wide time axis labels


    if (verbosity)
        connstamp (clog, connection) << "Rendering " << all_results.size () << " metrics"
                                     << ", ymin=" << ymin << ", ymax=" << ymax << endl;

    // Because we're going for basic acceptable rendering only, for
    // purposes of graphite-builder previews, we hard-code a simple
    // layout scheme:
    graphxlow = width * 0.1;
    graphxhigh = width * 0.9;
    graphylow = height * 0.05;
    graphyhigh = height * 0.95;
    // ... though these numbers might be adjusted a bit by legend etc. rendering

    // As a mnemonic, double typed variables are used to track
    // coordinates in graphics space, and float (with nan) for pcp
    // metric space.

    // Fetch curve color list
    colorList = params["colorList"];
    if (colorList == "") {
        // as per graphite render/glyph.py defaultGraphOptions
        colorList = "blue,green,red,purple,brown,yellow,aqua,grey,magenta,pink,gold,mistyrose";
    }
    colors = generate_colorlist (split (colorList,','), targets);
    assert (colors.size () == targets.size ());

    // Draw the title
    if (params["title"] != "") {
        const string& title = params["title"];
        double r, g, b;
        double spacing = 15.0;
        double baseline = spacing;
        cairo_text_extents_t ext;

        // draw text
        cairo_save (cr);
        string fgcolor = params["fgcolor"];
        if (fgcolor == "") {
            fgcolor = "black";
        }
        notcairo_parse_color (fgcolor, r, g, b);
        cairo_set_source_rgb (cr, r, g, b);
        cairo_select_font_face (cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size (cr, spacing);

        cairo_text_extents (cr, title.c_str (), &ext);
        cairo_move_to (cr,
                       width/2.0 - (ext.width/2 + ext.x_bearing),
                       baseline + ext.height + ext.y_bearing);
        cairo_show_text (cr, title.c_str ());
        cairo_restore (cr);

        // allocate space for the graph
        baseline += spacing*1.2;

        if (graphylow < baseline) {
            graphylow = baseline;
        }
    }

    // Draw the legend
    if (params["hideLegend"] != "true" &&
        (params["hideLegend"] == "false" || targets.size () <= 10)) { // maximum number of legend entries
        double spacing = 10.0;
        double baseline = height - 8.0;
        double leftedge = 10.0;
        for (unsigned i=0; i<targets.size (); i++) {
            const string& name = targets[i];
            double r, g, b;

            // draw square swatch
            cairo_save (cr);
            notcairo_parse_color (colors[i], r, g, b);
            cairo_set_source_rgb (cr, r, g, b);
            cairo_rectangle (cr, leftedge+1.0, baseline+1.0, spacing-1.0, 0.0-spacing-1.0);
            cairo_fill (cr);
            cairo_restore (cr);

            // draw text
            cairo_save (cr);
            string fgcolor = params["fgcolor"];
            if (fgcolor == "") {
                fgcolor = "black";
            }
            notcairo_parse_color (fgcolor, r, g, b);
            cairo_set_source_rgb (cr, r, g, b);
            cairo_select_font_face (cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size (cr, spacing);
            cairo_move_to (cr, leftedge + spacing*1.5, baseline);
            cairo_show_text (cr, name.c_str ());
            cairo_restore (cr);

            // allocate space for next row up
            baseline -= spacing * 1.2;
            if (graphyhigh > baseline) {
                graphyhigh = baseline - spacing;
            }
            if (graphyhigh < height*0.5) { // forget it, too many
                break;
            }
        }
    }

    if (params["hideGrid"] != "true") {
        // Shrink the graph to make room for axis labels
        graphxlow = width * 0.10;
        graphxhigh = width * 0.95;
        graphyhigh -= 10.;

        double r, g, b;
        double line_width = 1.5;

        // Draw the grid
        cairo_save(cr);
        string majorGridLineColor = params["majorGridLineColor"];
        if (majorGridLineColor == "") majorGridLineColor = "pink"; // XXX:
        notcairo_parse_color (majorGridLineColor, r, g, b);
        cairo_set_source_rgb (cr, r, g, b);
        cairo_set_line_width (cr, line_width);

        // Y axis grid & labels
        for (unsigned i=0; i<yticks.size(); i++) {
            float thisy = yticks[i];
            float ydelta = (ymax - ymin);
            double rely = (double)ymax/ydelta - (double)thisy/ydelta;
            double y = graphylow + (graphyhigh-graphylow)*rely;

            cairo_move_to (cr, graphxlow, y);
            cairo_line_to (cr, graphxhigh, y);
            cairo_stroke (cr);

            stringstream label;
            label << yticks[i];
            string lstr = label.str();
            cairo_text_extents_t ext;
            cairo_save (cr);
            cairo_select_font_face (cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size (cr, 8.0);
            string fgcolor = params["fgcolor"];
            if (fgcolor == "") {
                fgcolor = "black";
            }
            notcairo_parse_color (fgcolor, r, g, b);
            cairo_set_source_rgb (cr, r, g, b);
            cairo_text_extents (cr, lstr.c_str (), &ext);
            cairo_move_to (cr,
                           graphxlow - (ext.width + ext.x_bearing) - line_width*3,
                           y - (ext.height/2 + ext.y_bearing));
            cairo_show_text (cr, lstr.c_str ());
            cairo_restore (cr);
        }

        // X axis grid & labels
        for (unsigned i=0; i<xticks.size(); i++) {
            float thisx = xticks[i];
            float xdelta = (t_end - t_start);
            double relx = (double)thisx/xdelta - (double)t_start/xdelta;
            double x = graphxlow + (graphxhigh-graphxlow)*relx;

            cairo_move_to (cr, x, graphylow);
            cairo_line_to (cr, x, graphyhigh);
            cairo_stroke (cr);

            // We use gmtime / strftime to make a compact rendering of
            // the (UTC) time_t.
            char timestr[100];
            struct tm *t = gmtime(& xticks[i]);
            if (t->tm_hour == 0 && t->tm_sec == 0)
                strftime (timestr, sizeof(timestr), "%Y-%m-%d", t);
            else
                strftime (timestr, sizeof(timestr), "%Y-%m-%d %H:%M", t);

            cairo_text_extents_t ext;
            cairo_save (cr);
            cairo_select_font_face (cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size (cr, 8.0);
            string fgcolor = params["fgcolor"];
            if (fgcolor == "") {
                fgcolor = "black";
            }
            notcairo_parse_color (fgcolor, r, g, b);
            cairo_set_source_rgb (cr, r, g, b);
            cairo_text_extents (cr, timestr, &ext);
            cairo_move_to (cr,
                           x - (ext.width/2 + ext.x_bearing),
                           graphyhigh + (ext.height + ext.y_bearing) + 10);
            cairo_show_text (cr, timestr);
            cairo_restore (cr);
        }
        cairo_restore(cr);
        
        // Draw the frame (on top of the funky pink grid)
        cairo_save (cr);
        string fgcolor = params["fgcolor"];
        if (fgcolor == "") {
            fgcolor = "black";
        }
        notcairo_parse_color (fgcolor, r, g, b);
        cairo_set_source_rgb (cr, r, g, b);
        cairo_set_line_width (cr, line_width*1.5);
        cairo_rectangle (cr, graphxlow, graphylow, (graphxhigh-graphxlow), (graphyhigh-graphylow));
        cairo_stroke (cr);
        cairo_restore (cr);
    }


    // Draw the curves
    for (unsigned i=0; i<all_results.size (); i++) {
        const vector<timestamped_float>& f = all_results[i];

        double r,g,b;
        cairo_save (cr);
        notcairo_parse_color (colors[i], r, g, b);
        cairo_set_source_rgb (cr, r, g, b);

        string lineWidth = params["lineWidth"];
        if (lineWidth == "") lineWidth = "1.2";
        double line_width = atof(lineWidth.c_str());
        cairo_set_line_width (cr, line_width);
        cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);

#if 0
        // transform cairo rendering coordinate system, so we can feed
        // it raw data timestamp/value pairs and get them drawn onto
        // the designated graph[xy]{low,high} region.
        cairo_translate (cr, (double)graphxlow, (double)graphylow);
        cairo_scale (cr, (double)graphxhigh-graphxlow, (double)graphyhigh-graphylow);
        cairo_scale (cr, 1./((double)t_end-(double)t_start), 1./((double)ymax-(double)ymin));
        cairo_translate (cr, -(double)t_start, -(double)ymin);

        // XXX: unfortunately, the order of operations or something else is awry with the above.
#endif

        double lastx = nan ("");
        double lasty = nan ("");
        for (unsigned j=0; j<f.size (); j++) {
            float thisx = f[j].when.tv_sec + f[j].when.tv_usec/1000000.0;
            float thisy = f[j].what;

            // clog << "(" << lastx << "," << lasty << ")";

            if (isnanf (thisy)) {
                // This data slot is missing, so put a circle at the previous end, if
                // possible, to indicate the discontinuity
                if (! isnan (lastx) && ! isnan (lasty)) {
                    cairo_move_to (cr, lastx, lasty);
                    cairo_arc (cr, lastx, lasty, line_width*0.5, 0., 2*M_PI);
                    cairo_stroke (cr);
                }
                continue;
            }

            float xdelta = (t_end - t_start);
            float ydelta = (ymax - ymin);
            double relx = (double)thisx/xdelta - (double)t_start/xdelta;
            double rely = (double)ymax/ydelta - (double)thisy/ydelta;
            double x = graphxlow + (graphxhigh-graphxlow)*relx; // scaled into graphics grid area
            double y = graphylow + (graphyhigh-graphylow)*rely;

#if 0 // if only the cairo transform widget worked above
            double x = thisx;
            double y = thisy;
#endif
            // clog << "-(" << x << "," << y << ") ";

            cairo_move_to (cr, x, y);
            if (! isnan (lastx) && ! isnan (lasty)) {
                // draw it as a line
                cairo_line_to (cr, lastx, lasty);
            } else {
                // draw it as a circle
                cairo_arc (cr, x, y, line_width*0.5, 0., 2*M_PI);
            }
            cairo_stroke (cr);

            lastx = x;
            lasty = y;
        }

        // clog << endl;
        cairo_restore (cr);
    }


    // Need to get the data out of cairo and into microhttpd.  We use
    // a c++ string as a temporary, which can carry binary data.
render_done:
    cst = cairo_surface_write_to_png_stream (sfc, & notcairo_write_to_string, (void *) & imgbuf);
    if (cst != CAIRO_STATUS_SUCCESS || cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
        rc = -EIO;
        goto out3;
    }

    resp = MHD_create_response_from_buffer (imgbuf.size (), (void*) imgbuf.c_str (),
                                            MHD_RESPMEM_MUST_COPY);
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
    if (rc) {
        return mhd_notify_error (connection, rc);
    } else {
        return MHD_YES;
    }
}


#endif /* HAVE_CAIRO */

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
#ifdef HAVE_CAIRO
    else if (url2 == "render") {
        return pmgraphite_respond_render_gfx (connection, params, url);
    }
#endif

    return mhd_notify_error (connection, -EINVAL);
}
