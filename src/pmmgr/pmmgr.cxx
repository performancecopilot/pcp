/*
 * Copyright (c) 2013-2017 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU General Public License
 * for more details.
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif
#include "pmmgr.h"
#include "impl.h"

#include <sys/stat.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cassert>

// (to simulate non-pthreads build)
// #undef HAVE_PTHREAD_H

extern "C" {
#include <fcntl.h>
#include <unistd.h>
#include <glob.h>
#include <sys/wait.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#ifdef IS_LINUX
#include <sys/syscall.h>
#endif
}


using namespace std;

// ------------------------------------------------------------------------


int quit;
int polltime = 60;


// ------------------------------------------------------------------------


// Create a string that is safe to pass to system(3), i.e., sh -c,
// by quoting metacharacters.  This transform generally should be
// applied only once.
string
sh_quote(const string& input)
{
  string output;
  for (unsigned i=0; i<input.length(); i++)
    {
      char c = input[i];
      if ((ispunct(c) || isspace(c)) && // quite aggressive
	  (c != ':' && c != '.' && c != '_' && c != '/' && c != '-')) // safe & popular punctuation
	output += '\\';
      output += c;
    }

  return output;
}


// Print a string to cout/cerr progress reports, similar to the
// stuff produced by __pmNotifyErr
ostream&
timestamp(ostream &o)
{
  time_t now;
  time (&now);
  char now2buf[30];
  char *now2 = ctime_r (&now, now2buf);
  if (now2)
    now2[19] = '\0'; // overwrite " YYYY \n"

  return o << "[" << (now2 ? now2 : "") << "] " << pmProgname << "("
	   << getpid()
#ifdef HAVE_PTHREAD_H
#ifdef IS_LINUX
	   << "/" << syscall(SYS_gettid)
#else
	   << "/" << pthread_self()
#endif
#endif
	   << "): ";
}

// ------------------------------------------------------------------------


// Lightweight wrapper for pthread_mutex_t, incl. nonlocking non-pthread backup implementation
struct lock_t {
#if HAVE_PTHREAD_H
private:
  pthread_mutex_t _lock;
public:
  lock_t() { pthread_mutex_init(& this->_lock, NULL); }
  ~lock_t() { pthread_mutex_destroy (& this->_lock); }
  void lock() { pthread_mutex_lock (& this->_lock); }
  void unlock() { pthread_mutex_unlock (& this->_lock); }
#else
public:
  lock_t() {}
  ~lock_t() {}
  void lock() {}
  void unlock() {}
#endif
private:
  lock_t(const lock_t&); // make uncopyable
  lock_t& operator=(lock_t const&); // make unassignable
};


// RAII style mutex holder that matches { } block lifetime
struct locker
{
public:
  locker(lock_t *_m): m(_m) { m->lock(); }
  ~locker() { m->unlock(); }
private:
  lock_t* m;
};



// A little class that impersonates an ostream to the extent that it can
// take << streaming operations.  It batches up the bits into an internal
// stringstream until it is destroyed; then flushes to the original ostream.
class lock_t;
class obatched
{
private:
  ostream& o;
  stringstream stro;
  static lock_t lock;
public:
  obatched(ostream& oo): o(oo) { }
  ~obatched() { locker do_not_cross_the_streams(& obatched::lock); o << stro.str(); o.flush(); }
  operator ostream& () { return stro; }
  template <typename T> ostream& operator << (const T& t) { stro << t; return stro; }
};
lock_t obatched::lock; // just the one, since cout/cerr iostreams are not thread-safe


extern "C" void *
pmmgr_daemon_poll_thread (void* a)
{
  pmmgr_daemon* d = (pmmgr_daemon*) a;
  d->poll();
  return 0;
}


// A wrapper for something like system(3), but responding quicker to
// interrupts and standardizing tracing.
int
pmmgr_configurable::wrap_system(const std::string& cmd)
{
  if (pmDebugOptions.appl1)
    timestamp(obatched(cout)) << "running " << cmd << endl;

  int pid = fork();
  if (pid == 0)
    {
      // child
      int rc = execl ("/bin/sh", "sh", "-c", cmd.c_str(), NULL);
      timestamp(obatched(cerr)) << "failed to execl sh -c " << cmd << " rc=" << rc << endl;
      _exit (1);
    }
  else if (pid < 0)
    {
      // error
      timestamp(obatched(cerr)) << "fork for " << cmd << " failed: errno=" << errno << endl;
      return -1;
    }
  else
    {
      // parent
      int status = -1;
      int rc;
      //timestamp(obatched(cout)) << "waiting for pid=" << pid << endl;

      do { rc = waitpid(pid, &status, 0); } while (!quit && rc == -1 && errno == EINTR); // TEMP_FAILURE_RETRY
      if (quit)
	{
	  // timestamp(obatched(cout)) << "killing pid=" << pid << endl;
	  kill (pid, SIGTERM); // just to be on the safe side
	  // it might linger a few seconds in zombie mode
	}

      //timestamp(obatched(cout)) << "done status=" << status << endl;
      if (status != 0)
	timestamp(obatched(cerr)) << "system(" << cmd << ") failed: rc=" << status << endl;
      return status;
    }
}


// A wrapper for something like popen(3), but responding quicker to
// interrupts and standardizing tracing.
string
pmmgr_configurable::wrap_popen(const std::string& cmd)
{
  string output;
  
  if (pmDebug & DBG_TRACE_APPL1)
    timestamp(obatched(cout)) << "pipe-running " << cmd << endl;

  FILE* f = popen(cmd.c_str(), "r");
  if (f == NULL)
    {
      timestamp(obatched(cerr)) << "failed to popen " << cmd << endl;
      return output;
    }

  // Block, collecting all stdout from the child process.
  // Expecting only quick & small snippets.
  while (!quit)
    {
      char buf[1024];
      size_t rc = fread(buf, 1, sizeof(buf), f);
      output += string(buf,rc);
      if (feof(f) || ferror(f))
        break;
    }

  int status = pclose(f);
  //timestamp(obatched(cout)) << "done status=" << status << endl;
  if (status != 0)
    timestamp(obatched(cerr)) << "popen(" << cmd << ") failed: rc=" << status << endl;

  if (pmDebug & DBG_TRACE_APPL1)
    timestamp(obatched(cout)) << "collected " << output << endl;

  return output;
}


// ------------------------------------------------------------------------


pmmgr_configurable::pmmgr_configurable(const string& dir):
  config_directory(dir)
{
}


vector<string>
pmmgr_configurable::get_config_multi(const string& file) const
{
  vector<string> lines;

  string complete_filename = config_directory + (char)__pmPathSeparator() + file;
  ifstream f (complete_filename.c_str());
  while (f.good()) {
    string line;
    getline(f, line);
    if (line != "") // could happen, even if now !f.good() due to EOF (line without \n)
      lines.push_back(line);
    if (! f.good())
      break;
    // NB: an empty line is not necessarily a problem
    #if 0
    if (line == "")
      timestamp(obatched(cerr)) << "file '" << file << "' parse warning: empty line ignored" << endl;
    #endif
  }

  return lines;
}


bool
pmmgr_configurable::get_config_exists(const string& file) const
{
  string complete_filename = config_directory + (char)__pmPathSeparator() + file;
  ifstream f (complete_filename.c_str());
  return (f.good());
}


string
pmmgr_configurable::get_config_single(const string& file) const
{
  vector<string> lines = get_config_multi (file);
  if (lines.size() == 1)
    return lines[0];
  else if (lines.size() > 1)
    {
      timestamp(obatched(cerr)) << "file '" << file << "' parse warning: ignored extra lines" << endl;
      return "";
    }
  else
    return ""; // even an empty or nonexistent file comes back with a ""
}

ostream& // NB: return by value!
pmmgr_configurable::timestamp(ostream& o) const
{
  return ::timestamp(o) << config_directory << ": ";
}



// ------------------------------------------------------------------------

// This can be a global cache, shared amongst all threads & connections,
// as it represents siply a parsing of a pcp metric-description string.

static std::map<std::string,pmMetricSpec*> parsed_metric_cache;
static lock_t parsed_metric_cache_lock;

pmMetricSpec*
pmmgr_job_spec::parse_metric_spec (const string& spec) const
{
  locker metric_cache_locking (& parsed_metric_cache_lock);

  if (parsed_metric_cache.find(spec) != parsed_metric_cache.end())
    return parsed_metric_cache[spec];

  const char* specstr =	 spec.c_str();
  pmMetricSpec* pms = 0;
  char *errmsg;
  char dummy_host[] = "";
  int rc = pmParseMetricSpec (specstr,
			      0, dummy_host, /* both ignored */
			      & pms, & errmsg);
  if (rc < 0) {
    timestamp(obatched(cerr)) << "hostid-metrics '" << specstr << "' parse error: " << errmsg << endl;
    free (errmsg);
  }

  parsed_metric_cache[spec] = pms;
  return pms;
}


