/*
 * Copyright (c) 2013 Red Hat.
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


#include "pmmgr.h"
#include "impl.h"

#include <sys/stat.h>
#include <cassert>
#include <fstream>
#include <iostream>

extern "C" {
#include <glob.h>
#include <sys/wait.h>
}


using namespace std;


// ------------------------------------------------------------------------


#ifdef IS_MINGW /* ie. not posix */
#error "posix required"
so is #error
// since we use fork / glob / etc.
#endif


// ------------------------------------------------------------------------


pmmgr_configurable::pmmgr_configurable(const string& dir):
  config_directory(dir)
{
}


vector<string>
pmmgr_configurable::get_config_multi(const string& file)
{
  vector<string> lines;

  string complete_filename = config_directory + (char)__pmPathSeparator() + file;
  ifstream f (complete_filename.c_str());
  while (f.good()) {
    string line;
    getline(f, line);
    if (! f.good())
      break;
    if (line != "")
      lines.push_back(line);
  }

  return lines;
}


bool
pmmgr_configurable::get_config_exists(const string& file)
{
  string complete_filename = config_directory + (char)__pmPathSeparator() + file;
  ifstream f (complete_filename.c_str());
  return (f.good());
}


string
pmmgr_configurable::get_config_single(const string& file)
{
  vector<string> lines = get_config_multi (file);
  if (lines.size() == 1)
    return lines[0];
  else
    return "";
}


// ------------------------------------------------------------------------


pmmgr_exception::pmmgr_exception(int rc): 
  runtime_error (pmErrStr(rc)),
  pmerror(rc)
{
}


pmmgr_exception::pmmgr_exception(int rc, const std::string& m):
  runtime_error (m),
  pmerror(rc) 
{
}


std::ostream&
operator << (std::ostream& o, const pmmgr_exception& e)
{
  o << "error: " << e.what() << " (code " << e.pmerror << ")";
  return o;
}


// ------------------------------------------------------------------------


pmmgr_hostid
pmmgr_job_spec::compute_hostid (const pcp_context_spec& ctx)
{
  int pmc = pmNewContext (PM_CONTEXT_HOST, ctx.c_str());
  if (pmc < 0)
    return "";

  // fetch all hostid metrics in sequence; they've already been
  // fed to pmParseMetricSpec().
  vector<string> hostid_fields;
  for (unsigned i=0; i<hostid_metrics.size(); i++)
    {
      pmMetricSpec* pms = hostid_metrics[i];

      pmID pmid;
      int rc = pmLookupName (1, & pms->metric, &pmid);
      if (rc < 0)
        continue;

      pmDesc desc;
      rc = pmLookupDesc (pmid, & desc);
      if (rc < 0)
        continue;

      if (desc.type != PM_TYPE_STRING)
        continue;

      if ((desc.indom != PM_INDOM_NULL) && pms->ninst > 0)
        {
          // reset the indom to include all elements
          rc = pmDelProfile(desc.indom, 0, (int *)0);
          if (rc < 0)
            continue;

          int *inums = (int *) malloc (pms->ninst * sizeof(int));
          if (inums == NULL)
            continue;
          // NB: after this point, 'continue' must also free(inums);

          // map the instance names to instance numbers
          unsigned numinums_used = 0;
          for (int j=0; j<pms->ninst; j++)
            {
              int inum = pmLookupInDom (desc.indom, pms->inst[j]);
              if (inum < 0)
                continue;
              inums[numinums_used++] = inum;
            }

          // add the selected instances to the profile
          rc = pmAddProfile (desc.indom, numinums_used, inums);
          free (inums);
          if (rc < 0)
            continue;
        }

      // fetch the values
      pmResult *r;
      rc = pmFetch (1, &pmid, &r);
      if (rc < 0)
        continue;
      // NB: after this point, 'continue' must also pmFreeResult(r)

      // only vset[0] will be set, for csb->pmid
      if (r->vset[0]->numval > 0)
        // XXX: sort the vlist[j] over indom numbers?
        for (int j=0; j<r->vset[0]->numval; j++) // iterate over instances
          {
            // fetch the string value
            pmAtomValue av;
            rc = pmExtractValue(r->vset[0]->valfmt,
                                & r->vset[0]->vlist[j],
                                PM_TYPE_STRING, & av, PM_TYPE_STRING);
            if (rc < 0)
              continue;

            // at last!  we have a string we can accumulate
            hostid_fields.push_back (av.cp);
            free (av.cp);
          }

      (void) pmFreeResult (r);
    }

  (void) pmDestroyContext (pmc);


  // Sanitize the host-id metric values into a single string that is
  // suitable for posix-portable-filenames, and not too ugly for
  // someone to look at or type in.
  //
  // http://www.opengroup.org/onlinepubs/007904975/basedefs/xbd_chap03.html
  string sanitized;
  for (unsigned i=0; i<hostid_fields.size(); i++)
    {
      const string& f = hostid_fields[i];
      if (f == "") continue;
      if (sanitized != "") sanitized += "-"; // separate fields
      for (unsigned j=0; j<f.length(); j++)
        {
          char c = f[j];
          if (isalnum(c))
            sanitized += c;
          else if (c== '-' || c == '.' || c == '_')
            sanitized += c;
          else
            // drop other non-portable characters NB: this can mean
            // unintentional duplication in IDs, which a user can work
            // around by configuring additional hostid metrics.
            ;
        }
    }

    return pmmgr_hostid (sanitized);
}


