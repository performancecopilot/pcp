![Alt text](./logo.png)
# PCP QA Cookbook

# Table of contents
<br>[1 Preamble](#preamble)
<br>[2 The basic model](#the-basic-model)
<br>[3 Creating a new test](#creating-a-new-test)
<br>[4 Common shell variables](#common-shell-variables)
<br>[5 Coding style suggestions for tests](#coding-style-suggestions-for-tests)
<br>&nbsp;&nbsp;&nbsp;[5.1 General principles](#general-principles)
<br>&nbsp;&nbsp;&nbsp;[5.2 Take control of stdout and stderr](#take-control-of-stdout-and-stderr)
<br>&nbsp;&nbsp;&nbsp;[5.3 $seq.full file suggestions](#$seq.full-file-suggestions)
<br>[6 Shell functions from *common.check*](#shell-functions-from-common.check)
<br>&nbsp;&nbsp;&nbsp;[6.1 \_notrun](#_notrun)
<br>&nbsp;&nbsp;&nbsp;[6.2 \_service](#_service)
<br>&nbsp;&nbsp;&nbsp;[6.3 \_exit](#_exit)
<br>[7 Shell functions from *common.filter*](#shell-functions-from-common.filter)
<br>[8 *group* file](#group-file)
<br>[9 check script](#check-script)
<br>&nbsp;&nbsp;&nbsp;[9.1 Command line options](#command-line-options)
<br>&nbsp;&nbsp;&nbsp;[9.2 *check.log* file](#check.log-file)
<br>&nbsp;&nbsp;&nbsp;[9.3 *check.time* file](#check.time-file)
<br>&nbsp;&nbsp;&nbsp;[9.4 *qa_hosts.primary *and *qa_hosts* files](#qa_hosts.primary-and-qa_hosts-files)
<br>[10 check.callback script](#check.callback-script)
<br>[11 *triaged* file](#triaged-file)
<br>[12 Other helper scripts](#other-helper-scripts)
<br>[13 Using valgrind](#using-valgrind)
<br>[14 Using helgrind](#using-helgrind)
<br>[15 *common* and *common\** files](#common-and-common-files)
<br>[16 Admin scripts](#admin-scripts)
<br>[17 Selinux considerations](#selinux-considerations)
<br>[18 Package lists](#package-lists)
<br>[19 Dealing with the Known Unknowns](#dealing-with-the-known-unknowns)
<br>[Initial Setup Appendix](#initial-setup-appendix)

<a name="preamble"></a>
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
000 to 999 and then 1000 <sup>1</sup>... For shell usage the "glob" pattern
**[0-9]\*[0-9]** does a reasonable job of matching all test scripts.

Where these notes need to refer to a specific test, we\'ll use **$seq
**to mean \"the test you\'re currently interested in\", so we\'re
assuming you\'ve already done something like:  
`$ seq=1234`

Commands (and their options), scripts, environment variables, shell
variables and shell procedures appear in **bold** case. File names
appear in *italic* case. Code snippets and example commands appear
indented and in `fixed width` font.

1. The unfortunate mix of 3-digit and 4-digit test numbers is a
historical accident; when we started, no one could have imagined
that we'd ever have more than a thousand test scripts.

<a name="the-basic-model"></a>
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

<a name="creating-a-new-test"></a>
# 3 Creating a new test

Always use **new** to create a skeletal test. In addition to creating
the skeletal test, this will **git** **add** the new test and the
**.out** file, and update the *group* file, so when you\'ve finished the
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

<a name="common-shell-variables"></a>
# 4 Common shell variables

The common preamble in every test script source some *common\** scripts
and the following shell variables that may be used in your test script.

|Variable |Description                                                                                                                                                                                                                                                                                                                                                                                                                                            |
|---------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
|\* |Everything from **$PCP_DIR***/etc/pcp.conf* is placed in the environment by  calling **$PCP_DIR***/etc/pcp.env* from *common.rc*, so for example **$PCP_LOG_DIR** is  always defined and **$PCP_AWK_PROG** should be used instead of **awk**.|
|**$here**|Current directory tests are run from. Most useful after a test  script **cd**'s someplace else and you need to **cd** back, or reference  a file back in the starting directory.|
|**$seq**|The sequence number of the current test.|
|**$seq_full**|Proper pathname to the test's *.full* file. Always use this in preference to **$seq**.*full* because **$seq_full** works no matter where the test script might have **cd**'d to.|
|**$status**| Exit status for the test script.|
|**$sudo**|Proper invocation of **sudo**(1) that includes any per-platform additional command line options.|
|**$tmp**|Unique prefix for temporary files or directory. Use **$tmp.foo** or `$ mkdir $tmp` and then use **$tmp/foo** or both. The standard **trap** cleanup in each test will remove all these files and directories automatically when the test finishes, so save anything useful to **$seq_full**.|

<a name="coding-style-suggestions-for-tests"></a>
# 5 Coding style suggestions for tests

<a name="general-principles"></a>
## 5.1 General principles

"Good" QA tests are ones that typically:

-   are focused on one area of functionality or previous regression
    (complex tests are more likely to pass in multiple subtests but
    failing a single subtest makes the whole test fail, and complex
    tests are harder to debug)
-   run quickly -- the entire QA suite already takes a long time to run
-   are resilient to platform and distribution changes
-   don't check something that's already covered in another test
-   when exercising complex parts of core PCP functionality we'd like to
    see both a non-valgrind and a valgrind version of the test (see
    [**new**](#new) and [**new-grind**](#new-grind) below).

TODO

<a name="take-control-of-stdout-and-stderr"></a>
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

<a name="$seq.full-file-suggestions"></a>
## 5.3 $seq.full file suggestions

Assume your test is going to fail at some point, so be defensive up
front. In particular ensure that your test appends[^2] the following
sort of information to **$seq_full**:

-   values of critical shell variables that are not hard-coded, e.g.
    remote host, port being used for testing
-   unfiltered output (before it goes to the **.out** file) especially
    if the filtering is complex or aggressive to meet determinism
    requirements
-   on error paths, output from commands that might help explain the
    error cause
-   context that helps match up test cases in the script with the output
    in both the **.out** and **.full** files, e.g. this is a common
    pattern:<br>`$ echo "--- subtest foobar ---" | tee -a $seq_full`
-   log files that are unsuitable for inclusion in **.out **

<a name="shell-functions-from-common.check"></a>
# 6 Shell functions from *common.check*

<a name="_notrun"></a>
## 6.1 \_notrun

<a name="_service"></a>
## 6.2 \_service

<a name="_exit"></a>
## 6.3 \_exit

TODO -- more of these

<a name="shell-functions-from-common.filter"></a>
# 7 Shell functions from *common.filter*

TODO

<a name="group-file"></a>
# 8 *group* file

TODO


<a name="check-script"></a>
# 9 check script

TODO

<a name="command-line-options"></a>
## 9.1 Command line options

<a name="check.log-file"></a>
## 9.2 *check.log* file

Historical record of each execution of **check**, reporting what tests
were run, notrun, passed, triaged, failed, etc.

<a name="check.time-file"></a>
## 9.3 *check.time* file

Elapsed time for last successful execution of each test run by
**check**.

<a name="qa_hosts.primary-and-qa_hosts-files"></a>
## 9.4 *qa_hosts.primary *and *qa_hosts* files

TODO

<a name="check.callback-script"></a>
# 10 check.callback script

If **check.callback** exists and is executable, then it will be run from
**check** both before and after each test case is completed. The
optional first argument is --**precheck **for the before call, and the
last argument is the test *seqno*. This provides a hook for testing
broader classes of failures that could be triggered from (but not
necessarily tested by) all tests.

An example script is provided in **check.callback.sample** and this can
either be used as a template for building a customized
**check.callback**, or more simply used as is:\
* $ ln -s check.callback.sample check.callback*

Based on **check.callback.sample **the sort of things tested here might
include:

-   Pre and post capture of AVCs to detect new AVCs triggered by a
    specific test.
-   Did **pmlogger_daily** get run as expected?
-   Is **pmcd** healthy? This is delegated to **./941** with
    **\--check**.
-   Is **pmlogger** healthy? This is delegated to **./870** with
    **--check**.
-   Are all of the configured PMDAs still alive?
-   Has the Performance Metrics Namespace (PMNS) been trashed? This is
    delegated to **./1190** with **--check**.
-   Are there any PCP configuration files that contain text to indicate
    they have been modified by a QA test, as opposed to the version
    installed from packages.

<a name="triaged-file"></a>
# 11 *triaged* file

TODO
<a name="other-helper-scripts"></a>
# 12 Other helper scripts

There are a large number of shell scripts in the QA directory that are
intended for common QA development and triage tasks beyond simply
running tests with **check**.
<br>

|**Command**     |**Description**                                                                                       |
|----------------|------------------------------------------------------------------------------------------------------|
|**all-by-group**|Report all tests (excluding those tagged **:retired** or **:reserved**) in *group* sorted by group.|
|**appchange**|Options: [**-c**\] *app1* [*app2* ...\]<br>Recheck all QA tests that appear to use the test application src/*app1* or *src/app2* or ... *${TMPDIR:-/tmp}/appcache* is a cache of mappings between test sequence numbers and uses, for all applications in *src/* \... **appchange** will build the cache if it is not already there, use **-c** to clear and rebuild the cache.|                                                                                  
|**bad-by-group**|Use the *\*.out.bad* files to report failures by group.
|**check.app.ok**|Options: *app*<br>When the test application *src/app.c* (or similar) has been changed, this script<br>(a) remakes the application and checks **make**(1) status, and<br>(b) finds all the tests that appear to run the *src/app* application and runs **check** for these tests.|
|**check-auto**|Options: [*seqno* ...\]<br>Check that if a QA script uses **\_stop_auto_restart** for a (**systemd**) service, it also uses **\_restore_auto_restart** (preferably in \_cleanup()). If no *seqno* options are given then check all tests.|
|**check-flakey**|Options: [*seqno* ...\]<br>Recheck failed tests and try to classify them as "flakey" if they pass  now, or determine if the failure is "hard" (same **$seqno.out.bad**) or  some other sort of non-deterministic failure. If no *seqno* options  are given then check all tests with a **\*.out.bad*** file.|
|**check-group**|Options: *command*<br>Check the *group* file and test scripts for a specific *command*  that is assumed to be *both* the name of a command that appears in the  test scripts (or part of a command, e.g. **purify** in **\_setup_purify**)  and the name of a group in the *group* file. Report differences,  e.g. *command* appears in the *group* file for a specific test but  is not apparently used in that test, or *command* is used in a  specific test but is not included in the *group* file entry for that  test.<br>There are some special cases to handle the pcp-foo commands, aliases  and PMDAs ... refer to **check-group** for details.<br>Special control lines like:<br>`# check-group-include: group ...`<br>`# check-group-exclude: group ...`<br>may be embedded in test scripts to over-ride the heuristics used by  **check-group**.|
|**check-pdu-coverage**|Check that PDU-related QA apps in *src* provide full coverage of all current PDU types.|
|**check-setup**|Check QA environment is as expected. Documented in *README* but not used otherwise.|
|**check-vars**|Check shell variables across the *common\** \"include\" files and the scripts used to run and manage QA. For the most part, the *common\** files should use a \"\_\_\" prefix for shell variables[^3] to insulate them from the use of arbitrarily name shell variables in the QA tests themselves (all of which \"source\" multiple of the *common\** files). **check-vars** also includes some exceptions which are a useful cross-reference.|
|**cull-pmlogger-config**|Cull he default **pmlogger**(1) configuration (**$PCP_VAR_DIR***/config/pmlogger/config.default) *to remove any **zeroconf** proc metric logging that threatens to fill the filesystem on small QA machines.|
|**daily-cleanup**|Run from **check**, this script will try to make sure the **pmlogger_daily**(1) work has really been done; this is important for QA VMs that are only booted for QA and tend to miss the nightly **pmlogger_daily**(1) run.|
|**find-app**|Options: *app*<br>Find and report tests that use a QA application *src/app*.|
|**find-bound**|Options: *archive* *timestamp* *metric* [*instance*\]<br>Scan *archive* for values of *metric* (optionally constrained to the one *instance*) within the interval *timestamp* (in the format HH:MM:SS, as per **pmlogdump**(1) and assuming a timezone as per **-z**).|
|**find-metric**|Options: [**-a**\|**-h**\] *pattern* ...<br>Search for metrics with name or metadata that matches *pattern*. With **-h** interrogate the local **pmcd**(1), else with **-a** (the default) search all the QA archives in the directories *archive* and *tmparchive*.<br>Multiple pattern arguments are disjuncted in the search which uses **grep**(1) style regular expressions. Metadata matches are against the **pminfo**(1) **-d** output for the type, instance domain, semantics, and units.|
|**flakey-summary**|Assuming the output from **check-flakey** has been kept for multiple QA runs across multiple hosts and saved in a file called *flakey*, this script will summarize the test failure classifications.|
|**getpmcdhosts**|Options: lots of them<br>Find a remote host matching a selection criteria based on hardware, operating system, installed PMDA, primary logger running, etc. Use<br>`$ getpmcdhosts -?`<br>to see all options.|
|**grind**|Options: *seqno* [...\]<br>Run select test(s) in an loop until one of them fails and produces a **.out.bad** file. Stop with Ctl-C or for a more orderly end after the current iteration<br>`$ touch grind.stop`|
|**grind-pmda**|
|**group-stats**|
|**mk.localconfig**|localconfig
|**mk.logfarm**|
|**mk.pcpversion**|
|**mk.qa_hosts**|
|**mk.variant**|
|**new<a name="new"></a>**|
|**new-dup**|
|**new-grind<a name="new-grind"></a>**|
|**new-seqs**|
|**really-retire**|
|**recheck**|
|**remake**|
|**sameas**|
|**show-me**|
|**var-use**|

<a name="using-valgrind"></a>
# 13 Using valgrind

TODO. Suppressions via valgrind-suppress or
valgrind-suppress-\<version\> or insitu.

<a name="using-helgrind"></a>
# 14 Using helgrind

TODO. helgrind-suppress

<a name="common-and-common-files"></a>
# 15 *common* and *common\** files

TODO [brief description in general terms, esp adding common.foo\]

<a name="admin-scripts"></a>
# 16 Admin scripts

TODO

<a name="selinux-considerations"></a>
# 17 Selinux considerations

TODO pcp-testsuite.fc, pcp-testsuite.if and pcp-testsuite.te

TODO change and install

<a name="package-lists"></a>
# 18 Package lists

package-list dir

other-packages/manifest et al

**admin/list-packages** ... -c ... -m ... -n ... -v ...

<a name="dealing-with-the-known-unknowns"></a>
# 19 Dealing with the Known Unknowns

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

<a name="initial-setup-appendix"></a>
# Initial Setup Appendix

TODO

[^2]: The common preamble for all tests will ensure **$seq_full** is
    removed at the start of each test, so you can safely use constructs
    like:\
    $ echo ... \>\>$seq_full\
    $ cmd ... \| tee -a $seq_full \| \...

[^3]: If all shells supported the **local** keyword for variables we
    could use that, but that's not the case across all the platforms PCP
    runs on, so the "\_\_" prefix model is a weak substitute for proper
    variable scoping.