// NB: note: this function needs to be reentrant/concurrency-aware, since
// multiple threads may be running it at the same time against the same
// pmmgr_job_spec object!
pmmgr_hostid
pmmgr_job_spec::compute_hostid (const pcp_context_spec& ctx) const
{
  // static hostid takes precedence
  string hostid_static = get_config_single ("hostid-static");
  if (hostid_static != "")
    return pmmgr_hostid (hostid_static);

  pmFG fg;
  int sts = pmCreateFetchGroup (&fg, PM_CONTEXT_HOST, ctx.c_str());
  if (sts < 0)
    return "";

  // parse all the hostid metric specifications
  vector<string> hostid_specs = get_config_multi("hostid-metrics");
  if (hostid_specs.size() == 0)
    hostid_specs.push_back(string("pmcd.hostname"));

  // load all metrics into a single big array of pmAtomValue via pmfg
  const unsigned max_instances_per_metric = 100;
  pmAtomValue *values = new pmAtomValue[max_instances_per_metric * hostid_specs.size()];
  memset (values, 0, sizeof(pmAtomValue)*max_instances_per_metric * hostid_specs.size());
  unsigned values_idx = 0; // index into values[]

  // extend the fetchgroup with all the metrics
  // NB: even cursory error handling is skipped, relying on pmfgs' reliable representation
  // of missing/potential values as (char*) NULLs in the output pmAtomValue.
  for (unsigned i=0; i<hostid_specs.size(); i++)
    {
      pmMetricSpec* pms = parse_metric_spec (hostid_specs[i]);

      if (pms->ninst)
        for (unsigned j=0; j<(unsigned)pms->ninst && j<max_instances_per_metric; j++)
          (void) pmExtendFetchGroup_item (fg, pms->metric, pms->inst[j], NULL,
                                          & values[values_idx++], PM_TYPE_STRING, NULL);
      else {
        (void) pmExtendFetchGroup_indom (fg, pms->metric, NULL, NULL, NULL,
                                         & values[values_idx], PM_TYPE_STRING, NULL,
                                         max_instances_per_metric, NULL, NULL);
        values_idx += max_instances_per_metric;
      }
    }

  // fetch them all, depositing strings in the pmAtomValue .cp fields
  (void) pmFetchGroup (fg);

  vector<string> hostid_fields;
  for (unsigned i=0; i<max_instances_per_metric * hostid_specs.size(); i++)
    if (values[i].cp) // assured by pmfg & prior-memset to be NULL or valid
      hostid_fields.push_back (values[i].cp);

  (void) pmDestroyFetchGroup (fg);
  delete[] values;

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
            {
              // drop other non-portable characters NB: this can mean
              // unintentional duplication in IDs, which a user can work
              // around by configuring additional hostid metrics.
            }
	}
    }

    return pmmgr_hostid (sanitized);
}


// NB: note: this function needs to be reentrant/concurrency-aware, since
// multiple threads may be running it at the same time against the same
// pmmgr_job_spec object!
set<string>
pmmgr_job_spec::find_containers (const pcp_context_spec& ctx) const
{
    set<string> result;
    enum { max_num_containers = 10000 }; /* large enough? */
    pmAtomValue running[max_num_containers];
    char* names[max_num_containers];
    unsigned num_containers = 0;
    pmFG pmfg;

    int sts = pmCreateFetchGroup (&pmfg, PM_CONTEXT_HOST, ctx.c_str());
    if (sts < 0)
        goto out;

    sts = pmExtendFetchGroup_indom (pmfg, "containers.state.running", NULL,
                                    NULL, names, running,
                                    PM_TYPE_32, NULL,
                                    max_num_containers, &num_containers, NULL);
    if (sts < 0)
        goto out2;

    sts = pmFetchGroup (pmfg);
    if (sts < 0)
        goto out2;

    for (unsigned i=0; i<num_containers; i++) {
      if (running[i].l == 0) // not running or value unavailable
        continue;
      if (names[i])
        result.insert (string(names[i]));
    }

 out2:
    (void) pmDestroyFetchGroup (pmfg);

 out:
    return result;
}


pmmgr_job_spec::pmmgr_job_spec(const std::string& config_directory):
  pmmgr_configurable(config_directory)
{
  // We don't actually have to do any configuration parsing at this
  // time.  Let's do it during poll(), which makes us more responsive
  // to run-time changes.
}


// ------------------------------------------------------------------------


pmmgr_job_spec::~pmmgr_job_spec()
{
  // free any cached pmMetricSpec's
  // locking not necessary; this dtor is not run in parallel with anything else
  for (map<string,pmMetricSpec*>::iterator it = parsed_metric_cache.begin();
       it != parsed_metric_cache.end();
       ++it)
    free (it->second); // aka pmFreeMetricSpec

  // kill all our daemons created during poll()
  for (map<pmmgr_hostid,pcp_context_spec>::iterator it = known_targets.begin();
       it != known_targets.end();
       ++it)
    note_dead_hostid (it->first); // shrinks daemons[] also

  assert (daemons.size()==0);
}





// ------------------------------------------------------------------------
// parallel pmcd searching
//
// (set<pcp_context_spec> => multimap<hostid,pcp_context_spec> mapping)

struct pmcd_search_task
{
  // Single BKL to protect all shared data.  This should be sufficient
  // since the bulk of the time of the search threads should be
  // blocked in the network stack (attempting connections).
  lock_t lock;

  pmmgr_job_spec *job; // reentrant/const members called from multiple threads
  set<pcp_context_spec>::const_iterator new_specs_iterator; // pointer into same
  set<pcp_context_spec>::const_iterator new_specs_end; // pointer into same

  multimap<pmmgr_hostid,pcp_context_spec> output;
};


extern "C" void *
pmmgr_pmcd_search_thread (void *a)
{
  pmcd_search_task* t = (pmcd_search_task*) a;
  assert (t != NULL);

  while (1)
    {
      pcp_context_spec spec;

      {
        locker grab_next_piece_of_work (& t->lock);

        if (t->new_specs_iterator == t->new_specs_end) // all done!
          break;

        spec = * (t->new_specs_iterator ++);
      }

      // NB: this may take seconds! $PMCD_CONNECT_TIMEOUT
      pmmgr_hostid hostid = t->job->compute_hostid (spec);

      if (hostid != "") // verified existence/liveness
	{
          locker update_output (& t->lock);

          t->output.insert (make_pair (hostid, spec));
        }
    }

  return 0;
}



// ------------------------------------------------------------------------
// parallel pcp_context_spec selection and container searching
//
// multimap<hostid,pcp_context_spec> => map<hostid,pcp_context_spec> mapping

struct pmcd_choice_container_search_task
{
  // Single BKL
  lock_t lock;