pmmgr_job_spec::pmmgr_job_spec(const std::string& config_directory):
  pmmgr_configurable(config_directory)
{
  vector<string> hostid_specs = get_config_multi("hostid-metrics");
  if (hostid_specs.size() == 0)
    hostid_specs.push_back(string("pmcd.hostname"));

  for (unsigned i=0; i<hostid_specs.size(); i++)
    {
      const char* spec = hostid_specs[i].c_str();
      pmMetricSpec* pms;
      char *errmsg;
      char dummy_host[] = "";
      int rc = pmParseMetricSpec (spec,
                                  0, dummy_host, /* both ignored */
                                  & pms, & errmsg);
      if (rc < 0) {
        string errmsg2 = errmsg;
        free (errmsg);
        // we can't parse the metric expression: FAIL
        throw pmmgr_exception (rc, errmsg2);
      }

      hostid_metrics.push_back (pms);
    }

  // check for compulsory configuration
  vector<string> target_host = get_config_multi("target-host");
  vector<string> target_discovery = get_config_multi("target-discovery");
  if (target_host.size() == 0 &&
      target_discovery.size() == 0)
    throw pmmgr_exception (-ENOENT, string("empty target-* files in ") + config_directory);

  // We don't actually have to do any other configuration parsing at
  // this time.  Let's do it during poll(), which makes us more
  // responsive to run-time changes anyway.
}


pmmgr_job_spec::~pmmgr_job_spec()
{
  // free the pmMetricSpec's allocated in the ctor
  for (unsigned i=0; i<hostid_metrics.size(); i++)  
    free(hostid_metrics[i]); // aka pmFreeMetricSpec

  // kill all our daemons created during poll()
  for (map<pmmgr_hostid,pcp_context_spec>::iterator it = known_targets.begin();
       it != known_targets.end();
       it++)
    note_dead_hostid (it->first);
}


// ------------------------------------------------------------------------


