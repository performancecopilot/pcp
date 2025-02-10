
Package: libpcp-pmda-perl
Section: perl
Architecture: any
Depends: ${misc:Depends}, ${shlibs:Depends}, ${perl:Depends}, libpcp-pmda3 (= ${binary:Version})
Description: Performance Co-Pilot Domain Agent Perl module
 The PCP::PMDA Perl module contains the language bindings for
 building Performance Metric Domain Agents (PMDAs) using Perl.
 Each PMDA exports performance data for one specific domain, for
 example the operating system kernel, Cisco routers, a database,
 an application, etc.
 .
 The Performance Co-Pilot provides a unifying abstraction for
 all of the interesting performance data in a system, and allows
 client applications to easily retrieve and process any subset of
 that data.

Package: libpcp-import-perl
Section: perl
Architecture: any
Depends: ${misc:Depends}, ${shlibs:Depends}, ${perl:Depends}, libpcp-import1 (= ${binary:Version})
Description: Performance Co-Pilot log import Perl module
 The PCP::LogImport Perl module contains the language bindings for
 building Perl applications that import performance data from a file
 or real-time source and create a Performance Co-Pilot (PCP) archive
 suitable for use with the PCP tools.
 .
 The Performance Co-Pilot provides a unifying abstraction for
 all of the interesting performance data in a system, and allows
 client applications to easily retrieve and process any subset of
 that data.

Package: libpcp-logsummary-perl
Section: perl
Architecture: any
Depends: ${misc:Depends}, ${perl:Depends}, pcp (= ${binary:Version})
Description: Performance Co-Pilot historical log summary module
 The PCP::LogSummary module provides a Perl module for using the
 statistical summary data produced by the Performance Co-Pilot
 pmlogsummary utility.  This utility produces various averages,
 minima, maxima, and other calculations based on the performance
 data stored in a PCP archive.  The Perl interface is ideal for
 exporting this data into third-party tools (e.g. spreadsheets).
 .
 The Performance Co-Pilot provides a unifying abstraction for
 all of the interesting performance data in a system, and allows
 client applications to easily retrieve and process any subset of
 that data.

Package: libpcp-mmv-perl
Section: perl
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends}, ${perl:Depends}, libpcp-mmv1 (= ${binary:Version})
Description: Performance Co-Pilot Memory Mapped Value Perl module
 The PCP::MMV module contains the Perl language bindings for
 building scripts instrumented with the Performance Co-Pilot
 (PCP) Memory Mapped Value (MMV) mechanism.
 .
 This mechanism allows arbitrary values to be exported from an
 instrumented script into the PCP infrastructure for monitoring
 and analysis with pmchart, pmie, pmlogger and other PCP tools.

Package: pcp-import-sar2pcp
Depends: ${perl:Depends}, ${misc:Depends}, libpcp-import-perl, libxml-tokeparser-perl
Architecture: all
Description: Tool for importing data from sar into PCP archive logs
 Performance Co-Pilot (PCP) front-end tool for importing data from sar
 into standard PCP archive logs for replay with any PCP monitoring tool
 (such as pmie, pmlogsummary, pmchart or pmdumptext).

Package: pcp-import-ganglia2pcp
Depends: ${perl:Depends}, ${misc:Depends}, libpcp-import-perl, librrds-perl
Architecture: all
Description: Tool for importing data from ganglia into PCP archive logs
 Performance Co-Pilot (PCP) front-end tool for importing data from ganglia
 into standard PCP archive logs for replay with any PCP monitoring tool
 (such as pmie, pmlogsummary, pmchart or pmdumptext).

Package: pcp-import-mrtg2pcp
Depends: ${perl:Depends}, ${misc:Depends}, libpcp-import-perl
Architecture: all
Description: Tool for importing data from MRTG into PCP archive logs
 Performance Co-Pilot (PCP) front-end tool for importing data from MRTG
 (the Multi Router Traffic Grapher tool) into standard PCP archive logs
 for replay with any PCP monitoring tool (such as pmie, pmlogsummary,
 pmchart or pmdumptext).

Package: pcp-import-sheet2pcp
Depends: ${perl:Depends}, ${misc:Depends}, libpcp-import-perl, libxml-tokeparser-perl, libspreadsheet-read-perl
Architecture: all
Description: Tool for importing data from a spreadsheet into PCP archive logs
 Performance Co-Pilot (PCP) front-end tool for importing spreadheet data
 into standard PCP archive logs for replay with any PCP monitoring tool.
 (such as pmie, pmlogsummary, pmchart, or pmdumptext).

Package: pcp-import-iostat2pcp
Depends: ${perl:Depends}, ${misc:Depends}, libpcp-import-perl
Architecture: all
Description: Tool for importing data from iostat into PCP archive logs
 Performance Co-Pilot (PCP) front-end tool for importing data from iostat
 into standard PCP archive logs for replay with any PCP monitoring tool.
 (such as pmie, pmlogsummary, pmchart or pmdumptext).