  bool subtarget_containers; // RO: cached spec->get_config_exists()

  pmmgr_job_spec *job; // reentrant/const members called from multiple threads

  multimap<pmmgr_hostid,pcp_context_spec>* input;
  multimap<pmmgr_hostid,pcp_context_spec>::const_iterator input_iterator; // pointer into same
  multimap<pmmgr_hostid,pcp_context_spec>::const_iterator input_end; // pointer into same

  const map<pmmgr_hostid,pcp_context_spec>* previous_output;
  map<pmmgr_hostid,pcp_context_spec>* output;
};


extern "C" void *
pmmgr_pmcd_choice_container_search_thread (void *a)
{
  pmcd_choice_container_search_task* t = (pmcd_choice_container_search_task*) a;
  assert (t != NULL);

  while (1)
    {
      multimap<pmmgr_hostid,pcp_context_spec>::const_iterator work_begin;
      multimap<pmmgr_hostid,pcp_context_spec>::const_iterator work_end;

      {
        locker grab_next_piece_of_work (& t->lock);

        work_begin = t->input_iterator;
        if (work_begin == t->input_end) // all done!
          break;

        // advance other threads to past this hostid
        work_end = t->input->upper_bound(work_begin->first); // as opposed to ++work_begin
        t->input_iterator = work_end;
      }
      // ... while we keep working on [work,work_end)

      // Choose a pcp_context_spec from the set available for this hostid.
      // Favour preexisting connection (from previous_output[]).

      pmmgr_hostid host = work_begin->first;
      assert (host != "");
      pcp_context_spec spec; // the chosen one
      map<pmmgr_hostid,pcp_context_spec>::const_iterator prev_spec_iter = t->previous_output->find(host);
      if (prev_spec_iter == t->previous_output->end()) // new hostid?
        spec = work_begin->second; // choose first (arbitrary); done
      else
        {
          // search for a match; choose if it it exists
          pcp_context_spec prev_spec = prev_spec_iter->second;
          multimap<pmmgr_hostid,pcp_context_spec>::const_iterator work;
          for (work = work_begin; work != work_end; work++)
            {
              if (work->second == prev_spec) // match found
                {
                  spec = work->second; // reconfirm previous choice; done
                  break;
                }
            }
          if (work == work_end) // no match found
            spec = work_begin->second; // choose first (arbitrary); done
        }
      assert (spec != "");

      // Enumerate running containers on the chosen spec, if appropriate.

      set<string> containers;
      if (t->subtarget_containers)
        containers = t->job->find_containers(spec);

      // Record all the pmcd instances (+containers) in the output
      {
        locker write_output (& t->lock);

        t->output->insert(make_pair(host,spec));

        for (set<string>::const_iterator it2 = containers.begin();
             it2 != containers.end();
             ++it2) {
          // XXX: presuming that the container name is safe & needs no escape;
          // on docker, this is ok because the container id is a long hex string,
          // and we don't support nested containers.
          pmmgr_hostid subtarget_hostid = host + string("--") + *it2;
          // Choose ? or & for the hostspec suffix-prefix, depending
          // on whether there's already a ?.  There can be only one (tm).
          char pfx = (spec.find('?') == string::npos) ? '?' : '&';
          pcp_context_spec subtarget_spec = spec + pfx + string("container=") + *it2;

          t->output->insert(make_pair(subtarget_hostid,subtarget_spec));
        }
      }
  }

  return 0;
}


void
pmmgr_job_spec::parallel_do(int num_threads, void * (*fn)(void *), void *data) const
{
#ifdef HAVE_PTHREAD_H
  vector <pthread_t> threads;
#endif

  for (int i = 0; i<num_threads; i++)
    {
#ifdef HAVE_PTHREAD_H
      pthread_t foo;
      int rc = pthread_create(&foo, NULL, fn, data);
      if (rc == 0)
	threads.push_back (foo);
      // No problem if we can't launch as many as the user suggested.
#endif
    }

  // Race main thread with the worker function too, until it returns.
  (void) (*fn) (data);

  // All threads should shut themselves down shortly.
#ifdef HAVE_PTHREAD_H
  for (unsigned i=0; i<threads.size(); i++)
    pthread_join (threads[i], NULL);
#endif
}



void
pmmgr_job_spec::poll()
{
  if (quit) return;

  // phase 1: run all discovery/probing functions to collect context-spec's
  set<pcp_context_spec> new_specs;

  vector<string> target_hosts = get_config_multi("target-host");
  for (unsigned i=0; i<target_hosts.size(); i++)
    new_specs.insert(target_hosts[i]);

  vector<string> target_discovery = get_config_multi("target-discovery");
  for (unsigned i=0; i<target_discovery.size() && !quit; i++)
    {
      char **urls = NULL;
      const char *discovery = (target_discovery[i] == "")
	? NULL
	: target_discovery[i].c_str();
      // NB: this call may take O(seconds).
      int numUrls = pmDiscoverServices (PM_SERVER_SERVICE_SPEC, discovery, &urls);
      if (numUrls <= 0)
	continue;
      for (int i=0; i<numUrls; i++)
	new_specs.insert(string(urls[i]));
      free ((void*) urls);
    }

  // probe kubernetes pods; assume they may be running pmcd at default port
  if (get_config_exists ("target-kubectl-pod"))
    {
      string target_kubectl_pod = get_config_single("target-kubectl-pod");
      string kubectl_cmd = "kubectl get pod -o " + sh_quote("jsonpath={.items[*].status.podIP}") +" "+ target_kubectl_pod;
      string ips_str = wrap_popen(kubectl_cmd);
      istringstream ips(ips_str);
      while(ips.good())
        {
          string ip;
          ips >> ip;
          if (ip != "")
            new_specs.insert(ip);
        }
    }

  // fallback to logging the local server, if nothing else is configured/discovered
  if (target_hosts.size() == 0 &&
      target_discovery.size() == 0 &&
      !get_config_exists ("target-kubectl-pod"))
    new_specs.insert("local:");

  if (pmDebug & DBG_TRACE_APPL1)
    {
      timestamp(obatched(cout)) << "poll targets" << endl;
      for (set<string>::const_iterator it = new_specs.begin();
           it != new_specs.end();
           ++it)
        timestamp(obatched(cout)) << *it << endl;
    }
  
  // phase 2: move previously-identified targets over, so we can tell who
  // has come or gone
  const map<pmmgr_hostid,pcp_context_spec> old_known_targets = known_targets;
  known_targets.clear();

  string num_threads_str = get_config_single("target-threads");
#ifdef _SC_NPROCESSORS_ONLN
  int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_cpus < 0) num_cpus = 1;
#else
  int num_cpus = 1;