void
pmmgr_job_spec::poll()
{
  // phase 1: run all discovery/probing functions to collect context-spec's
  set<pcp_context_spec> new_specs;

  vector<string> target_hosts = get_config_multi("target-host");
  for (unsigned i=0; i<target_hosts.size(); i++)
    new_specs.insert(target_hosts[i]);

  vector<string> target_discovery = get_config_multi("target-discovery");
  for (unsigned i=0; i<target_discovery.size(); i++)
    {
#if HAVE_DISCOVERY_API
      char **urls;
      int rc = pmDiscoverServices (& urls, "pmcd", target_discovery[i].c_str());
      if (rc < 0)
        continue;
      for (char **url = urls; *url != NULL; url ++)
        new_specs.insert(string(*url));
      free ((void*) urls);
#endif
    }

  // phase 2: move previously-identified targets over, so we can tell who
  // has come or gone
  const map<pmmgr_hostid,pcp_context_spec> old_known_targets = known_targets;
  known_targets.clear();

  // phase 3: map the context-specs to hostids to find new hosts
  for (set<pcp_context_spec>::iterator it = new_specs.begin();
       it != new_specs.end();
       it++)
    {
      pmmgr_hostid hostid = compute_hostid (*it);
      if (hostid != "") // verified existence/liveness
        known_targets[hostid] = *it;
      // NB: for hostid's with multiple specs, this logic will pick an
      // *arbitrary* one.  Perhaps we want to tie-break deterministically.
    }

  // phase 4a: compare old_known_targets vs. known_targets: look for any recently died
  for (map<pmmgr_hostid,pcp_context_spec>::const_iterator it = old_known_targets.begin();
       it != old_known_targets.end();
       it++)
    {
      const pmmgr_hostid& hostid = it->first;
      if (known_targets.find(hostid) == known_targets.end())
        note_dead_hostid (hostid);
    }

  // phase 4b: compare new known_targets & old_known_targets: look for recently born
  for (map<pmmgr_hostid,pcp_context_spec>::const_iterator it = known_targets.begin();
       it != known_targets.end();
       it++)
    {
      const pmmgr_hostid& hostid = it->first;
      if (old_known_targets.find(hostid) == old_known_targets.end())
        note_new_hostid (hostid, known_targets[hostid]);
    }

  // phase 5: poll all the live daemons
  // NB: there is a parallelism opportunity, as running many pmlogconf/etc.'s in series
  // is a possible bottleneck.
  for (multimap<pmmgr_hostid,pmmgr_daemon*>::iterator it = daemons.begin();
       it != daemons.end();
       it ++)
    it->second->poll();
}


// ------------------------------------------------------------------------


void
pmmgr_job_spec::note_new_hostid(const pmmgr_hostid& hid, const pcp_context_spec& spec)
{
  if (pmDebug & DBG_TRACE_APPL0)
    cerr << pmProgname << " " << config_directory
         << ": new hostid " << hid << " at " << string(spec) << endl;

  if (get_config_exists("pmlogger"))
    daemons.insert(make_pair(hid, new pmmgr_pmlogger_daemon(config_directory, hid, spec)));

  if (get_config_exists("pmie"))
    daemons.insert(make_pair(hid, new pmmgr_pmie_daemon(config_directory, hid, spec)));
}


void
pmmgr_job_spec::note_dead_hostid(const pmmgr_hostid& hid)
{
  if (pmDebug & DBG_TRACE_APPL0)
    cerr << pmProgname << " " << config_directory << ": dead hostid " << hid << endl;

  pair<multimap<pmmgr_hostid,pmmgr_daemon*>::iterator,
       multimap<pmmgr_hostid,pmmgr_daemon*>::iterator> range =
    daemons.equal_range(hid);

  for (multimap<pmmgr_hostid,pmmgr_daemon*>::iterator it = range.first;
       it != range.second;
       it ++)
    delete (it->second);

  daemons.erase(range.first, range.second);
}


// ------------------------------------------------------------------------


pmmgr_daemon::pmmgr_daemon(const std::string& config_directory,
                           const pmmgr_hostid& hostid, 
                           const pcp_context_spec& spec):
  pmmgr_configurable(config_directory),
  hostid(hostid),
  spec(spec),
  pid(0)
{
}


pmmgr_pmlogger_daemon::pmmgr_pmlogger_daemon(const std::string& config_directory,
                                             const pmmgr_hostid& hostid, 
                                             const pcp_context_spec& spec):
  pmmgr_daemon(config_directory, hostid, spec)
{
}


pmmgr_pmie_daemon::pmmgr_pmie_daemon(const std::string& config_directory,
                                         const pmmgr_hostid& hostid, 
                                         const pcp_context_spec& spec):
  pmmgr_daemon(config_directory, hostid, spec)
{
}


pmmgr_daemon::~pmmgr_daemon()
{
  if (pid != 0)
    {
      (void) kill ((pid_t) pid, SIGTERM);
      if (pmDebug & DBG_TRACE_APPL0)
        cerr << "daemon pid " << pid << " killed" << endl;
    }
}


