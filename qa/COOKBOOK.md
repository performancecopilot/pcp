# PCP QA Cookbook
![Alt text](../images/pcplogo-80.png)

# Table of contents
<br>[1 Preamble](#preamble)
<br>[2 The basic model](#the-basic-model)
<br>[3 Creating a new test](#creating-a-new-test)
<br>&nbsp;&nbsp;&nbsp;[3.1 The **new** script](#the-new-script)
<br>[4 Common shell variables](#common-shell-variables)
<br>[5 Coding style suggestions for tests](#coding-style-suggestions-for-tests)
<br>&nbsp;&nbsp;&nbsp;[5.1 General principles](#general-principles)
<br>&nbsp;&nbsp;&nbsp;[5.2 Take control of stdout and stderr](#take-control-of-stdout-and-stderr)
<br>&nbsp;&nbsp;&nbsp;[5.3 **$seq_full** file suggestions](#$seq_full-file-suggestions)
<br>[6 Shell functions from *common.check*](#shell-functions-from-common.check)
<br>[7 Shell functions from *common.filter*](#shell-functions-from-common.filter)
<br>[8 **check** script](#check-script)
<br>&nbsp;&nbsp;&nbsp;[8.1 Command line options](#command-line-options)
<br>&nbsp;&nbsp;&nbsp;[8.2 **check.callback** script](#check.callback-script)
<br>&nbsp;&nbsp;&nbsp;[8.3 *check.log* file](#check.log-file)
<br>&nbsp;&nbsp;&nbsp;[8.4 *check.time* file](#check.time-file)
<br>&nbsp;&nbsp;&nbsp;[8.5 *qa_hosts.primary* and *qa_hosts* files](#qa_hosts.primary-and-qa_hosts-files)
<br>[9 Control files](#control-files)
<br>&nbsp;&nbsp;&nbsp;[9.1 The *group* file](#the-group-file)
<br>&nbsp;&nbsp;&nbsp;[9.2 The *triaged* file](#the-triaged-file)
<br>&nbsp;&nbsp;&nbsp;[9.3 The *localconfig* file](#the-localconfig-file)
<br>[10 Other helper scripts](#other-helper-scripts)
<br>&nbsp;&nbsp;&nbsp;[Summary](#summary)
<br>&nbsp;&nbsp;&nbsp;[10.1 **mk.logfarm** script](#mk.logfarm-script)
<br>&nbsp;&nbsp;&nbsp;[10.2 **mk.qa_hosts** script](#mk.qa_hosts-script)
<br>[11 Using valgrind](#using-valgrind)
<br>[12 Using helgrind](#using-helgrind)
<br>[13 *common* and *common.\** files](#common-and-common.-files)
<br>[14 Admin scripts](#admin-scripts)
<br>[15 Selinux considerations](#selinux-considerations)
<br>[16 Package lists](#package-lists)
<br>[17 Dealing with the Known Unknowns](#dealing-with-the-known-unknowns)
<br>[Initial Setup Appendix](#initial-setup-appendix)
<br>[Index](#index)

<a id="preamble"></a>
# 1 Preamble

These notes are designed to help with building and maintaining QA tests
for the Performance Co-Pilot (PCP) project
([www.pcp.io](http://www.pcp.io/) and
[https://github.com/performancecopilot/pcp](https://github.com/performancecopilot/pcp/pull/2501))

We assume you're a developer or tester, building or fixing PCP QA
tests, so you are operating in a git tree (not the
*/var/lib/pcp/testsuite* directory that is packaged and installed) and
let's assume you've already done:

`$ cd qa`

Since the "qa" directory is where all the action happens, scripts and
files in this cookbook that are not absolute paths are all relative to
the "qa" directory, so for example *src/app1* is the path *qa/src/app1*
relative to the base of the git tree.

If you're setting up the PCP QA infrastructure for a new machine or VM or container,
then refer to the [Initial Setup Appendix](#initial-setup-appendix) and the [Package lists](#package-lists) section in this document.

The phrase "test script" or simply "test" refers to one of the thousands of test scripts numbered
000 to 999 and then 1000 \[1]... For shell usage the "glob" pattern
**[0-9\]\*[0-9\]** does a reasonable job of matching all test scripts.

Where these notes need to refer to a specific test, we'll use **$seq**
to mean "the test you're currently interested in", so we're
assuming you've already done something like:

`$ seq=1234`

Commands (and their options), scripts, environment variables, shell
variables and shell procedures appear in **bold** case. File names
appear in *italic* case. Code snippets and example commands appear
indented and in `fixed width` font.

\[1] The unfortunate mix of 3-digit and 4-digit test numbers is a
historical accident; when we started, no one could have imagined that
we'd ever have more than a thousand test scripts!

<a id="the-basic-model"></a>
# 2 The basic model

Each test consists of a shell script **$seq** and an expected output
file **$seq.out**.

When run under the control of **check**, **$seq** is executed and the
output is captured and compared to **$seq.out**. If the two outputs are
identical the test is deemed to have passed, else the (unexpected)
output is saved to **$seq.out.bad**.

Central to this model is the fact that **$seq** must produce
deterministic output, independent of hostname, filesystem pathname
differences, date, locale, platform differences, variations in output
from non-PCP applications, etc. This is achieved by "filtering" command
output and log files to remove lines or fields that are not
deterministic, and replace variable text with constants.

The tests scripts are expected to exit with status 0, but may exit with
a non-zero status in cases of catastrophic failure, e.g. some service to
be exercised did not start correctly, so there is nothing to test.

As tests are developed and evolve, the **remake** script is used to
generate updated versions of **$seq.out**.

To assist with test failure triage, all tests also generate a
**$seq.full** file that contains additional diagnostics and unfiltered
output.

<a id="creating-a-new-test"></a>
# 3 Creating a new test

Always use **new** to create a skeletal test. In addition to creating
the skeletal test, this will **git** **add** the new test and the
**.out** file, and update the *group* file, so when you've finished the
script development you need to (at least):
```
$ remake $seq
$ git add $seq $seq.out
$ git commit
```
additional **git** commands and possibly *GNUmakefile* changes will be
needed if your test needs any additional new files, e.g. a new source
program or script below in the *qa/src* directory or a new archive in
the *qa/archives* directory.

<a id="the-new-script"></a>
## 3.1 The <a id="idx+cmds+new">**new**</a> script

Usage: **new** \[*options*] \[*seqno* \[*group* ...]]

The **new** script creates a new skeletal test.

The new test has a test number assigned from "gaps" in the
*group* file; the starting test number is randomly generated
(based on a range in **new**, your ordinal login name converted to a number,
the seconds component of the current time and some hashing ...
all designed to assign different numbers to different users at about
the same time and so a weak collision avoidance scheme).
The default pseudo-random allocation is the same as specifying
a *seqno* of **-**.

Alternatively
specify *seqno* on the command line to over-ride the starting position,
or use **-s** to pick the smallest unallocated test number.

Once the test number is assigned, a new skeletal test is created
and **$EDITOR** is launched to start editing the test script.

Other *options* are:

- **-n** show-me, do nothing
- **-q** quiet and quick, avoiding launching **$EDITOR**
- **-r** in addition tag the test as **:reserved** in the *group* file
  **-R** *seqno* use *seqno* that was previously tagged as **:reserved** in the *group* file
  **-v** be verbose

When you exit **$EDITOR** you'll be prompted for groups to associate
the new test with, unless one or more *group* was specified on the command line.

<a id="common-shell-variables"></a>
# 4 Common shell variables

The common preamble in every test script source some *common\** scripts
and the following shell variables that may be used in your test script.

|**Variable**|**Description**|
|---|---|
|<a id="idx+vars+PCP_star">**$PCP_\***</a>|Everything from **$PCP_DIR***/etc/pcp.conf* is placed in the environment by calling **$PCP_DIR***/etc/pcp.env* from *common.rc*, so for example **$PCP_LOG_DIR** is always defined and **$PCP_AWK_PROG** should be used instead of **awk**.|
|<a id="idx+vars+here">**$here**</a>|Current directory tests are run from. Most useful after a test script **cd**'s someplace else and you need to **cd** back, or reference a file back in the starting directory.|
|<a id="idx+vars+seq">**$seq**</a>|The sequence number of the current test.|
|<a id="idx+vars+seq_full">**$seq_full**</a>|Proper pathname to the test's *.full* file. Always use this in preference to **$seq**.*full* because **$seq_full** works no matter where the test script might have **cd**'d to.|
|<a id="idx+vars+status">**$status**</a>| Exit status for the test script.|
|<a id="idx+vars+sudo">**$sudo**</a>|Proper invocation of **sudo**(1) that includes any per-platform additional command line options.|
|<a id="idx+vars+tmp">**$tmp**</a>|Unique prefix for temporary files or directory. Use **$tmp.foo** or `$ mkdir $tmp` and then use **$tmp/foo** or both. The standard **trap** cleanup in each test will remove all these files and directories automatically when the test finishes, so save anything useful to **$seq_full**.|

<a id="coding-style-suggestions-for-tests"></a>
# 5 Coding style suggestions for tests

<a id="general-principles"></a>
## 5.1 General principles

"Good" QA tests are ones that typically:

- are focused on one area of functionality or previous regression (complex tests are more likely to pass in multiple subtests but failing a single subtest makes the whole test fail, and complex tests are harder to debug)
- run quickly -- the entire QA suite already takes a long time to run
- are resilient to platform and distribution changes
- don't check something that's already covered in another test
- when exercising complex parts of core PCP functionality we'd like to see both a non-valgrind and a valgrind version of the test (see [**new**](#new) and [**new-grind**](#new-grind) below).

TODO

<a id="take-control-of-stdout-and-stderr"></a>
## 5.2 Take control of stdout and stderr

If an application used in a test may produce output on either stdout or
stderr or both, the test may need to take control to capture all the
output and prevent platform-specific or libc-version-specific buffer
flushing that makes the output non-deterministic.

Instead of<br>
`$ cmd | _filter`<br>
the test may need to do<br>
`$ cmd 2>&1 | _filter`<br>
or even<br>
`$ cmd >$tmp.out 2>$tmp.err`<br>
`$ cat $tmp.err $tmp.out | _filter`

<a id="$seq_full-file-suggestions"></a>
## 5.3 <a id="idx+files+seq_full">**$seq_full**</a> file suggestions

Assume your test is going to fail at some point, so be defensive up
front. In particular ensure that your test appends the following
sort of information to **$seq_full**:

- values of critical shell variables that are not hard-coded, e.g. remote host, port being used for testing
- unfiltered output (before it goes to the **.out** file) especially if the filtering is complex or aggressive to meet determinism requirements
- on error paths, output from commands that might help explain the error cause
- context that helps match up test cases in the script with the output in both the **.out** and **.full** files, e.g. this is a common pattern:<br>
`$ echo "--- subtest foobar ---" | tee -a $seq_full`
- log files that are unsuitable for inclusion in **.out **

The common preamble for all tests will ensure **$seq_full** is removed at the start of each test, so you can safely use constructs like:
```
$ echo ... >>$seq_full
$ cmd ... | tee -a $seq_full | ...
```
Remember that **$seq_full** translates to file **$seq.full** (dot, not underscore) in the directory the **$seq** test is run from.

<a id="shell-functions-from-common.check"></a>
# 6 Shell functions from *common.check*

A large number of shell functions that are useful across
multiple test scripts are provided by *common.check* to assist with
test development and these are always available for tests created with
a standard preamble (as provided by the [**new**](#the-new-script) script.

In addition to defining the shell procedures (some of the more frequently
used ones are described in the table below), *common.check* also
handles:

- if necessary running [**mk.localconfig**](#idx+cmds+mk.localconfig)
- sourcing [*localconfig*](#idx+files+localconfig)
- setting <a id="idx+vars+PCPQA_SYSTEMD">**$PCPQA_SYSTEMD**</a> to **yes** or **no** depending if services are controlled by **systemctl**(1) or not
<br>

|**Function**|**Description**|
|---|---|
|<a id="idx+funcs+_all_hostnames">**\_all_hostnames**</a>|TODO|
|<a id="idx+funcs+_all_ipaddrs">**\_all_ipaddrs**</a>|TODO|
|<a id="idx+funcs+_arch_start">**\_arch_start**</a>|TODO|
|<a id="idx+funcs+_avail_metric">**\_avail_metric**</a>|TODO|
|<a id="idx+funcs+_change_config">**\_change_config**</a>|TODO|
|<a id="idx+funcs+_check_64bit_platform">**\_check_64bit_platform**</a>|TODO|
|<a id="idx+funcs+_check_agent ">**\_check_agent**</a>|TODO|
|<a id="idx+funcs+_check_core">**\_check_core**</a>|TODO|
|<a id="idx+funcs+_check_display">**\_check_display**</a>|TODO|
|<a id="idx+funcs+_check_freespace">**\_check_freespace**</a>|Usage: **\_check_freespace** *need*<br> Returns 0 if there is more that *need* Mbytes of free space in the filesystem for the current working directory, else returns 1.
|<a id="idx+funcs+_check_helgrind">**\_check_helgrind**</a>|TODO|
|<a id="idx+funcs+_check_job_scheduler">**\_check_job_scheduler**</a>|TODO|
|<a id="idx+funcs+_check_key_server">**\_check_key_server**</a>|TODO|
|<a id="idx+funcs+_check_key_server_ping">**\_check_key_server_ping**</a>|TODO|
|<a id="idx+funcs+_check_key_server_version">**\_check_key_server_version**</a>|TODO|
|<a id="idx+funcs+_check_key_server_version_offline">**\_check_key_server_version_offline**</a>|TODO|
|<a id="idx+funcs+_check_local_primary_archive">**\_check_local_primary_archive**</a>|TODO|
|<a id="idx+funcs+_check_metric">**\_check_metric**</a>|TODO|
|<a id="idx+funcs+_check_purify">**\_check_purify**</a>|TODO|
|<a id="idx+funcs+_check_search">**\_check_search**</a>|TODO|
|<a id="idx+funcs+_check_series">**\_check_series**</a>|TODO|
|<a id="idx+funcs+_check_valgrind">**\_check_valgrind**</a>|TODO|
|<a id="idx+funcs+_clean_display">**\_clean_display**</a>|TODO|
|<a id="idx+funcs+_cleanup_pmda">**\_cleanup_pmda**</a>|TODO|
|<a id="idx+funcs+_disable_loggers">**\_disable_loggers**</a>|TODO|
|<a id="idx+funcs+_domain_name">**\_domain_name**</a>|TODO|
|<a id="idx+funcs+_exit">**\_exit**</a>|Usage: **\_exit** *status*<br>Set [$status](#idx+vars+status) to *status* and force test exit.|
|<a id="idx+funcs+_fail">**\_fail**</a>|Usage: **\_fail** *message*<br>Emit *message* on stderr and force failure exit of test.|
|<a id="idx+funcs+_filesize">**\_filesize**</a>|Usage: **\_filesize** *file*<br>Output the size of *file* in bytes on stdout.|
|<a id="idx+funcs+_filter_helgrind">**\_filter_helgrind**</a>|TODO|
|<a id="idx+funcs+_filter_init_distro">**\_filter_init_distro**</a>|TODO|
|<a id="idx+funcs+__filter_pmlogger_log">**\__filter_pmlogger_log**</a>|TODO|
|<a id="idx+funcs+_filter_purify">**\_filter_purify**</a>|TODO|
|<a id="idx+funcs+_filter_valgrind">**\_filter_valgrind**</a>|TODO|
|<a id="idx+funcs+_find_free_port">**\_find_free_port**</a>|TODO|
|<a id="idx+funcs+_find_key_server_modules">**\_find_key_server_modules**</a>|TODO|
|<a id="idx+funcs+_find_key_server_name">**\_find_key_server_name**</a>|TODO|
|<a id="idx+funcs+_find_key_server_search">**\_find_key_server_search**</a>|TODO|
|<a id="idx+funcs+_get_config">**\_get_config**</a>|TODO|
|<a id="idx+funcs+_get_endian">**\_get_endian**</a>|TODO|
|<a id="idx+funcs+_get_fqdn">**\_get_fqdn**</a>|TODO|
|<a id="idx+funcs+_get_libpcp_config">**\_get_libpcp_config**</a>|TODO|
|<a id="idx+funcs+_get_port">**\_get_port**</a>|TODO|
|<a id="idx+funcs+_get_primary_logger_pid">**\_get_primary_logger_pid**</a>|TODO|
|<a id="idx+funcs+_get_word_size">**\_get_word_size**</a>|TODO|
|<a id="idx+funcs+_host_to_fqdn">**\_host_to_fqdn**</a>|TODO|
|<a id="idx+funcs+_host_to_ipaddr">**\_host_to_ipaddr**</a>|TODO|
|<a id="idx+funcs+_host_to_ipv6addrs">**\_host_to_ipv6addrs**</a>|TODO|
|<a id="idx+funcs+_ipaddr_to_host">**\_ipaddr_to_host**</a>|TODO|
|<a id="idx+funcs+_ipv6_localhost">**\_ipv6_localhost**</a>|TODO|
|<a id="idx+funcs+_libvirt_is_ok">**\_libvirt_is_ok**</a>|TODO|
|<a id="idx+funcs+_machine_id">**\_machine_id**</a>|TODO|
|<a id="idx+funcs+_make_helptext">**\_make_helptext**</a>|TODO|
|<a id="idx+funcs+_make_proc_stat">**\_make_proc_stat**</a>|TODO|
|<a id="idx+funcs+_need_metric">**\_need_metric**</a>|TODO|
|<a id="idx+funcs+_notrun">**\_notrun**</a>|Usage: **\_notrun** *message*<br>Not all tests are expected to be able to run on all platforms.  Reasons might include: won't work at all a certain operating system, application required by the test is not installed, metric required by the test is not available from **pmcd**(1), etc.<br>In these cases, the test should include a guard that captures the required precondition and call **\_notrun** with a helpful *message* if the guard fails.  For example.<br>&nbsp;&nbsp;&nbsp;`which pmrep >/dev/null 2>&1 || _notrun "pmrep not installed"`|
|<a id="idx+funcs+_path_readable">**\_path_readable**</a>|TODO|
|<a id="idx+funcs+_pid_in_container">**\_pid_in_container**</a>|TODO|
|<a id="idx+funcs+_prefer_valgrind">**\_prefer_valgrind**</a>|TODO|
|<a id="idx+funcs+_prepare_pmda">**\_prepare_pmda**</a>|TODO|
|<a id="idx+funcs+_prepare_pmda_install">**\_prepare_pmda_install**</a>|TODO|
|<a id="idx+funcs+_prepare_pmda_mmv">**\_prepare_pmda_mmv**</a>|TODO|
|<a id="idx+funcs+_private_pmcd">**\_private_pmcd**</a>|TODO|
|<a id="idx+funcs+_ps_tcp_port">**\_ps_tcp_port**</a>|TODO|
|<a id="idx+funcs+_pstree_all">**\_pstree_all**</a>|TODO|
|<a id="idx+funcs+_pstree_oneline">**\_pstree_oneline**</a>|TODO|
|<a id="idx+funcs+_remove_job_scheduler">**\_remove_job_scheduler**</a>|TODO|
|<a id="idx+funcs+_restore_config">**\_restore_config**</a>|Usage: **\_restore_config** *target*<br> Reinstates a configuration file or directory (*target*) previously save with **\_save_config**.|
|<a id="idx+funcs+_restore_job_scheduler">**\_restore_job_scheduler**</a>|TODO|
|<a id="idx+funcs+_restore_loggers">**\_restore_loggers**</a>|TODO|
|<a id="idx+funcs+_restore_pmda_install">**\_restore_pmda_install**</a>|TODO|
|<a id="idx+funcs+_restore_pmda_mmv">**\_restore_pmda_mmv**</a>|TODO|
|<a id="idx+funcs+_restore_pmlogger_control">**\_restore_pmlogger_control**</a>|TODO|
|<a id="idx+funcs+_restore_primary_logger">**\_restore_primary_logger**</a>|TODO|
|<a id="idx+funcs+_run_helgrind">**\_run_helgrind**</a>|TODO|
|<a id="idx+funcs+_run_purify">**\_run_purify**</a>|TODO|
|<a id="idx+funcs+_run_valgrind">**\_run_valgrind**</a>|TODO|
|<a id="idx+funcs+_save_config">**\_save_config**</a>|Usage: **\_save_config** *target*<br>Save a configuration file or directory (*target*) with a name that uses [$seq](#idx+vars+seq) so that if a test aborts we know who was dinking with the configuration.<br>Operates in concert with **\_restore_config**.|
|<a id="idx+funcs+_service">**\_service**</a>|Usage: **\_service** \[**-v**] *service* *action*<br>Controlling services like **pmcd**(1) or **pmlogger**(1) or ... may involve **init**(1) or **systemctl**(1) or something else.  This complexity is hidden behind the **\_service** function which should be used whenever as test wants to control a PCP service.<br> Supported values for *service* are: **pmcd**, **pmlogger**, **pmproxy** **pmie**.<br>*action* is one of **stop**, **start** (may be no-op if already started) or **restart** (force stop if necessary, then start).<br>Use **-v** for more verbosity.|
|<a id="idx+funcs+_set_dsosuffix">**\_set_dsosuffix**</a>|TODO|
|<a id="idx+funcs+_setup_purify">**\_setup_purify**</a>|TODO|
|<a id="idx+funcs+_sighup_pmcd">**\_sighup_pmcd**</a>|TODO|
|<a id="idx+funcs+_start_up_pmlogger">**\_start_up_pmlogger**</a>|TODO|
|<a id="idx+funcs+_stop_auto_restart">**\_stop_auto_restart**</a>|Usage: **\_stop_auto_restart** *service*<br>When testing error handling or timeout conditions for services it may be important to ensure the system does not try to restart a failed service (potentially leading to an hard loop of retry-fail-retry). **\_stop_auto_start** will change configuration to prevent restarting for *service* if the system supports this function.<br>Use <a id="idx+funcs+_restore_auto_restart">**\_restore_auto_restart**</a> with the same *service* to reinstate the configuration.|
|<a id="idx+funcs+_systemctl_status">**\_systemctl_status**</a>|TODO|
|<a id="idx+funcs+_triage_pmcd">**\_triage_pmcd**</a>|TODO|
|<a id="idx+funcs+__triage_systemd">**\__triage_systemd**</a>|TODO|
|<a id="idx+funcs+_triage_wait_point">**\_triage_wait_point**</a>|TODO|
|<a id="idx+funcs+_try_pmlc">**\_try_pmlc**</a>|TODO|
|<a id="idx+funcs+_wait_for_pmcd">**\_wait_for_pmcd**</a>|TODO|
|<a id="idx+funcs+_wait_for_pmcd_stop">**\_wait_for_pmcd_stop**</a>|TODO|
|<a id="idx+funcs+_wait_for_pmie">**\_wait_for_pmie**</a>|TODO|
|<a id="idx+funcs+_wait_for_pmlogger">**\_wait_for_pmlogger**</a>|TODO|
|<a id="idx+funcs+_wait_for_pmproxy">**\_wait_for_pmproxy**</a>|TODO|
|<a id="idx+funcs+_wait_for_pmproxy_logfile">**\_wait_for_pmproxy_logfile**</a>|TODO|
|<a id="idx+funcs+_wait_for_pmproxy_metrics">**\_wait_for_pmproxy_metrics**</a>|TODO|
|<a id="idx+funcs+_wait_for_port">**\_wait_for_port**</a>|TODO|
|<a id="idx+funcs+_wait_pmcd_end">**\_wait_pmcd_end**</a>|TODO|
|<a id="idx+funcs+_wait_pmie_end">**\_wait_pmie_end**</a>|TODO|
|<a id="idx+funcs+_wait_pmlogctl">**\_wait_pmlogctl**</a>|TODO|
|<a id="idx+funcs+_wait_pmlogger_end">**\_wait_pmlogger_end**</a>|TODO|
|<a id="idx+funcs+_wait_pmproxy_end">**\_wait_pmproxy_end**</a>|TODO|
|<a id="idx+funcs+_wait_process_end">**\_wait_process_end**</a>|TODO|
|<a id="idx+funcs+_webapi_header_filter">**\_webapi_header_filter**</a>|TODO|
|<a id="idx+funcs+_webapi_response_filter">**\_webapi_response_filter**</a>|TODO|
|<a id="idx+funcs+_within_tolerance">**\_within_tolerance**</a>|TODO|
|<a id="idx+funcs+_writable_primary_logger">**\_writable_primary_logger**</a>|TODO|

<a id="shell-functions-from-common.filter"></a>
# 7 Shell functions from *common.filter*

TODO

<a id="check-script"></a>
# 8 <a id="idx+cmds+check">**check**</a> script

TODO

<a id="command-line-options"></a>
## 8.1 Command line options

<a id="check.callback-script"></a>
## 8.2 **check.callback** script

If **check.callback** exists and is executable, then it will be run from
**check** both before and after each test case is completed. The
optional first argument is --**precheck** for the before call, and the
last argument is the test *seqno*. This provides a hook for testing
broader classes of failures that could be triggered from (but not
necessarily tested by) all tests.

An example script is provided in **check.callback.sample** and this can
either be used as a template for building a customized
**check.callback**, or more simply used as is:\
* $ ln -s check.callback.sample check.callback*

Based on **check.callback.sample** the sort of things tested here might
include:

- Pre and post capture of AVCs to detect new AVCs triggered by a specific test.
- Did **pmlogger_daily** get run as expected?
- Is **pmcd** healthy? This is delegated to **./941** with **\--check**.
- Is **pmlogger** healthy? This is delegated to **./870** with **--check**.
- Are all of the configured PMDAs still alive?
- Has the Performance Metrics Namespace (PMNS) been trashed? This is delegated to **./1190** with **--check**.
- Are there any PCP configuration files that contain text to indicate they have been modified by a QA test, as opposed to the version installed from packages.

<a id="check.log-file"></a>
## 8.3 <a id="idx+files+check.log">*check.log*</a> file

Historical record of each execution of **check**, reporting what tests
were run, notrun, passed, triaged, failed, etc.

<a id="check.time-file"></a>
## 8.4 <a id="idx+files+check.time">*check.time*</a> file

Elapsed time for last successful execution of each test run by
**check**.

<a id="qa_hosts.primary-and-qa_hosts-files"></a>
## 8.5 <a id="idx+files+qa_hosts.primary">*qa_hosts.primary*</a> and <a id="idx+files+qa_hosts">*qa_hosts*</a> files

TODO

<a id="control-files"></a>
# 9 Control files

There are several files that augment the test scripts and control
how QA tests are executed.

<a id="the-group-file"></a>
## 9.1 The <a id="idx+files+group">*group*</a> file

Each test belongs to one or more "groups" and the
*group* file is used to record
the set of known groups and the mapping between each test
and the associated groups of tests.

Groups are defined for applications, PMDAs, services, general
features or functional areas (e.g. **archive**, **pmns**, **getopt**, ...)
and testing type (e.g. **remote**, **local**, **not_in_ci**, ...).

Comments within the file provide further information as to format.

<a id="the-triaged-file"></a>
## 9.2 The <a id="idx+files+triaged">*triaged*</a> file

TODO

<a id="the-localconfig-file"></a>
## 9.3 The <a id="idx+files+localconfig">*localconfig*</a> file

The *localconfig* file is generated by the **mk.localconfig** script.
It defines the shell variables
*localconfig* is sourced from **common.check** so every test script has access to these shell variables.

<a id="other-helper-scripts"></a>
# 10 Other helper scripts

There are a large number of shell scripts in the QA directory that are
intended for common QA development and triage tasks beyond simply
running tests with **check**.

<a id="summary"></a>
## Summary

|**Command**|**Description**|
|---|---|
|<a id="idx+cmds+all-by-group">**all-by-group**</a>|Report all tests (excluding those tagged **:retired** or **:reserved**) in *group* sorted by group.|
|<a id="idx+cmds+appchange">**appchange**</a>|Options: \[**-c**] *app1* \[*app2* ...]<br>Recheck all QA tests that appear to use the test application src/*app1* or *src/app2* or ... *${TMPDIR:-/tmp}/appcache* is a cache of mappings between test sequence numbers and uses, for all applications in *src/* \... **appchange** will build the cache if it is not already there, use **-c** to clear and rebuild the cache.|
|<a id="idx+cmds+bad-by-group">**bad-by-group**</a>|Use the *\*.out.bad* files to report failures by group.
|<a id="idx+cmds+check.app.ok">**check.app.ok**</a>|Options: *app*<br>When the test application *src/app.c* (or similar) has been changed, this script<br>(a) remakes the application and checks **make**(1) status, and<br>(b) finds all the tests that appear to run the *src/app* application and runs **check** for these tests.|
|<a id="idx+cmds+check-auto">**check-auto**</a>|Options: \[*seqno* ...]<br>Check that if a QA script uses **\_stop_auto_restart** for a (**systemd**) service, it also uses **\_restore_auto_restart** (preferably in \_cleanup()). If no *seqno* options are given then check all tests.|
|<a id="idx+cmds+check-flakey">**check-flakey**</a>|Options: \[*seqno* ...\]<br>Recheck failed tests and try to classify them as "flakey" if they pass now, or determine if the failure is "hard" (same **$seqno.out.bad**) or some other sort of non-deterministic failure. If no *seqno* options are given then check all tests with a **\*.out.bad*** file.|
|<a id="idx+cmds+check-group">**check-group**</a>|Options: *query*<br>Check the *group* file and test scripts for a specific *query* that is assumed to be **both** the name of a command that appears in the test scripts (or part of a command, e.g. **purify** in **\_setup_purify**) and the name of a group in the *group* file. Report differences, e.g. *command* appears in the *group* file for a specific test but is not apparently used in that test, or *command* is used in a specific test but is not included in the *group* file entry for that test.<br>There are some special cases to handle the pcp-foo commands, aliases and PMDAs ... refer to **check-group** for details.<br>Special control lines like:<br>`# check-group-include: group ...`<br>`# check-group-exclude: group ...`<br>may be embedded in test scripts to over-ride the heuristics used by **check-group**.|
|<a id="idx+cmds+check-pdu-coverage">**check-pdu-coverage**</a>|Check that PDU-related QA apps in *src* provide full coverage of all current PDU types.|
|<a id="idx+cmds+check-setup">**check-setup**</a>|Check QA environment is as expected. Documented in *README* but not used otherwise.|
|<a id="idx+cmds+check-vars">**check-vars**</a>|Check shell variables across the *common\** "include" files and the scripts used to run and manage QA. For the most part, the *common\** files should use a "\_\_" prefix for shell variables\[2] to insulate them from the use of arbitrarily name shell variables in the QA tests themselves (all of which "source" multiple of the *common\** files). **check-vars** also includes some exceptions which are a useful cross-reference.|
|<a id="idx+cmds+cull-pmlogger-config">**cull-pmlogger-config**</a>|Cull he default **pmlogger**(1) configuration (**$PCP_VAR_DIR***/config/pmlogger/config.default) *to remove any **zeroconf** proc metric logging that threatens to fill the filesystem on small QA machines.|
|<a id="idx+cmds+daily-cleanup">**daily-cleanup**</a>|Run from **check**, this script will try to make sure the **pmlogger_daily**(1) work has really been done; this is important for QA VMs that are only booted for QA and tend to miss the nightly **pmlogger_daily**(1) run.|
|<a id="idx+cmds+find-app">**find-app**</a>|Options: *app*<br>Find and report tests that use a QA application *src/app*.|
|<a id="idx+cmds+find-bound">**find-bound**</a>|Options: *archive* *timestamp* *metric* \[*instance*]<br>Scan *archive* for values of *metric* (optionally constrained to the one *instance*) within the interval *timestamp* (in the format HH:MM:SS, as per **pmlogdump**(1) and assuming a timezone as per **-z**).|
|<a id="idx+cmds+find-metric">**find-metric**</a>|Options: \[**-a**\|**-h**] *pattern* ...<br>Search for metrics with name or metadata that matches *pattern*. With **-h** interrogate the local **pmcd**(1), else with **-a** (the default) search all the QA archives in the directories *archive* and *tmparchive*.<br>Multiple pattern arguments are treated as a disjunction in the search which uses **grep**(1) style regular expressions. Metadata matches are against the **pminfo**(1) **-d** output for the type, instance domain, semantics, and units.|
|<a id="idx+cmds+flakey-summary">**flakey-summary**</a>|Assuming the output from **check-flakey** has been kept for multiple QA runs across multiple hosts and saved in a file called *flakey*, this script will summarize the test failure classifications.|
|<a id="idx+cmds+getpmcdhosts">**getpmcdhosts**</a>|Options: lots of them<br>Find a remote host matching a selection criteria based on hardware, operating system, installed PMDA, primary logger running, etc. Use<br>`$ getpmcdhosts -?`<br>to see all options.|
|<a id="idx+cmds+grind">**grind**</a>|Options: *seqno* \[...]<br>Run select test(s) in an loop until one of them fails and produces a **.out.bad** file. Stop with Ctl-C or for a more orderly end after the current iteration<br>`$ touch grind.stop`|
|<a id="idx+cmds+grind-pmda">**grind-pmda**</a>|Options: *pmda* *seqno* \[...]<br> Exercise the *pmda* PMDA by running the PMDA's **Install** script, then using **check** to run all the selected tests, checking that the PMDA is still installed, running the PMDA's **Remove** script, then running the selected tests again and checking that the PMDA is still **not** installed.
|<a id="idx+cmds+group-stats">**group-stats**</a>|Report test frequency by group, and report any group name anomalies.
|<a id="idx+cmds+mk.localconfig">**mk.localconfig**</a>|Recreate the *localconfig* file that provides the platform and PCP version information and the *src/localconfig.h* file that can be used by C programs in the *src* directory.
|**mk.logfarm**|See the [**mk.logfarm**](#mk.logfarm-script) section.
|<a id="idx+cmds+mk.pcpversion">**mk.pcpversion**</a>|REMOVE NOT USED TODO
|**mk.qa_hosts**|See the [**mk.qa_hosts**](#mk.qa_hosts-script) section.
|<a id="idx+cmds+mk.variant">**mk.variant**</a>|TOO HARD TODO
|**new**|See the [**new**](#new-script) section.
|<a id="idx+cmds+new-dup">**new-dup**</a>|
|<a id="idx+cmds+new-grind">**new-grind**</a>|
|<a id="idx+cmds+new-seqs">**new-seqs**</a>|
|<a id="idx+cmds+really-retire">**really-retire**</a>|
|<a id="idx+cmds+recheck">**recheck**</a>|
|<a id="idx+cmds+remake">**remake**</a>|
|<a id="idx+cmds+sameas">**sameas**</a>|
|<a id="idx+cmds+show-me">**show-me**</a>|
|<a id="idx+cmds+var-use">**var-use**</a>|

<br>
\[2] If all shells supported the **local** keyword for variables we could use that, but that's not the case across all the platforms PCP runs on, so the "\_\_" prefix model is a weak substitute for proper variable scoping.

<a id="mk.logfarm-script"></a>
## 10.1 <a id="idx+cmds+mk.logfarm">**mk.logfarm**</a> script

Usage: **mk.logfarm** \[**-c** *config*] *rootdir*

The **mk.logfarm** script creates a forest of archives suitable for
use with **pmlogger_daily**(1) in tests.

The forest is rooted at the directory *rootdir* which must exist.  Most
often this would be **$tmp**.

A default configuration is hard-wired, but an alternative is read from
*config* if **-c** is specified.

Each line in configuration contains 3 fields:

- hostname
- source archive basename, typically one of the archives in the **archives** directory
- destination archive basename, usually in one of the formats *YYYYMMDD*, *YYDDMM.HH.MM* or *YYDDMM.HH.MM-NN*.

Destination archives are copied with **pmlogcp**(1), the hostname is changed with **pmlogrewrite**(1) and if the destination has one of the datestamp
formats, then **src/timeshift** is used to rewrite all the timestamps
in the archive relative to the date and time in the archive's basename.

A part of the default configuration is as follows:
```
thishost        archives/foo+   20011005
thishost        archives/foo+   20011006.00.10
thishost        archives/foo+   20011007
otherhost       archives/ok-foo 20011002.00.10
otherhost       archives/ok-foo 20011002.00.10-00
```
<a id="mk.qa_hosts-script"></a>
## 10.2 <a id="idx+cmds+mk.qa_hosts">**mk.qa_hosts**</a> script

The **mk.qa_hosts** script makes the
The process uses the domain name for
the current host to choose a set of hosts that can be considered
when running distributed tests, e.g. **pmlogger**(1) locally and
**pmcd**(1) on a remote host.  Anyone wishing to do this sort of
testing (it does not happen in the github CI and QA actions) will
need to figure out how to append control lines in the
*qa_hosts.primary* file.

**mk.qa_hosts** is run from *GNUmakefile* so once created, *qa_hosts*
will tend to hang around.

<a id="using-valgrind"></a>
# 11 Using valgrind

TODO. Suppressions via valgrind-suppress or
valgrind-suppress-\<version\> or insitu.

<a id="using-helgrind"></a>
# 12 Using helgrind

TODO. helgrind-suppress

<a id="common-and-common.-files"></a>
# 13 <a id="idx+files+common">*common*</a> and <a id="idx+files+common.star">*common.\**</a> files

TODO brief description in general terms, esp adding common.foo

<a id="admin-scripts"></a>
# 14 Admin scripts

TODO

<a id="selinux-considerations"></a>
# 15 Selinux considerations

TODO pcp-testsuite.fc, pcp-testsuite.if and pcp-testsuite.te

TODO change and install

<a id="package-lists"></a>
# 16 Package lists

package-list dir

other-packages/manifest et al

**admin/list-packages** ... -c ... -m ... -n ... -v ...

<a id="dealing-with-the-known-unknowns"></a>
# 17 Dealing with the Known Unknowns

If tests are dealing with time intervals in terms of "today" or
"yesterday" or "4 hours ago", then running the test in the region of
midnight can be problematic. Similarly New Year's Eve is a time where
"this year" can change quite quickly.

More subtle are the points where daylight saving might start or stop,
leaving the system clock running but wallclock time suddenly misses an
hour or runs the same hour twice.

When this is makes a test non-deterministic, the defensive mechanisms
are to either use an appropriate guard with **\_notrun** or add the test
to the *triaged* file.

<a id="initial-setup-appendix"></a>
# Initial Setup Appendix

TODO - incorporate README info

<!--
.\" control lines for scripts/man-spell -- need to fake troff comment here
.\" +ok+ _restore_auto_restart _stop_auto_restart _setup_purify
.\" +ok+ PCP_AWK_PROG getpmcdhosts localconfig pcpversion
.\" +ok+ tmparchive wallclock datestamp testsuite timeshift not_in_ci
.\" +ok+ appchange otherhost qa_hosts valgrind _cleanup PCP_star helgrind
.\" +ok+ seq_full zeroconf thishost appcache precheck _service
.\" +ok+ selinux Selinux _notrun logfarm rootdir sameas idxctl github
.\" +ok+ flakey TMPDIR insitu notrun seqno _exit mkdir funcs
.\" +ok+ nbsp cd'd cd's seqs libc cmds TODO pdu idx seq dir cmd tmp
.\" +ok+ VMs src Ctl dup qa rc HH mk al VM
.\" +ok+ br {from <br>}
.\" +ok+ fc te {selinux file suffixes}
-->

<!--idxctl
General Index|Commands and Scripts|Shell Functions|Shell Variables|Files
!|cmds|funcs|vars|files
-->
<a id="index"></a>
# Index

|**Commands and Scripts**|**Shell Functions ...**|**Shell Functions ...**|**Shell Functions ...**|
|---|---|---|---|
|[all-by-group](#idx+cmds+all-by-group)|[\_check_64bit_platform](#idx+funcs+_check_64bit_platform)|[\_host_to_ipv6addrs](#idx+funcs+_host_to_ipv6addrs)|[\_try_pmlc](#idx+funcs+_try_pmlc)|
|[appchange](#idx+cmds+appchange)|[\_check_agent](#idx+funcs+_check_agent )|[\_ipaddr_to_host](#idx+funcs+_ipaddr_to_host)|[\_wait_for_pmcd](#idx+funcs+_wait_for_pmcd)|
|[bad-by-group](#idx+cmds+bad-by-group)|[\_check_core](#idx+funcs+_check_core)|[\_ipv6_localhost](#idx+funcs+_ipv6_localhost)|[\_wait_for_pmcd_stop](#idx+funcs+_wait_for_pmcd_stop)|
|[check](#idx+cmds+check)|[\_check_display](#idx+funcs+_check_display)|[\_libvirt_is_ok](#idx+funcs+_libvirt_is_ok)|[\_wait_for_pmie](#idx+funcs+_wait_for_pmie)|
|[check.app.ok](#idx+cmds+check.app.ok)|[\_check_freespace](#idx+funcs+_check_freespace)|[\_machine_id](#idx+funcs+_machine_id)|[\_wait_for_pmlogger](#idx+funcs+_wait_for_pmlogger)|
|[check-auto](#idx+cmds+check-auto)|[\_check_helgrind](#idx+funcs+_check_helgrind)|[\_make_helptext](#idx+funcs+_make_helptext)|[\_wait_for_pmproxy](#idx+funcs+_wait_for_pmproxy)|
|[check-flakey](#idx+cmds+check-flakey)|[\_check_job_scheduler](#idx+funcs+_check_job_scheduler)|[\_make_proc_stat](#idx+funcs+_make_proc_stat)|[\_wait_for_pmproxy_logfile](#idx+funcs+_wait_for_pmproxy_logfile)|
|[check-group](#idx+cmds+check-group)|[\_check_key_server](#idx+funcs+_check_key_server)|[\_need_metric](#idx+funcs+_need_metric)|[\_wait_for_pmproxy_metrics](#idx+funcs+_wait_for_pmproxy_metrics)|
|[check-pdu-coverage](#idx+cmds+check-pdu-coverage)|[\_check_key_server_ping](#idx+funcs+_check_key_server_ping)|[\_notrun](#idx+funcs+_notrun)|[\_wait_for_port](#idx+funcs+_wait_for_port)|
|[check-setup](#idx+cmds+check-setup)|[\_check_key_server_version](#idx+funcs+_check_key_server_version)|[\_path_readable](#idx+funcs+_path_readable)|[\_wait_pmcd_end](#idx+funcs+_wait_pmcd_end)|
|[check-vars](#idx+cmds+check-vars)|[\_check_key_server_version_offline](#idx+funcs+_check_key_server_version_offline)|[\_pid_in_container](#idx+funcs+_pid_in_container)|[\_wait_pmie_end](#idx+funcs+_wait_pmie_end)|
|[cull-pmlogger-config](#idx+cmds+cull-pmlogger-config)|[\_check_local_primary_archive](#idx+funcs+_check_local_primary_archive)|[\_prefer_valgrind](#idx+funcs+_prefer_valgrind)|[\_wait_pmlogctl](#idx+funcs+_wait_pmlogctl)|
|[daily-cleanup](#idx+cmds+daily-cleanup)|[\_check_metric](#idx+funcs+_check_metric)|[\_prepare_pmda](#idx+funcs+_prepare_pmda)|[\_wait_pmlogger_end](#idx+funcs+_wait_pmlogger_end)|
|[find-app](#idx+cmds+find-app)|[\_check_purify](#idx+funcs+_check_purify)|[\_prepare_pmda_install](#idx+funcs+_prepare_pmda_install)|[\_wait_pmproxy_end](#idx+funcs+_wait_pmproxy_end)|
|[find-bound](#idx+cmds+find-bound)|[\_check_search](#idx+funcs+_check_search)|[\_prepare_pmda_mmv](#idx+funcs+_prepare_pmda_mmv)|[\_wait_process_end](#idx+funcs+_wait_process_end)|
|[find-metric](#idx+cmds+find-metric)|[\_check_series](#idx+funcs+_check_series)|[\_private_pmcd](#idx+funcs+_private_pmcd)|[\_webapi_header_filter](#idx+funcs+_webapi_header_filter)|
|[flakey-summary](#idx+cmds+flakey-summary)|[\_check_valgrind](#idx+funcs+_check_valgrind)|[\_ps_tcp_port](#idx+funcs+_ps_tcp_port)|[\_webapi_response_filter](#idx+funcs+_webapi_response_filter)|
|[getpmcdhosts](#idx+cmds+getpmcdhosts)|[\_clean_display](#idx+funcs+_clean_display)|[\_pstree_all](#idx+funcs+_pstree_all)|[\_within_tolerance](#idx+funcs+_within_tolerance)|
|[grind](#idx+cmds+grind)|[\_cleanup_pmda](#idx+funcs+_cleanup_pmda)|[\_pstree_oneline](#idx+funcs+_pstree_oneline)|[\_writable_primary_logger](#idx+funcs+_writable_primary_logger)|
|[grind-pmda](#idx+cmds+grind-pmda)|[\_disable_loggers](#idx+funcs+_disable_loggers)|[\_remove_job_scheduler](#idx+funcs+_remove_job_scheduler)|**Shell Variables**|
|[group-stats](#idx+cmds+group-stats)|[\_domain_name](#idx+funcs+_domain_name)|[\_restore_auto_restart](#idx+funcs+_restore_auto_restart)|[$here](#idx+vars+here)|
|[mk.localconfig](#idx+cmds+mk.localconfig)|[\_exit](#idx+funcs+_exit)|[\_restore_config](#idx+funcs+_restore_config)|[$PCP_\*](#idx+vars+PCP_star)|
|[mk.logfarm](#idx+cmds+mk.logfarm)|[\_fail](#idx+funcs+_fail)|[\_restore_job_scheduler](#idx+funcs+_restore_job_scheduler)|[$PCPQA_SYSTEMD](#idx+vars+PCPQA_SYSTEMD)|
|[mk.pcpversion](#idx+cmds+mk.pcpversion)|[\_filesize](#idx+funcs+_filesize)|[\_restore_loggers](#idx+funcs+_restore_loggers)|[$seq](#idx+vars+seq)|
|[mk.qa_hosts](#idx+cmds+mk.qa_hosts)|[\_filter_helgrind](#idx+funcs+_filter_helgrind)|[\_restore_pmda_install](#idx+funcs+_restore_pmda_install)|[$seq_full](#idx+vars+seq_full)|
|[mk.variant](#idx+cmds+mk.variant)|[\_filter_init_distro](#idx+funcs+_filter_init_distro)|[\_restore_pmda_mmv](#idx+funcs+_restore_pmda_mmv)|[$status](#idx+vars+status)|
|[new](#idx+cmds+new)|[\__filter_pmlogger_log](#idx+funcs+__filter_pmlogger_log)|[\_restore_pmlogger_control](#idx+funcs+_restore_pmlogger_control)|[$sudo](#idx+vars+sudo)|
|[new-dup](#idx+cmds+new-dup)|[\_filter_purify](#idx+funcs+_filter_purify)|[\_restore_primary_logger](#idx+funcs+_restore_primary_logger)|[$tmp](#idx+vars+tmp)|
|[new-grind](#idx+cmds+new-grind)|[\_filter_valgrind](#idx+funcs+_filter_valgrind)|[\_run_helgrind](#idx+funcs+_run_helgrind)|**Files**|
|[new-seqs](#idx+cmds+new-seqs)|[\_find_free_port](#idx+funcs+_find_free_port)|[\_run_purify](#idx+funcs+_run_purify)|[$seq_full](#idx+files+seq_full)|
|[really-retire](#idx+cmds+really-retire)|[\_find_key_server_modules](#idx+funcs+_find_key_server_modules)|[\_run_valgrind](#idx+funcs+_run_valgrind)|[check.log](#idx+files+check.log)|
|[recheck](#idx+cmds+recheck)|[\_find_key_server_name](#idx+funcs+_find_key_server_name)|[\_save_config](#idx+funcs+_save_config)|[check.time](#idx+files+check.time)|
|[remake](#idx+cmds+remake)|[\_find_key_server_search](#idx+funcs+_find_key_server_search)|[\_service](#idx+funcs+_service)|[common](#idx+files+common)|
|[sameas](#idx+cmds+sameas)|[\_get_config](#idx+funcs+_get_config)|[\_set_dsosuffix](#idx+funcs+_set_dsosuffix)|[common.\*](#idx+files+common.star)|
|[show-me](#idx+cmds+show-me)|[\_get_endian](#idx+funcs+_get_endian)|[\_setup_purify](#idx+funcs+_setup_purify)|[group](#idx+files+group)|
|[var-use](#idx+cmds+var-use)|[\_get_fqdn](#idx+funcs+_get_fqdn)|[\_sighup_pmcd](#idx+funcs+_sighup_pmcd)|[localconfig](#idx+files+localconfig)|
|**Shell Functions**|[\_get_libpcp_config](#idx+funcs+_get_libpcp_config)|[\_start_up_pmlogger](#idx+funcs+_start_up_pmlogger)|[qa_hosts](#idx+files+qa_hosts)|
|[\_all_hostnames](#idx+funcs+_all_hostnames)|[\_get_port](#idx+funcs+_get_port)|[\_stop_auto_restart](#idx+funcs+_stop_auto_restart)|[qa_hosts.primary](#idx+files+qa_hosts.primary)|
|[\_all_ipaddrs](#idx+funcs+_all_ipaddrs)|[\_get_primary_logger_pid](#idx+funcs+_get_primary_logger_pid)|[\_systemctl_status](#idx+funcs+_systemctl_status)|[triaged](#idx+files+triaged)|
|[\_arch_start](#idx+funcs+_arch_start)|[\_get_word_size](#idx+funcs+_get_word_size)|[\_triage_pmcd](#idx+funcs+_triage_pmcd)|
|[\_avail_metric](#idx+funcs+_avail_metric)|[\_host_to_fqdn](#idx+funcs+_host_to_fqdn)|[\__triage_systemd](#idx+funcs+__triage_systemd)|
|[\_change_config](#idx+funcs+_change_config)|[\_host_to_ipaddr](#idx+funcs+_host_to_ipaddr)|[\_triage_wait_point](#idx+funcs+_triage_wait_point)|