#endif
  int num_threads = num_cpus * 32;
  if (num_threads_str != "")
    num_threads = atoi(num_threads_str.c_str());
  if (num_threads < 0)
    num_threads = 0;
  // Why * 32?  The pmcd-search function is not CPU hungry at all: it's
  // just a socket connect and a few packets exchanged.  The scarce
  // resources are file descriptors (1ish) and thread stack memory
  // (2MBish) each.  We guesstimate that a reasonable server box can
  // bear an extra 64MB of momentary RAM consumption.

  // phase 3a: map the context-specs to hostids to find new hosts via parallel threads
  pmcd_search_task t1;
  t1.job = this;
  t1.new_specs_iterator = new_specs.begin();
  t1.new_specs_end = new_specs.end();

  parallel_do (min(num_threads,(int)new_specs.size()), &pmmgr_pmcd_search_thread, &t1);
  assert (t1.new_specs_iterator == t1.new_specs_end);

  // phase 3b: map the winning hostids/context-specs to containers (if configured required)
  pmcd_choice_container_search_task t2;
  t2.job = this;
  t2.input = &t1.output;
  t2.input_iterator = t1.output.begin();
  t2.input_end = t1.output.end();
  t2.subtarget_containers = get_config_exists("subtarget-containers");
  t2.previous_output = & old_known_targets;
  t2.output = & known_targets;

  parallel_do (min(num_threads, (int)t2.input->size()), &pmmgr_pmcd_choice_container_search_thread, &t2);
  assert (t2.input_iterator == t2.input_end);

  if (pmDebugOptions.appl1)
      {
	  timestamp(obatched(cout)) << "poll results" << endl;
	  for (map<pmmgr_hostid,pcp_context_spec>::const_iterator it = known_targets.begin();
	       it != known_targets.end();
	       ++it)
	      timestamp(obatched(cout)) << it->first << " @ " << it->second << endl;
      }

  // phase 4a: compare old_known_targets vs. known_targets: look for any recently died
  for (map<pmmgr_hostid,pcp_context_spec>::const_iterator it = old_known_targets.begin();
       it != old_known_targets.end();
       ++it)
    {
      const pmmgr_hostid& hostid = it->first;
      if ((known_targets.find(hostid) == known_targets.end()) || // host disappeared?
	  (known_targets[hostid] != it->second)) // reappeared at different address?
	note_dead_hostid (hostid); // kills daemon pids; shrinks daemons[]; might take a fraction of a second
    }

  // phase 4b: compare new known_targets & old_known_targets: look for recently born
  for (map<pmmgr_hostid,pcp_context_spec>::const_iterator it = known_targets.begin();
       it != known_targets.end();
       ++it)
    {
      const pmmgr_hostid& hostid = it->first;
      if ((old_known_targets.find(hostid) == old_known_targets.end()) || // new host?
	  (old_known_targets.find(hostid)->second != it->second)) // reappeared at different address?
	note_new_hostid (hostid, known_targets[hostid]); // grows daemons[]; doesn't start daemon pids; instant
    }

  // phase 5: poll all the live daemons
  // NB: there is a parallelism opportunity, as running many pmlogconf/etc.'s in series
  // is a possible bottleneck.
#ifdef HAVE_PTHREAD_H
  vector<pthread_t> poll_threads;
#endif
  for (multimap<pmmgr_hostid,pmmgr_daemon*>::iterator it = daemons.begin();
       it != daemons.end() && !quit;
       ++it)
    {
#ifdef HAVE_PTHREAD_H
      pthread_t foo;
      int rc = pthread_create(&foo, NULL, &pmmgr_daemon_poll_thread, it->second);
      if (rc == 0)
	poll_threads.push_back (foo);
#else
      int rc = -1;
#endif
      if (rc) // threading failed or running single-threaded
	it->second->poll();
    }

#ifdef HAVE_PTHREAD_H
  for (unsigned i=0; i<poll_threads.size(); i++)
    pthread_join (poll_threads[i], NULL);
  poll_threads.clear();
#endif

  // phase 6: garbage-collect ancient log-directory subdirs
  string subdir_gc = get_config_single("log-subdirectory-gc");
  if (subdir_gc == "")
    subdir_gc = "90days";
  struct timeval tv;
  char *errmsg;
  int rc = pmParseInterval(subdir_gc.c_str(), & tv, & errmsg);
  if (rc < 0)
    {
      timestamp(obatched(cerr)) << "log-subdirectory-gc '" << subdir_gc << "' parse error: " << errmsg << endl;
      free (errmsg);
      // default to 90days in another way
      tv.tv_sec = 60 * 60 * 24 * 90;
      tv.tv_usec = 0;
    }
  time_t now;
  (void) time(& now);

  // NB: check less frequently?

  // XXX: getting a bit duplicative
  string default_log_dir =
    string(pmGetConfig("PCP_LOG_DIR")) + (char)__pmPathSeparator() + "pmmgr";
  string log_dir = get_config_single ("log-directory");
  if (log_dir == "") log_dir = default_log_dir;
  else if(log_dir[0] != '/') log_dir = config_directory + (char)__pmPathSeparator() + log_dir;

  glob_t the_blob;
  string glob_pattern = log_dir + (char)__pmPathSeparator() + "*";
  rc = glob (glob_pattern.c_str(),
	     GLOB_NOESCAPE
#ifdef GLOB_ONLYDIR
	     | GLOB_ONLYDIR
#endif
	     , NULL, & the_blob);
  if (rc == 0)
    {
      for (unsigned i=0; i<the_blob.gl_pathc && !quit; i++)
	{
	  string item_name = the_blob.gl_pathv[i];

	  // Reject if currently live hostid
	  // NB: basename(3) might modify the argument string, so we don't feed
	  // it item_name.c_str().
	  string target_name = basename(the_blob.gl_pathv[i]);
	  if (known_targets.find(target_name) != known_targets.end())
	    continue;

	  struct stat foo;
	  rc = stat (item_name.c_str(), & foo);
	  if (rc == 0 &&
	      S_ISDIR(foo.st_mode) &&
	      (foo.st_mtime + tv.tv_sec) < now)
	    {
	      // <Janine Melnitz>We've got one!!!!!</>
	      timestamp(obatched(cout)) << "gc subdirectory " << item_name << endl;
	      string cleanup_cmd = "/bin/rm -rf " + sh_quote(item_name);
	      (void) wrap_system(cleanup_cmd);
	    }
	}
      globfree (& the_blob);
    }
}


// ------------------------------------------------------------------------


void
pmmgr_job_spec::note_new_hostid(const pmmgr_hostid& hid, const pcp_context_spec& spec)
{
  timestamp(obatched(cout)) << "new hostid " << hid << " at " << string(spec) << endl;

  if (get_config_exists("pmlogger"))
    daemons.insert(make_pair(hid, new pmmgr_pmlogger_daemon(config_directory, hid, spec)));

  if (get_config_exists("pmie"))
    daemons.insert(make_pair(hid, new pmmgr_pmie_daemon(config_directory, hid, spec)));

  vector<string> lines = get_config_multi ("monitor");
  for (unsigned i=0; i<lines.size(); i++)
    daemons.insert(make_pair(hid, new pmmgr_monitor_daemon(config_directory, lines[i], i, hid, spec)));
}


extern "C" void *
pmmgr_daemon_dtor_thread (void *a)
{
  pmmgr_daemon *d = (pmmgr_daemon *) a;
  delete d;
  return 0;
}


void
pmmgr_job_spec::note_dead_hostid(const pmmgr_hostid& hid)
{
  timestamp(obatched(cout)) << "dead hostid " << hid << endl;

  pair<multimap<pmmgr_hostid,pmmgr_daemon*>::iterator,
       multimap<pmmgr_hostid,pmmgr_daemon*>::iterator> range =
    daemons.equal_range(hid);

  for (multimap<pmmgr_hostid,pmmgr_daemon*>::iterator it = range.first;
       it != range.second;
       ++it)
    {
#ifdef HAVE_PTHREAD_H
      pthread_t tid;
      int rc = pthread_create (&tid, NULL, & pmmgr_daemon_dtor_thread, it->second);
      if (rc == 0)
        pthread_detach (tid); // no need to wait for it
      else
        delete it->second; // no thread; run dtor function directly
#else
      delete it->second; // no thread; run dtor function directly
#endif
    }

  daemons.erase(range.first, range.second);
}


// ------------------------------------------------------------------------


pmmgr_daemon::pmmgr_daemon(const std::string& config_directory,
			   const pmmgr_hostid& hostid,
			   const pcp_context_spec& spec):
  pmmgr_configurable(config_directory),
  hostid(hostid),
  spec(spec),
  pid(0),
  last_restart_attempt(0)
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


pmmgr_monitor_daemon::pmmgr_monitor_daemon(const std::string& config_directory,
                                           const std::string& config_line,
                                           unsigned config_index,
                                           const pmmgr_hostid& hostid,
                                           const pcp_context_spec& spec):
  pmmgr_daemon(config_directory, hostid, spec), config_line(config_line), config_index(config_index)
{
}