void pmmgr_daemon::poll()
{
  if (pid != 0) // test if it's still alive
    {
      int rc = kill ((pid_t) pid, 0);
      if (rc < 0) 
        {
          if (pmDebug & DBG_TRACE_APPL0)
            cerr << "daemon pid " << pid << " found dead" << endl;
          pid = 0;
          // we will try again immediately
        }
    }

  if (pid == 0) // needs a restart
    {
      string commandline = daemon_command_line();
      if (pmDebug & DBG_TRACE_APPL0)
        cerr << "spawning sh -c " << commandline << endl;
      pid = fork();
      if (pid == 0) // child process
        {
          int rc = execl ("/bin/sh", "sh", "-c", commandline.c_str(), NULL);
          cerr << "failed to execl sh -c " << commandline << " rc=" << rc << endl;
          _exit (1);
          // parent will try again at next poll
        }
      else if (pid < 0) // failed fork
        {
          cerr << "failed to fork for sh -c " << commandline << endl;
          pid = 0;
          // we will try again at next poll
        }
      else // congratulations!  we're a parent
        {
          if (pmDebug & DBG_TRACE_APPL0)
            cerr << "daemon pid " << pid << " started: " << commandline << endl;
        }
    }
}


std::string                                             
pmmgr_pmlogger_daemon::daemon_command_line()
{
  string default_log_dir =
    string(pmGetConfig("PCP_LOG_DIR")) + (char)__pmPathSeparator() + "pmmgr";
  string log_dir = get_config_single ("log-directory");
  if (log_dir == "") log_dir = default_log_dir;
  (void) mkdir2 (log_dir.c_str(), 0777); // implicitly consults umask(2)

  string host_log_dir = log_dir + (char)__pmPathSeparator() + hostid;
  (void) mkdir2 (host_log_dir.c_str(), 0777);
  // (errors creating actual files under host_log_dir will be noted shortly)

  string pmlogger_options = 
        string(pmGetConfig("PCP_BIN_DIR")) + (char)__pmPathSeparator() + "pmlogger";
  pmlogger_options += " " + get_config_single ("pmlogger");

  // run pmlogconf if requested
  if (get_config_exists("pmlogconf"))
    {
      string pmlogconf_options = get_config_single ("pmlogconf");
      string pmlogconf_output_file = host_log_dir + (char)__pmPathSeparator() + "config.pmlogger";

      (void) unlink (pmlogconf_output_file.c_str());

      string pmlogconf_command = 
        string(pmGetConfig("PCP_BINADM_DIR")) + (char)__pmPathSeparator() + "pmlogconf";
      string pmlogconf = 
        pmlogconf_command + " -c -r -h " + string(spec) + " " + pmlogconf_options + pmlogconf_output_file;
      if (pmDebug & DBG_TRACE_APPL0)
        cerr << "running " << pmlogconf << endl;
      int rc = system(pmlogconf.c_str());
      if (rc != 0) 
        cerr << "system(" << pmlogconf << ") failed: rc=" << rc << endl;

      pmlogger_options += " -c " + pmlogconf_output_file;
    }

  // collect -h direction
  pmlogger_options += " -h " + string(spec);

  // collect subsidiary pmlogger diagnostics
  pmlogger_options += " -l " + host_log_dir + (char)__pmPathSeparator() + "pmlogger.log";

  // do log merging
  if (get_config_exists ("pmlogmerge"))
    {
      string pmlogextract_options = 
        string(pmGetConfig("PCP_BIN_DIR")) + (char)__pmPathSeparator() + "pmlogextract";

      string retention = get_config_single ("pmlogmerge-retain");
      if (retention == "") retention = "14days";
      pmlogextract_options += " -S -" + retention;

      // Arrange our new pmlogger to kill itself after the given
      // period, to give us a chance to rerun.
      string period = get_config_single ("pmlogmerge");
      if (period == "") period = "24hours";
      pmlogger_options += " -s " + period;
      
      // Find prior archives by globbing for *.index files, 
      // just like pmlogger_merge does
      vector<string> old_archives;
      glob_t the_blob;
      string glob_pattern = host_log_dir + (char)__pmPathSeparator() + "*.index";
      int rc = glob (glob_pattern.c_str(), GLOB_NOESCAPE, NULL, & the_blob);
      if (rc == 0)
        {
          for (unsigned i=0; i<the_blob.gl_pathc; i++)
            {
              string index_name = the_blob.gl_pathv[i];
              string base_name = index_name.substr(0,index_name.length()-6); // trim .index
              old_archives.push_back (base_name);
            }
          globfree (& the_blob);
        }

      string timestr = "merged-archive";
      time_t now2 = time(NULL);
      struct tm *now = gmtime(& now2);
      if (now != NULL)
        {
          char timestr2[100];
          int rc = strftime(timestr2, sizeof(timestr2), "-%Y%m%d.%H%M%S", now);
          if (rc > 0)
            timestr += timestr2;
        }
      string merged_archive_name = host_log_dir + (char)__pmPathSeparator() + timestr;

      if (old_archives.size() > 1) // 1 or 0 are not worth merging!
        {
          // assemble final bits of pmlogextract command line: the inputs and the output
          for (unsigned i=0; i<old_archives.size(); i++)
            pmlogextract_options += " " + old_archives[i];

          pmlogextract_options += " " + merged_archive_name;

          if (pmDebug & DBG_TRACE_APPL0)
            cerr << "running " << pmlogextract_options << endl;

          rc = system(pmlogextract_options.c_str());
          if (rc != 0) 
            // will try again later
            cerr << "system(" << pmlogextract_options << ") failed: rc=" << rc << endl;
          else
            {
              // zap the previous archive files 
              for (unsigned i=0; i<old_archives.size(); i++)
                {
                  string base_name = old_archives[i];
                  string cleanup_cmd = string("/bin/rm -f")
                    + " " + base_name + ".[0-9]*"
                    + " " + base_name + ".index" + 
                    + " " + base_name + ".meta";

                  if (pmDebug & DBG_TRACE_APPL0)
                    cerr << "running " << cleanup_cmd << endl;

                  rc = system(cleanup_cmd.c_str());
                  if (rc != 0) 
                    cerr << "system(" << cleanup_cmd << ") failed: rc=" << rc << endl;
                }
            }
        }
    }
  
  // synthesize a logfile name similarly as pmlogger_check, but add %S (seconds)
  // to reduce likelihood of conflict with a short poll interval
  string timestr = "archive";
  time_t now2 = time(NULL);
  struct tm *now = gmtime(& now2);
  if (now != NULL)
    {
      char timestr2[100];
      int rc = strftime(timestr2, sizeof(timestr2), "-%Y%m%d.%H%M%S", now);
      if (rc > 0)
        timestr += timestr2;
    }
  string pmlogger_logfile = host_log_dir + (char)__pmPathSeparator() + timestr;

  // last argument
  pmlogger_options += " " + pmlogger_logfile; 

  return pmlogger_options;
}


