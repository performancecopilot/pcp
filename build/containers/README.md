PCP Containers
==============

There are two Performance Co-Pilot container builds here.

1. The main PCP container, pcp, which includes all components and is
   setup to run (your choice of) pmcd, pmproxy, pmlogger, pmie and a
   selection of PMDAs.
    
   https://quay.io/repository/performancecopilot/pcp
   https://github.com/performancecopilot/pcp/pkgs/container/pcp

2. A Grafana container, archive-analysis, which focusses on allowing
   PCP archive analysis using Grafana dashboards.  This container is
   not for live analysis and is setup to run pmproxy (PCP pmwebapi),
   a local valkey-server and grafana-server.  An automated discovery
   process loads PCP data in /archives and pre-fills dashboards.
   
   https://quay.io/repository/performancecopilot/archive-analysis
   https://github.com/performancecopilot/pcp/pkgs/container/archive-analysis

The ideal way to build these is using the Containerfile for each.  A
Containerfile.Makepkgs also exists - this is a local development aid
for occassionally bootstrapping new features and includes a complete
local PCP build process.  It is recommended to use the Containerfile
in practice - these are the files used for the PCP registries listed
above.

See the README files in each directory for detailed usage and build
instructions.