pmmgr_daemon::~pmmgr_daemon() // NB: possibly launched in a detached thread
{
  if (pid != 0)
    {
      int ignored;

      (void) kill ((pid_t) pid, SIGTERM);

      // Unfortunately, some daemons don't always respond to SIGTERM
      // immediately, so we mustn't simply hang in a waitpid().	 This
      // has been observed with 3.9.6-era pmie.

      for (unsigned c=0; c<10; c++) { // try to kill/reap only a brief while
	struct timespec killpoll;
	killpoll.tv_sec = 0;
	killpoll.tv_nsec = 250*1000*1000; // 250 milliseconds
	(void) nanosleep (&killpoll, NULL);

	int rc = waitpid ((pid_t) pid, &ignored, WNOHANG); // collect zombie
	if (rc == pid)
	  break;

	// not dead yet ... try again a little harder
	(void) kill ((pid_t) pid, SIGKILL);
      }
      if (pmDebugOptions.appl1)
	timestamp(obatched(cout)) << "daemon pid " << pid << " killed" << endl;
    }
}


void pmmgr_daemon::poll()
{
  if (quit) return;

  if (pid != 0) // test if it's still alive
    {
      // reap it if it might have died
      int ignored;
      int rc = waitpid ((pid_t) pid, &ignored, WNOHANG);

      rc = kill ((pid_t) pid, 0);
      if (rc < 0)
	{
	  if (pmDebugOptions.appl0)
	    timestamp(obatched(cout)) << "daemon pid " << pid << " found dead" << endl;
	  pid = 0;
	  // we will try again immediately
	  sleep (1);
	  // .. but not quite immediately; if a pmmgr daemon in
	  // granular mode shut down one second before the end of its
	  // period, the restarted form shouldn't be started in that
	  // exact same second.
	}
    }

  if (pid == 0) // needs a restart
    {
      time_t now;
      time (& now);

      // Prevent an error in the environment or the pmmgr daemon
      // command lines from generating a tight loop of failure /
      // retry, wasting time and log file space.  Limit retry attempts
      // to one per poll interval (pmmgr -p N parameter).
      if (last_restart_attempt && (last_restart_attempt + polltime) >= now)
	return; // quietly, without attempting to restart

      string commandline = daemon_command_line(); // <--- may take many seconds!

      // NB: Note this time as a restart attempt, even if daemon_command_line()
      // returned an empty string, so that we don't try to restart it too soon.
      // We note this time rather than the beginning of daemon_command_line(),
      // to ensure at least polltime seconds of rest between attempts.
      last_restart_attempt = now;

      if (quit) return; // without starting the daemon process

      if (commandline == "") // error in some intermediate processing stage
	{
	  timestamp(obatched(cerr)) << "failed to prepare daemon command line" << endl;
	  return;
	}

      // We are going to run the daemon with sh -c, but on some versions of
      // sh, this doesn't imply an exec, which interferes with signalling.
      // Enforce exec on even these shells.
      commandline = string("exec ") + commandline;

      if (pmDebugOptions.appl1)
	timestamp(obatched(cout)) << "fork/exec sh -c " << commandline << endl;
      pid = fork();
      if (pid == 0) // child process
	{
	  int rc = execl ("/bin/sh", "sh", "-c", commandline.c_str(), NULL);
	  timestamp(obatched(cerr)) << "failed to execl sh -c " << commandline << " rc=" << rc << endl;
	  _exit (1);
	  // parent will try again at next poll
	}
      else if (pid < 0) // failed fork
	{
	  timestamp(obatched(cerr)) << "failed to fork for sh -c " << commandline << endl;
	  pid = 0;
	  // we will try again at next poll
	}
      else // congratulations!	we're apparently a parent
	{
	  if (pmDebugOptions.appl0)
	    timestamp(obatched(cout)) << "daemon pid " << pid << " started: " << commandline << endl;
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
  else if(log_dir[0] != '/') log_dir = config_directory + (char)__pmPathSeparator() + log_dir;

  (void) mkdir2 (log_dir.c_str(), 0777); // implicitly consults umask(2)

  string host_log_dir = log_dir + (char)__pmPathSeparator() + hostid;
  (void) mkdir2 (host_log_dir.c_str(), 0777);
  // (errors creating actual files under host_log_dir will be noted shortly)

  string pmlogger_command =
	string(pmGetConfig("PCP_BIN_DIR")) + (char)__pmPathSeparator() + "pmlogger";
  string pmlogger_options = sh_quote(pmlogger_command);
  pmlogger_options += " " + get_config_single ("pmlogger") + " ";

  // run pmlogconf if requested
  if (get_config_exists("pmlogconf"))
    {
      string pmlogconf_output_file = host_log_dir + (char)__pmPathSeparator() + "config.pmlogger";
      (void) unlink (pmlogconf_output_file.c_str());
      string pmlogconf_command =
	string(pmGetConfig("PCP_BINADM_DIR")) + (char)__pmPathSeparator() + "pmlogconf";
      string pmlogconf_options =
	sh_quote(pmlogconf_command)
	+ " -c -r -h " + sh_quote(spec)
	+ " " + get_config_single ("pmlogconf")
	+ " " + sh_quote(pmlogconf_output_file)
	+ " >/dev/null"; // pmlogconf is too chatty

      int rc = wrap_system(pmlogconf_options);
      if (rc) return "";

      pmlogger_options += " -c " + sh_quote(pmlogconf_output_file);
    }

  // collect -h direction
  pmlogger_options += " -h " + sh_quote(spec);
  pmlogger_options += " -H " + sh_quote(hostid);

  // hard-code -r to report metrics & expected disk usage rate
  pmlogger_options += " -r";

  // collect subsidiary pmlogger diagnostics
  pmlogger_options += " -l " + sh_quote(host_log_dir + (char)__pmPathSeparator() + "pmlogger.log");

  // do log merging
  if (get_config_exists ("pmlogmerge"))
    {
      string pmlogextract_command =
	string(pmGetConfig("PCP_BIN_DIR")) + (char)__pmPathSeparator() + "pmlogextract";

      string pmlogcheck_command =
	string(pmGetConfig("PCP_BIN_DIR")) + (char)__pmPathSeparator() + "pmlogcheck";

      string pmlogrewrite_command =
	string(pmGetConfig("PCP_BINADM_DIR")) + (char)__pmPathSeparator() + "pmlogrewrite";

      string pmlogreduce_command =
	string(pmGetConfig("PCP_BINADM_DIR")) + (char)__pmPathSeparator() + "pmlogreduce";

      string pmlogextract_options = sh_quote(pmlogextract_command);

      string retention = get_config_single ("pmlogmerge-retain");
      if (retention == "") retention = "14days";
      struct timeval retention_tv;
      char *errmsg;
      int rc = pmParseInterval(retention.c_str(), &retention_tv, &errmsg);
      if (rc)
	{
	  timestamp(obatched(cerr)) << "pmlogmerge-retain '" << retention << "' parse error: " << errmsg << endl;
	  free (errmsg);
	  retention = "14days";
	  retention_tv.tv_sec = 14*24*60*60;
	  retention_tv.tv_usec = 0;
	}
      pmlogextract_options += " -S -" + sh_quote(retention);

      // Arrange our new pmlogger to kill itself after the given
      // period, to give us a chance to rerun.
      string period = get_config_single ("pmlogmerge");
      if (period == "") period = "24hours";
      struct timeval period_tv;
      rc = pmParseInterval(period.c_str(), &period_tv, &errmsg);
      if (rc)
	{
	  timestamp(obatched(cerr)) << "pmlogmerge '" << period << "' parse error: " << errmsg << endl;
	  free (errmsg);
	  period = "24hours";
	  period_tv.tv_sec = 24*60*60;
	  period_tv.tv_usec = 0;
	}
      if (get_config_exists ("pmlogmerge-granular"))
	{
	  // adjust stopping time to the next multiple of period
	  struct timeval now_tv;
	  __pmtimevalNow (&now_tv);
	  time_t period_s = period_tv.tv_sec;
	  if (period_s < 1) period_s = 1; // at least one second
	  time_t period_end = ((now_tv.tv_sec + 1 + period_s) / period_s) * period_s - 1;

	  // Assert calculation sanity: we want to avoid the case
	  // where a daemon launches for 0 seconds.  This should already
	  // be prevented by the "+ 1" above.
	  if (period_end == now_tv.tv_sec)
	    period_end ++;

          char ctime_r_buf[26]; // 26 bytes of ctime()
	  period = string(" @") +
	    string(ctime_r(& period_end, ctime_r_buf)).substr(0,24); // 24: ctime(3) magic value, sans \n
	}
      pmlogger_options += " -y -T " + sh_quote(period); // NB: pmmgr host local time!

      // Find prior archives by globbing for archive-*.index files,
      // to exclude reduced-archives (if any).  (*.index files are
      // optional as per pcp-archive.5, but pmlogger_merge.sh relies
      // on it.)
      vector<string> mergeable_archives; // those to merge
      glob_t the_blob;
      string glob_pattern = host_log_dir + (char)__pmPathSeparator() + "archive-*.index";
      rc = glob (glob_pattern.c_str(), GLOB_NOESCAPE, NULL, & the_blob);
      if (rc == 0)
	{
	  struct timeval now_tv;
	  __pmtimevalNow (&now_tv);
	  time_t period_s = period_tv.tv_sec;
	  if (period_s < 1) period_s = 1; // at least one second
	  time_t prior_period_start = ((now_tv.tv_sec + 1 - period_s) / period_s) * period_s;
	  time_t prior_period_end = prior_period_start + period_s - 1;
	  // schedule end -before- the period boundary, so that the
	  // last recorded metric timestamp is strictly before the end

	  for (unsigned i=0; i<the_blob.gl_pathc; i++)
	    {
	      if (quit) return "";

	      string index_name = the_blob.gl_pathv[i];
	      string base_name = index_name.substr(0,index_name.length()-6); // trim .index

	      // Manage retention based upon the stat timestamps of the .index file,
	      // because the archives might be so corrupt that even loglabel-based
	      // checks could fail.  Non-corrupt archives will have already been merged
	      // into a fresher archive.
	      struct stat foo;
	      rc = stat (the_blob.gl_pathv[i], & foo);
	      if (rc)
		{
		  // this apprx. can't happen
		  timestamp(obatched(cerr)) << "stat '" << the_blob.gl_pathv[i] << "' error; skipping cleanup" << endl;
		  continue; // likely nothing can be done to this one
		}
	      else if ((foo.st_mtime + retention_tv.tv_sec) < now_tv.tv_sec)
		{
		  string bnq = sh_quote(base_name);

                  rc = 0;
                  if (get_config_exists ("pmlogreduce"))
                    {
                      string pmlogreduce_options = sh_quote(pmlogreduce_command);
                      pmlogreduce_options += " " + get_config_single ("pmlogreduce");

                      // turn $host_log_dir/archive-FOO.meta into $host_log_dir/reduced-FOO.meta
                      // NB: Don't assume $host_log_dir is too sanitized, so proceed backward from
                      // end.

                      string cutme = "archive-";
                      size_t cut_here = base_name.rfind(cutme);
                      if (cut_here == string::npos) // can't happen; guaranteed by glob_pattern
                        continue;
                      size_t cut_len = cutme.length();
                      string output_file = base_name;
                      output_file.replace(cut_here, cut_len, "reduced-");

                      pmlogreduce_options += " " + sh_quote(base_name) + " " + sh_quote(output_file);
                      rc = wrap_system(pmlogreduce_options);
                      if (rc)
                        timestamp(obatched(cerr)) << "pmlogreduce error; keeping " << index_name << endl;
                    }

		  string cleanup_cmd = string("/bin/rm -f")
                      + " " + bnq + ".[0-9]*"
                      + " " + bnq + ".index" +
                      + " " + bnq + ".meta";

                  if (rc == 0) // only delete if the pmlogreduce succeeded!
                    (void) wrap_system(cleanup_cmd);
		  continue; // it's gone now; don't try to merge it or anything
		}

	      if (quit) return "";

	      // In granular mode, skip if this file is too old or too new.  NB: Decide
	      // based upon the log-label, not fstat timestamps, since files postdate
	      // the time region they cover.
	      if (get_config_exists ("pmlogmerge-granular"))
		{
		  // One could do this the pmloglabel(1) __pmLog* way,
		  // rather than the pmlogsummary(1) PMAPI way.

		  int ctx = pmNewContext(PM_CONTEXT_ARCHIVE, base_name.c_str());
		  if (ctx < 0)
		    continue; // skip; gc later

		  pmLogLabel label;
		  rc = pmGetArchiveLabel (& label);
		  if (rc < 0)
		    continue; // skip; gc later

		  if (label.ll_start.tv_sec >= prior_period_end) // archive too new?
		    {
		      if (pmDebugOptions.appl1)
			timestamp(obatched(cout)) << "skipping merge of too-new archive " << base_name << endl;
		      pmDestroyContext (ctx);
		      continue;
		    }

		  struct timeval archive_end;
		  rc = pmGetArchiveEnd(&archive_end);
		  if (rc < 0)
		    {
		      pmDestroyContext (ctx);
		      continue; // skip; gc later
		    }

		  if (archive_end.tv_sec < prior_period_start) // archive too old?
		    {
		      if (pmDebugOptions.appl1)
			timestamp(obatched(cout)) << "skipping merge of too-old archive " << base_name << endl;
		      pmDestroyContext (ctx);
		      continue; // skip; gc later
		    }

		  pmDestroyContext (ctx);
		  // fallthrough: the archive intersects the prior_period_{start,end} interval

		  // XXX: What happens for archives that span across granular periods?
		}

	      if (quit) return "";

	      // sic pmlogcheck on it; if it is broken, pmlogextract
	      // will give up and make no progress
	      string pmlogcheck_options = sh_quote(pmlogcheck_command);
	      pmlogcheck_options += " " + sh_quote(base_name) + " >/dev/null 2>/dev/null";

	      rc = wrap_system(pmlogcheck_options);
	      if (rc != 0)
		{
		  timestamp(obatched(cerr)) << "corrupt archive " << base_name << " preserved." << endl;

                  string preserved_name = base_name;
                  size_t pos = preserved_name.rfind("archive-");
                  assert (pos != string::npos); // by glob
                  preserved_name.replace(pos, 8, "corrupt-");

                  string rename_cmd = string(pmGetConfig("PCP_BIN_DIR"))+(char)__pmPathSeparator()+"pmlogmv";
                  rename_cmd += " " + sh_quote(base_name) + " " + sh_quote(preserved_name);
		  (void) wrap_system(rename_cmd);

		  continue;
		}

              // ugly heuristic to protect against SGI PR1054: sending
              // too many archives to pmlogextract at once can let it
              // exhaust file descriptors and fail without making progress
              const char *batch_str = getenv("PCP_PMMGR_MERGEBATCH");
              if (batch_str == NULL) batch_str = "";
              int batch = atoi(batch_str);
              if (batch <= 1) // need some forward progress
                batch = 64;
              mergeable_archives.push_back (base_name);
              if ((int)mergeable_archives.size() > batch)
                break; // we'll retry merging the others before too long - at next poll cycle
	    }
	  globfree (& the_blob);
	}

      // remove too-old reduced archives too
      glob_pattern = host_log_dir + (char)__pmPathSeparator() + "reduced-*.index";
      logans_run_archive_glob(glob_pattern, "pmlogreduce-retain", 90*24*60*60);

      // remove too-old corrupt archives too
      glob_pattern = host_log_dir + (char)__pmPathSeparator() + "corrupt-*.index";
      logans_run_archive_glob(glob_pattern, "pmlogcheck-corrupt-gc", 90*24*60*60);

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
      string merged_archive_name = host_log_dir + (char)__pmPathSeparator() + timestr;

      if (mergeable_archives.size() > 1) // 1 or 0 are not worth merging!
	{
	  // assemble final bits of pmlogextract command line: the inputs and the output
	  for (unsigned i=0; i<mergeable_archives.size(); i++)
	    {
	      if (quit) return "";

	      if (get_config_exists("pmlogmerge-rewrite"))
		{
		  string pmlogrewrite_options = sh_quote(pmlogrewrite_command);
		  pmlogrewrite_options += " -i " + get_config_single("pmlogmerge-rewrite");
		  pmlogrewrite_options += " " + sh_quote(mergeable_archives[i]);

		  (void) wrap_system(pmlogrewrite_options.c_str());
		  // In case of error, don't break; let's try to merge it anyway.
		  // Maybe pmlogrewrite will succeed and will get rid of this file.
		}

	      pmlogextract_options += " " + sh_quote(mergeable_archives[i]);
	    }

	  if (quit) return "";

	  pmlogextract_options += " " + sh_quote(merged_archive_name);

	  rc = wrap_system(pmlogextract_options.c_str());
	  if (rc == 0)
	    {
	      // zap the previous archive files
	      //
	      // Don't skip this upon "if (quit)", since the new merged archive is already complete;
	      // it'd be a waste to keep these files around for a future re-merge.
	      for (unsigned i=0; i<mergeable_archives.size(); i++)
		{
		  string base_name = sh_quote(mergeable_archives[i]);
		  string cleanup_cmd = string("/bin/rm -f")
		    + " " + base_name + ".[0-9]*"
		    + " " + base_name + ".index" +
		    + " " + base_name + ".meta";

		  (void) wrap_system(cleanup_cmd.c_str());
		}
	    }
	}
    }

  // synthesize a logfile name similarly as pmlogger_check, but add %S (seconds)
  // to reduce likelihood of conflict with a short poll interval
  string timestr = "archive";

  string pmlogger_timefmt = get_config_single ("pmlogger-timefmt");
  if (pmlogger_timefmt == "") pmlogger_timefmt = "%Y%m%d.%H%M%S";

  time_t now2 = time(NULL);
  struct tm *now = gmtime(& now2);
  if (now != NULL)
    {
      char timestr2[100];
      int rc = strftime(timestr2, sizeof(timestr2), pmlogger_timefmt.c_str(), now);
      if (rc > 0)
        {
          timestr += "-";
          timestr += timestr2; // no sh_quote required
        }
    }

  // last argument
  pmlogger_options += " " + sh_quote(host_log_dir + (char)__pmPathSeparator() + timestr);

  return pmlogger_options;
}


void
pmmgr_pmlogger_daemon::logans_run_archive_glob(const std::string& glob_pattern,
                                               const std::string& carousel_config,
                                               time_t carousel_default)
{
  int rc;
  struct timeval retention_tv;
  string retention = get_config_single (carousel_config);
  if (retention != "")
    {
      char *errmsg = NULL;
      rc = pmParseInterval(retention.c_str(), &retention_tv, &errmsg);
      if (rc)
        {
          timestamp(obatched(cerr)) << carousel_config << " '" << retention << "' parse error: " << errmsg << endl;
	  free (errmsg);
          retention_tv.tv_sec = carousel_default;
          retention_tv.tv_usec = 0;
        }
    }
  else
    {
      retention_tv.tv_sec = carousel_default;
      retention_tv.tv_usec = 0;
    }

  glob_t the_blob;
  rc = glob (glob_pattern.c_str(), GLOB_NOESCAPE, NULL, & the_blob);
  if (rc == 0)
    {
      struct timeval now_tv;
      __pmtimevalNow (&now_tv);
      for (unsigned i=0; i<the_blob.gl_pathc; i++)
        {
          if (quit) break;

          string index_name = the_blob.gl_pathv[i];
          string base_name = index_name.substr(0,index_name.length()-6); // trim .index

          // Manage retention based upon the stat timestamps of the .index file,
          // same as above.  NB: this is invariably a -younger- base point than
          // the archive log-label!
          struct stat foo;
          rc = stat (the_blob.gl_pathv[i], & foo);
          if (rc)
            {
              // this apprx. can't happen
              timestamp(obatched(cerr)) << "stat '" << the_blob.gl_pathv[i] << "' error; skipping cleanup" << endl;
              continue; // likely nothing can be done to this one
            }
          if (pmDebugOptions.appl1)
            timestamp(obatched(cout)) << "contemplating deletion of archive " << base_name
                                      << " (" << foo.st_mtime << "+" << retention_tv.tv_sec
                                      << " < " << now_tv.tv_sec << ")"
                                      << endl;
          if ((foo.st_mtime + retention_tv.tv_sec) < now_tv.tv_sec)
            {
              string bnq = sh_quote(base_name);
              string cleanup_cmd = string("/bin/rm -f")
                + " " + bnq + ".[0-9]*"
                + " " + bnq + ".index" +
                + " " + bnq + ".meta";

              (void) wrap_system(cleanup_cmd);
            }
        }
    }
  globfree (& the_blob);
}



std::string
pmmgr_pmie_daemon::daemon_command_line()
{
  string default_log_dir =
    string(pmGetConfig("PCP_LOG_DIR")) + (char)__pmPathSeparator() + "pmmgr";
  string log_dir = get_config_single ("log-directory");
  if (log_dir == "") log_dir = default_log_dir;
  else if(log_dir[0] != '/') log_dir = config_directory + (char)__pmPathSeparator() + log_dir;

  (void) mkdir2 (log_dir.c_str(), 0777); // implicitly consults umask(2)

  string host_log_dir = log_dir + (char)__pmPathSeparator() + hostid;
  (void) mkdir2 (host_log_dir.c_str(), 0777);
  // (errors creating actual files under host_log_dir will be noted shortly)

  string pmie_command =
	string(pmGetConfig("PCP_BIN_DIR")) + (char)__pmPathSeparator() + "pmie";
  string pmie_options = sh_quote (pmie_command);

  pmie_options += " " + get_config_single ("pmie") + " ";

  // run pmieconf if requested
  if (get_config_exists ("pmieconf"))
    {
      string pmieconf_output_file = host_log_dir + (char)__pmPathSeparator() + "config.pmie";
      string pmieconf_command =
	string(pmGetConfig("PCP_BIN_DIR")) + (char)__pmPathSeparator() + "pmieconf";

      // NB: pmieconf doesn't take a host name as an argument, unlike pmlogconf
      string pmieconf_options =
	sh_quote(pmieconf_command)
	+ " -F -c " + get_config_single ("pmieconf")
	+ " -f " + sh_quote(pmieconf_output_file);

      int rc = wrap_system(pmieconf_options.c_str());
      if (rc) return "";

      pmie_options += "-c " + sh_quote(pmieconf_output_file);
    }

  if (quit) return "";

  // collect -h direction
  pmie_options += " -h " + sh_quote(spec);

  // collect -f, to get it to run in the foreground, avoid setuid
  pmie_options += " -f";

  // collect subsidiary pmlogger diagnostics
  pmie_options += " -l " + sh_quote(host_log_dir + (char)__pmPathSeparator() + "pmie.log");

  return pmie_options;
}


std::string
pmmgr_monitor_daemon::daemon_command_line()
{
  string default_log_dir =
    string(pmGetConfig("PCP_LOG_DIR")) + (char)__pmPathSeparator() + "pmmgr";
  string log_dir = get_config_single ("log-directory");
  if (log_dir == "") log_dir = default_log_dir;
  else if(log_dir[0] != '/') log_dir = config_directory + (char)__pmPathSeparator() + log_dir;

  (void) mkdir2 (log_dir.c_str(), 0777); // implicitly consults umask(2)

  string host_log_dir = log_dir + (char)__pmPathSeparator() + hostid;
  (void) mkdir2 (host_log_dir.c_str(), 0777);

  stringstream monitor_command;
  monitor_command << config_line
                  << " -h " << sh_quote(spec)
                  << " >" << sh_quote(host_log_dir) << "/monitor-"  << config_index << ".out"
                  << " 2>" << sh_quote(host_log_dir) << "/monitor-"  << config_index << ".err";

  return monitor_command.str();
}


// ------------------------------------------------------------------------


extern "C"
void handle_interrupt (int sig)
{
  (void) sig;

  // Propagate signal to inferior processes (just once, to prevent
  // recursive signals or whatnot, despite sa_mask in
  // setup_signals()).
  if (quit == 0)
    kill(-getpid(), SIGTERM);

  quit ++;
  if (quit > 3) // ignore 1 from user; 1 from kill(-getpid) above; 1 from same near main() exit
    {
      char msg[] = "Too many interrupts received, exiting.\n";
      int rc = write (2, msg, sizeof(msg)-1);
      if (rc) {/* Do nothing; we don't care if our last gasp went out. */ ;}
      // XXX: send a suicide signal to the process group?
      _exit (1);
    }
}

extern "C"
void ignore_signal (int sig)
{
  (void) sig;
}



void setup_signals()
{
  // NB: we eschew __pmSetSignalHandler, since it uses signal(3),
  // whose behavior is less predictable than sigaction(2).

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_interrupt;
  sigemptyset (&sa.sa_mask);
  sigaddset (&sa.sa_mask, SIGHUP);
  sigaddset (&sa.sa_mask, SIGINT);
  sigaddset (&sa.sa_mask, SIGTERM);
  sigaddset (&sa.sa_mask, SIGXFSZ);
  sigaddset (&sa.sa_mask, SIGXCPU);
  sa.sa_flags = SA_RESTART;
  sigaction (SIGHUP, &sa, NULL);
  sigaction (SIGINT, &sa, NULL);
  sigaction (SIGTERM, &sa, NULL);
  sigaction (SIGXFSZ, &sa, NULL);
  sigaction (SIGXCPU, &sa, NULL);

  // Not SIGPIPE - it can come from OS socket errors via __pmSend
  // that may or may not be blocked completely.
  signal (SIGPIPE, SIG_IGN);
}



// ------------------------------------------------------------------------


static pmOptions opts;
static pmLongOptions longopts[] =
  {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "config", 1, 'c', "DIR", "add configuration directory [default $PCP_SYSCONF_DIR/pmmgr]" },
    { "poll", 1, 'p', "NUM", "set pmcd polling interval [default 60]" },
    { "username", 1, 'U', "USER", "decrease privilege from root to user [default pcp]" },
    { "log", 1, 'l', "PATH", "redirect diagnostics and trace output" },
    { "verbose", 0, 'v', 0, "verbose diagnostics to stderr" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
  };

int main (int argc, char *argv[])
{
  /* Become our own process group, to assist signal passing to children. */
  setpgid(getpid(), 0);
  setup_signals();

  string default_config_dir =
    string(pmGetConfig("PCP_SYSCONF_DIR")) + (char)__pmPathSeparator() + "pmmgr";
  vector<pmmgr_job_spec*> js;

  int c;
  char* username_str;
  __pmGetUsername(& username_str);
  string username = username_str;
  char* output_filename = NULL;

  opts.long_options = longopts;
  opts.short_options = "D:c:vp:U:l:?";

  while ((c = pmgetopt_r(argc, argv, &opts)) != EOF)
    {
      switch (c)
	{
	case 'D': // undocumented
	  if ((c = pmSetDebug(opts.optarg)) < 0)
	    {
	      pmprintf("%s: unrecognized debug options specification (%s)\n",
		       pmProgname, opts.optarg);
	      opts.errors++;
	    }
	  break;

	case 'l':
	  output_filename = opts.optarg;
	  break;

	case 'v':
	    if (pmDebugOptions.appl0 == 0)
		pmSetDebug("appl0");
	    else
		pmSetDebug("appl1");
	  break;

	case 'p':
	  polltime = atoi(opts.optarg);
	  if (polltime <= 0)
	    {
	      pmprintf("%s: poll time too short\n", pmProgname);
	      opts.errors++;
	    }
	  break;

	case 'c':
	  js.push_back (new pmmgr_job_spec(opts.optarg));
	  break;

	case 'U':
	  username = opts.optarg;
	  break;

	default:
	    opts.errors++;
	}
    }

  if (opts.errors)
    {
      pmUsageMessage(&opts);
      exit(1);
    }

  // default
  if (js.size() == 0)
    js.push_back (new pmmgr_job_spec(default_config_dir));

  // let pmdapmcd know pmmgr is currently running
  // NB: A failure from this call is of no significance: pmmgr is not
  // required to be run as uid pcp or root, so must not fail for the
  // mere inability to write into /var/run/pcp.
  (void) __pmServerCreatePIDFile(pmProgname, 0);

#ifndef IS_MINGW
    /* lose root privileges if we have them */
    if (geteuid () == 0)
#endif
      __pmSetProcessIdentity(username.c_str());

  // (re)create log file, redirect stdout/stderr
  // NB: must be done after __pmSetProcessIdentity() for proper file permissions
  if (output_filename)
    {
      int fd;
      (void) unlink (output_filename); // in case one's left over from a previous other-uid run
      fd = open (output_filename, O_WRONLY|O_APPEND|O_CREAT|O_TRUNC, 0666);
      if (fd < 0)
	timestamp(obatched(cerr)) << "Cannot re-create logfile " << output_filename << endl;
      else
	{
	  int rc;
	  // Move the new file descriptors on top of stdout/stderr
	  rc = dup2 (fd, STDOUT_FILENO);
	  if (rc < 0) // rather unlikely
	    timestamp(obatched(cerr)) << "Cannot redirect logfile to stdout" << endl;
	  rc = dup2 (fd, STDERR_FILENO);
	  if (rc < 0) // rather unlikely
	    timestamp(obatched(cerr)) << "Cannot redirect logfile to stderr" << endl;
	  rc = close (fd);
	  if (rc < 0) // rather unlikely
	    timestamp(obatched(cerr)) << "Cannot close logfile fd" << endl;
	}

    }

  timestamp(obatched(cout)) << "Log started" << endl;
  while (! quit)
    {
      // In this section, we must not fidget with SIGCHLD, due to use of system(3).
      for (unsigned i=0; i<js.size() && !quit; i++)
	js[i]->poll();

      if (quit)
	break;

      // We want to respond quickly if a child daemon process dies.
      (void) signal (SIGCHLD, ignore_signal);
      (void) signal (SIGALRM, ignore_signal);
      // align alarm with next natural polltime-sized interval
      alarm (polltime-(((unsigned)time(NULL)%(unsigned)polltime)));
      pause ();
      alarm (0);
      (void) signal (SIGCHLD, SIG_DFL);
      (void) signal (SIGALRM, SIG_DFL);
    }

  // NB: don't let this cleanup be interrupted by pending-quit signals;
  // we want the daemon pid's killed.
  for (unsigned i=0; i<js.size(); i++)
    delete js[i];

  sleep (1); // apprx. wait for any detached daemon-killer threads to stop

  timestamp(obatched(cout)) << "Log finished" << endl;

  // Send a last-gasp signal out, just in case daemons somehow missed
  kill(-getpid(), SIGTERM);

  return 0;
}

/* Local Variables:  */
/* mode: c++         */
/* c-basic-offset: 2 */
/* End:              */
