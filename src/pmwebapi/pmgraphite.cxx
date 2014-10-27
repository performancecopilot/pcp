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

using namespace std;

extern "C" {
#include <ctype.h>
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
string pmgraphite_metric_encode (const string & foo)
{
    string output;
    static const char hex[] = "0123456789ABCDEF";

    assert (foo.size () > 0);
    for (unsigned i = 0; i < foo.size (); i++) {
	char c = foo[i];
	if (isalnum (c) || (c == '_'))
	    output += c;
	else
	    output += "%" + hex[(c >> 4) & 15] + hex[(c >> 0) & 15];
    }
    return output;
}

// NB: decoding failure is possible (could arise from mischevious URLs
// being fed to us) and is signalled with an empty return string.
string pmgraphite_metric_decode (const string & foo)
{
    string output;
    static const char hex[] = "0123456789ABCDEF";
    for (unsigned i = 0; i < foo.size (); i++) {
	char c = foo[i];
	if (c == '%') {
	    if (i + 2 >= foo.size ())
		return "";
	    const char *p = lower_bound (hex, hex + 16, foo[i + 1]);
	    if (*p != foo[i + 1])
		return "";
	    const char *q = lower_bound (hex, hex + 16, foo[i + 2]);
	    if (*q != foo[i + 2])
		return "";
	    output += (char) (((p - hex) << 4) | (q - hex));
	} else
	    output += c;
    }
    return output;

}


// Heavy lifter.  Parse graphite "target" name into archive
// file/directory, metric name, and (if appropriate) instance within
// metric indom; fetch all the data values interpolated between given
// end points, and assemble them into single series-of-numbers for
// rendering.
//
// A lot can go wrong, but is signalled only with a stderr message and
// an empty vector.  (As a matter of security, we prefer not to give too
// much information to a remote web user about the exact error.)
vector < float >pmgraphite_fetch_series (struct MHD_Connection *connection, const string & target, time_t t_start, time_t t_end, time_t t_step)	// time bounds
{
    vector < float >empty_output;
    vector < float >output;

    // XXX: in future, parse graphite functions-of-metrics
    // XXX: in future, parse target-component wildcards 
    // XXX: in future, cache the pmid/pmdesc/inst# -> pcp-context

    vector < string > target_tok = split (target, '.');
    if (target_tok.size () < 2) {
	connstamp (cerr, connection) << "not enough target components" << endl;
	return empty_output;
    }
    for (unsigned i = 0; i < target_tok.size (); i++)
	if (target_tok[i] == "") {
	    connstamp (cerr, connection) << "empty target components" << endl;
	    return empty_output;
	}
    // Extract the archive file/directory name
    string archive;
    string archive_part = pmgraphite_metric_decode (target_tok[0]);
    if (archive_part == "") {
	connstamp (cerr,
		   connection) << "undecodeable archive-path " << target_tok[0] << endl;
	return empty_output;
    }
    if (__pmAbsolutePath ((char *) archive_part.c_str ()))	// accept absolute paths too
	archive = archive_part;
    else
	archive = archivesdir + (char) __pmPathSeparator () + archive_part;

    if (cursed_path_p (archivesdir, archive)) {
	connstamp (cerr, connection) << "invalid archive path " << archive << endl;
	return empty_output;
    }
    // Open the bad boy.
    // XXX: if it's a directory, redirect to the newest entry
    int pmc = pmNewContext (PM_CONTEXT_ARCHIVE, archive.c_str ());
    if (pmc < 0) {
	// error already noted
	return empty_output;
    }
    if (verbosity > 2)
	connstamp (clog, connection) << "opened archive " << archive << endl;

    // NB: past this point, exit via 'goto out;' to release pmc

    // We need to decide whether the next dotted components represent
    // a metric name, or whether there is an instance name squished at
    // the end.
    string metric_name;
    for (unsigned i = 1; i < target_tok.size () - 1; i++) {
	const string & piece = target_tok[i];
	if (i > 1)
	    metric_name += '.';
	metric_name += piece;
    }
    string last_component = target_tok[target_tok.size () - 1];

    pmID pmid;			// as yet unknown
    pmDesc pmd;

    {
	char *namelist[1];
	pmID pmidlist[1];
	namelist[0] = (char *) metric_name.c_str ();
	int sts = pmLookupName (1, namelist, pmidlist);

	if (sts == 1) {		// found ... last name must be instance domain name
	    pmid = pmidlist[0];
	    sts = pmLookupDesc (pmid, &pmd);
	    if (sts != 0) {
		connstamp (clog,
			   connection) << "cannot find metric descriptor " << metric_name
		    << endl;
		goto out;
	    }
	    // check that there is an instance domain, in order to use that last component 
	    if (pmd.indom == PM_INDOM_NULL) {
		connstamp (clog,
			   connection) << "metric " << metric_name <<
		    " lacks expected indom " << last_component << endl;
		goto out;

	    }
	    // look up that instance name
	    string instance_name = last_component;
	    int inst = pmLookupInDomArchive (pmd.indom,
					     (char *) instance_name.c_str ());	// XXX: why not pmLookupInDom?
	    if (inst < 0) {
		connstamp (clog,
			   connection) << "metric " << metric_name <<
		    " lacks recognized indom " << last_component << endl;
		goto out;
	    }
	    // activate only that instance name in the profile
	    sts = pmDelProfile (pmd.indom, 0, NULL);
	    sts |= pmAddProfile (pmd.indom, 1, &inst);
	    if (sts != 0) {
		connstamp (clog,
			   connection) << "metric " << metric_name <<
		    " cannot set unitary instance profile " << inst << endl;
		goto out;
	    }
	} else {		// not found ... ok, try again with that last component
	    metric_name = metric_name + '.' + last_component;
	    namelist[0] = (char *) metric_name.c_str ();
	    int sts = pmLookupName (1, namelist, pmidlist);
	    if (sts != 1) {	// still not found .. give up
		connstamp (clog,
			   connection) << "cannot find metric name " << metric_name <<
		    endl;
		goto out;
	    }

	    pmid = pmidlist[0];
	    sts = pmLookupDesc (pmid, &pmd);
	    if (sts != 0) {
		connstamp (clog,
			   connection) << "cannot find metric descriptor " << metric_name
		    << endl;
		goto out;
	    }
	    // check that there is no instance domain
	    if (pmd.indom != PM_INDOM_NULL) {
		connstamp (clog,
			   connection) << "metric " << metric_name <<
		    " has unexpected indom " << pmd.indom << endl;
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
	    ;
	default:
	    connstamp (clog,
		       connection) << "metric " << metric_name << " has unsupported type"
		<< endl;
	    goto out;
    }

    // Time to 



  out:
    pmDestroyContext (pmc);

    return output;
}


/* ------------------------------------------------------------------------ */

int pmgraphite_respond (struct MHD_Connection *connection, const vector < string > &url)
{
    int rc;

    string url1 = (url.size () >= 2) ? url[1] : "";
    assert (url1 == "graphite");
    string url2 = (url.size () >= 3) ? url[2] : "";
    string url3 = (url.size () >= 4) ? url[3] : "";

    /* XXX: emit CORS header, e.g.
       https://developer.mozilla.org/en-US/docs/HTTP/Access_control_CORS */

    /* -------------------------------------------------------------------- */
    if (url2 == "metrics" && url2 == "find") {
    }


    rc = -EINVAL;

  out:
    return mhd_notify_error (connection, rc);
}