std::string                                             
pmmgr_pmie_daemon::daemon_command_line()
{
  string default_log_dir =
    string(pmGetConfig("PCP_LOG_DIR")) + (char)__pmPathSeparator() + "pmmgr";
  string log_dir = get_config_single ("log-directory");
  if (log_dir == "") log_dir = default_log_dir;
  (void) mkdir2 (log_dir.c_str(), 0777); // implicitly consults umask(2)

  string host_log_dir = log_dir + (char)__pmPathSeparator() + hostid;
  (void) mkdir2 (host_log_dir.c_str(), 0777);
  // (errors creating actual files under host_log_dir will be noted shortly)

  string pmie_options = 
        string(pmGetConfig("PCP_BIN_DIR")) + (char)__pmPathSeparator() + "pmie";
  pmie_options += " " + get_config_single ("pmie");

  // run pmieconf if requested
  if (get_config_exists ("pmieconf"))
    {
      string pmieconf_options = get_config_single ("pmieconf");
      string pmieconf_output_file = host_log_dir + (char)__pmPathSeparator() + "config.pmie";
      string pmieconf_command = 
        string(pmGetConfig("PCP_BIN_DIR")) + (char)__pmPathSeparator() + "pmieconf";

      // NB: pmieconf doesn't take a host name as an argument, unlike pmlogconf
      string pmieconf = 
        pmieconf_command + " -F -c " + pmieconf_options + " -f " + pmieconf_output_file;
      if (pmDebug & DBG_TRACE_APPL0)
        cerr << "running " << pmieconf << endl;
      int rc = system(pmieconf.c_str());
      if (rc != 0) 
        cerr << "system(" << pmieconf << ") failed: rc=" << rc << endl;

      pmie_options += "-c " + pmieconf_output_file;
    }

  // collect -h direction
  pmie_options += " -h " + string(spec);

  // collect -f, to get it to run in the foreground, avoid setuid
  pmie_options += " -f";
 
  // collect subsidiary pmlogger diagnostics
  pmie_options += " -l " + host_log_dir + (char)__pmPathSeparator() + "pmie.log";

  return pmie_options;
}



