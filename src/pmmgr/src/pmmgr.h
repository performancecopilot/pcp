/* -*- C++ -*-
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

#ifndef PMMGR_H
#define PMMGR_H

extern "C" {
#include "pmapi.h"
}
#include <string>
#include <vector>
#include <set>
#include <map>
#include <stdexcept>
#include <iostream>


typedef std::string pcp_context_spec; // pmNewContext PM_CONTEXT_HOST parameter
typedef std::string pmmgr_hostid; // a unique id for a pmcd

struct pmmgr_exception: public std::runtime_error {
  ~pmmgr_exception() throw () {} 
  pmmgr_exception(int rc);
  pmmgr_exception(int rc, const std::string& m);
  int pmerror;
};

std::ostream& operator << (std::ostream& o, const pmmgr_exception& e);



// Instances of pmmgr_configurable represent a configurable object,
// which reads one or more lines of djb-style directories. 
class pmmgr_configurable
{
protected:
  pmmgr_configurable(const std::string& dir);
  virtual ~pmmgr_configurable() {}

  std::vector<std::string> get_config_multi(const std::string&);
  std::string get_config_single(const std::string&);
  bool get_config_exists(const std::string&);

  // private: maybe?
  std::string config_directory;
};




// Instances of pmmgr_daemon represent a possibly-live, restartable daemon.
class pmmgr_daemon: public pmmgr_configurable 
{
public:
  pmmgr_daemon(const std::string& config_directory, 
               const pmmgr_hostid& hostid, const pcp_context_spec& spec);
  virtual ~pmmgr_daemon();
  void poll();

protected:
  pmmgr_hostid hostid;
  pcp_context_spec spec;
  int pid;

  virtual std::string daemon_command_line() = 0;
};


class pmmgr_pmlogger_daemon: public pmmgr_daemon
{
public:
  pmmgr_pmlogger_daemon(const std::string& config_directory, 
                        const pmmgr_hostid& hostid, const pcp_context_spec& spec);
protected:
  std::string daemon_command_line();
};

class pmmgr_pmie_daemon: public pmmgr_daemon
{
public:
  pmmgr_pmie_daemon(const std::string& config_directory, 
                    const pmmgr_hostid& hostid, const pcp_context_spec& spec);
protected:
  std::string daemon_command_line();
};




// An instance of a pmmgr_job_spec represents a pmmgr
// configuration item to monitor some set of pcp target patterns
// (which collectively map to a varying set of pmcd's), and a
// corresponding set of daemons to keep running for each of them.
//
// The pmcds are identified by a configurable algorithm that collects
// site-specific metrics into a single string, which is then sanitized
// to make it typeable, portable, useful as a directory name.
//
// It is configured from a djb-style control directory with files containing
// 100% pure content.  Multiple values within the files, where permitted,
// are newline-separated.
//
// /hostid: METRIC [METRIC]*
// /target-host: HOSTNAME [HOSTNAME*]
// /target-discover: PARAM [PARAM*]
// /pmlogreduce: MODE
// /pmlogconf: PARAMETERS
// /pmlogger: PARAMETERS
// /pmieconf: PARAMETERS
// /pmie: PARAMETERS
//
class pmmgr_job_spec: pmmgr_configurable
{
public:
  pmmgr_job_spec(const std::string& config_directory);
  ~pmmgr_job_spec(); // shut down all daemons
  void poll(); // check targets, daemons

private:
  std::vector<pmMetricSpec*> hostid_metrics;

  pmmgr_hostid compute_hostid (const pcp_context_spec&);
  std::map<pmmgr_hostid,pcp_context_spec> known_targets;

  void note_new_hostid(const pmmgr_hostid&, const pcp_context_spec&);
  void note_dead_hostid(const pmmgr_hostid&);
  std::multimap<pmmgr_hostid,pmmgr_daemon*> daemons;
};






#endif