// ------------------------------------------------------------------------


int quit;

extern "C"
void handle_interrupt (int sig)
{
  quit ++;
  if (quit > 2)
    {
      char msg[] = "Too many interrupts received, exiting.\n";
      int rc = write (2, msg, sizeof(msg)-1);
      if (rc) {/* Do nothing; we don't care if our last gasp went out. */ ;}
      // XXX: send a suicide signal to the process group?
      _exit (1);
    }
}


void setup_signals()
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_interrupt;
  sigemptyset (&sa.sa_mask);
  sigaddset (&sa.sa_mask, SIGHUP);
  sigaddset (&sa.sa_mask, SIGPIPE);
  sigaddset (&sa.sa_mask, SIGINT);
  sigaddset (&sa.sa_mask, SIGTERM);
  sigaddset (&sa.sa_mask, SIGXFSZ);
  sigaddset (&sa.sa_mask, SIGXCPU);
  sa.sa_flags = SA_RESTART;

  sigaction (SIGHUP, &sa, NULL);
  sigaction (SIGPIPE, &sa, NULL);
  sigaction (SIGINT, &sa, NULL);
  sigaction (SIGTERM, &sa, NULL);
  sigaction (SIGXFSZ, &sa, NULL);
  sigaction (SIGXCPU, &sa, NULL);
}



// ------------------------------------------------------------------------



int main (int argc, char *argv[])
{
  __pmSetProgname(argv[0]);
  setup_signals();

  string default_config_dir = 
    string(pmGetConfig("PCP_SYSCONF_DIR")) + (char)__pmPathSeparator() + "pmmgr";
  vector<pmmgr_job_spec*> js;

  int c;
  int polltime = 60;
  while ((c = getopt(argc, argv, "D:c:vp:")) != EOF)
    {
      switch (c)
        {
        case 'D':
          (void) __pmParseDebug(optarg);
          break;

        case 'v':
          pmDebug |= DBG_TRACE_APPL0;
          break;

        case 'p':
          polltime = atoi(optarg);
          if (polltime <= 0) 
            {
              cerr << "Poll time too short." << endl;
              exit(1);
            }
          break;

        case 'c':
          try 
            {
              pmmgr_job_spec* j = new pmmgr_job_spec(optarg);
              js.push_back (j);
            }
          catch (const pmmgr_exception& pme)
            {
              cerr << pme << endl;
              exit (1);
            }
          break;

        default:
          cerr << "Usage: " << pmProgname << " [options] ..." << endl
               << "Options:" << endl
               << "  -c DIR   add given configuration directory "
               <<                "(default " << default_config_dir << ")" << endl
               << "  -p NUM   set pmcd polling interval "
               <<                "(default " << polltime << ")" << endl
               << "  -v       verbose diagnostics to stderr" << endl
               << endl;
          exit (1);
        }
    }

  // default
  if (js.size() == 0)
    try 
      {
        pmmgr_job_spec* j = new pmmgr_job_spec(default_config_dir);
        js.push_back (j);
      }
    catch (const pmmgr_exception& pme)
      {
        cerr << pme << endl;
        exit (1);
      }

  while (! quit)
    {
      // Absorb any zombie child processes, which a subsequent poll may look for.
      // NB: don't tweak signal(SIGCHLD...) due to interference with system(3).
      while (1)
        {
          int ignored;
          int rc = waitpid ((pid_t) -1, &ignored, WNOHANG);
          if (rc <= 0)
            break;
        }

      for (unsigned i=0; i<js.size(); i++)
        try
          {
            js[i]->poll();
          }
        catch (const pmmgr_exception& pme)
          {
            cerr << pme << endl;
          }
      sleep (polltime);
    }

  for (unsigned i=0; i<js.size(); i++)
    delete js[i];

  // XXX: send a suicide signal to the process group?
  return 0;
}
