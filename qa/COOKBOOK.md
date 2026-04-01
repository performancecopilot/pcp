# PCP QA Cookbook
![Alt text](../images/pcplogo-80.png)

# Table of contents
<br>[1 Preamble](#preamble)
<br>[2 The basic model](#the-basic-model)
<br>[3 Creating a new test](#creating-a-new-test)
<br>&nbsp;&nbsp;&nbsp;[3.1 The **new** script](#the-new-script)
<br>&nbsp;&nbsp;&nbsp;[3.2 Dealing with the Known Unknowns](#dealing-with-the-known-unknowns)
<br>[4 **check** script](#check-script)
<br>&nbsp;&nbsp;&nbsp;[4.1 **check** setup](#check-setup)
<br>&nbsp;&nbsp;&nbsp;[4.2 command line options for **check** ](#command-line-options-for-check-)
<br>&nbsp;&nbsp;&nbsp;[4.3 **check.callback** script](#check.callback-script)
<br>&nbsp;&nbsp;&nbsp;[4.4 *check.log* file](#check.log-file)
<br>&nbsp;&nbsp;&nbsp;[4.5 *check.time* file](#check.time-file)
<br>&nbsp;&nbsp;&nbsp;[4.6 *qa_hosts.primary* and *qa_hosts* files](#qahosts.primary-and-qahosts-files)
<br>[5 **show-me** script](#show-me-script)
<br>[6 Common shell variables](#common-shell-variables)
<br>[7 Coding style suggestions for tests](#coding-style-suggestions-for-tests)
<br>&nbsp;&nbsp;&nbsp;[7.1 Take control of stdout and stderr](#take-control-of-stdout-and-stderr)
<br>&nbsp;&nbsp;&nbsp;[7.2 **$seq_full** file suggestions](#seqfull-file-suggestions)
<br>[8 Shell functions from *common.check*](#shell-functions-from-common.check)
<br>&nbsp;&nbsp;&nbsp;[8.1 **\_triage_wait_point**](#triagewaitpoint)
<br>[9 Shell functions from *common.filter*](#shell-functions-from-common.filter)
<br>[10 Control files](#control-files)
<br>&nbsp;&nbsp;&nbsp;[10.1 The *group* file](#the-group-file)
<br>&nbsp;&nbsp;&nbsp;[10.2 The *triaged* file](#the-triaged-file)
<br>&nbsp;&nbsp;&nbsp;[10.3 The *localconfig* file](#the-localconfig-file)
<br>[11 Helper scripts](#helper-scripts)
<br>&nbsp;&nbsp;&nbsp;[Summary](#summary)
<br>&nbsp;&nbsp;&nbsp;[11.1 **mk.logfarm** script](#mk.logfarm-script)
<br>&nbsp;&nbsp;&nbsp;[11.2 **mk.qa_hosts** script](#mk.qahosts-script)
<br>[12 qa subdirectories](#qa-subdirectories)
<br>&nbsp;&nbsp;&nbsp;[12.1 *src*](#src)
<br>&nbsp;&nbsp;&nbsp;[12.2 *archives*](#archives)
<br>&nbsp;&nbsp;&nbsp;[12.3 *tmparch*](#tmparch)
<br>&nbsp;&nbsp;&nbsp;[12.4 *pmdas*](#pmdas)
<br>&nbsp;&nbsp;&nbsp;[12.5 *admin*](#admin)
<br>[13 Using valgrind](#using-valgrind)
<br>[14 Using helgrind](#using-helgrind)
<br>[15 *common* and *common.\** files](#common-and-common.-files)
<br>[16 Selinux considerations](#selinux-considerations)
<br>[17 Package lists](#package-lists)
<br>[18 The last word](#the-last-word)
<br>[Initial Setup Appendix](#initial-setup-appendix)
<br>&nbsp;&nbsp;&nbsp;[**sudo** setup](#sudo-setup)
<br>&nbsp;&nbsp;&nbsp;[Distributed QA](#distributed-qa)
<br>&nbsp;&nbsp;&nbsp;[Firewall setup](#firewall-setup)
<br>&nbsp;&nbsp;&nbsp;[*common.config* files](#common.config-files)
<br>&nbsp;&nbsp;&nbsp;[Some special test cases](#some-special-test-cases)
<br>&nbsp;&nbsp;&nbsp;[Take it for a test drive](#take-it-for-a-test-drive)
<br>[PCP Acronyms Appendix](#pcp-acronyms-appendix)
<br>[Index](#index)

<a id="preamble"></a>
# 1 Preamble

These notes are designed to help with building, running and maintaining QA
(Quality Assurance) tests
for the Performance Co-Pilot ([PCP](#idx+pcp)) project
([www.pcp.io](https://www.pcp.io/) and
[https://github.com/performancecopilot/pcp](https://github.com/performancecopilot/pcp/)).

The PCP QA infrastructure is designed with a philosophy that aims to
exercise the PCP components in a context that is as close as possible to that
which an end-user would experience. For this reason, the PCP software to
be tested should be installed in the "usual" places, with the "usual"
permissions and communicate on the "usual" TCP/IP ports.

The PCP QA infrastructure does **not** execute PCP applications
like **pmcd**(1),
**pmlogger**(1), **pminfo**(1), **pmie**(1), **pmrep**(1), etc built in
the git tree,
rather they
need to have been already built, packaged and installed on the local system
prior to starting any QA.

Refer to the **Makepkgs** script in the top directory of the source
tree for a recipe
that may be used to build packages for a variety of platforms.

We assume you're a developer or tester, running, building or fixing PCP QA
tests, so you are operating in a git tree (not the
*/var/lib/pcp/testsuite* directory that is packaged and installed)
so you're using a non-root login and
let's assume that from the base of the git tree you've already done:<br>
`$ cd qa`

Since the "qa" directory is where all the QA action happens, scripts and
files in this cookbook that are not absolute paths are all relative to
the "qa" directory, so for example *src/app1* is the path *qa/src/app1*
relative to the base of the git tree.

If you're setting up the PCP QA infrastructure for a new machine or VM or container,
then refer to the [Initial Setup Appendix](#initial-setup-appendix) and the [Package lists](#package-lists) section in this document.

The PCP QA infrastructure exercises and tests aspects of the PCP
packaging, use of certain local accounts, interaction with system
daemons and services, a number of PCP-related system administrative
functions, e.g. to stop and start PCP services.
Some of these require "root" privileges, refer to the
[**sudo** setup](#sudo-setup) section below.

But this also means the QA tests may alter existing system configuration
files, and this introduces some risk, so PCP QA should not be run
on production systems. Historically we have used developer systems
and dedicated QA systems for running QA - VMs are
particularly well-suited to this task.

In addition to the base PCP package installation, the **sample** and **simple**
PMDAs need to be installed (however the QA infrastructure will take
care of this).

The phrase "test script" or simply "test" refers to one of the thousands of test scripts numbered
000 to 999 and then 1000 \[1]... For shell usage the "glob" pattern
**[0-9\]\*[0-9\]** does a reasonable job of matching all test scripts.

Where these notes need to refer to a specific test, we'll use **$seq**
to mean "the test you're currently interested in", so we're
assuming you've already done something like:<br>
`$ seq=1234`

For the components of the PCP QA infrastructure, commands (and their options), scripts, environment variables, shell
variables and shell procedures all appear in **bold** case. File names
appear in *italic* case. Snippets of code, shell scripts, configuration
file contents and examples appear
in `fixed width` font.

Annotations of the form **command**(1) are oblique pointers to the "man"
pages that are **not** part of the PCP QA infrastructure, although they
may well be PCP "man" pages, e.g. **pmcd**(1).

\[1] The unfortunate mix of 3-digit and 4-digit test numbers is a
historical accident; when we started, no one could have imagined that
we'd ever have more than a thousand test scripts!

<a id="the-basic-model"></a>
# 2 The basic model

Minimally each test consists of a shell script **$seq** and an expected output
file **$seq***.out*.

When run under the control of **check**, **$seq** is executed and the
output is captured and compared to **$seq***.out*. If the two outputs are
identical the test is deemed to have passed, else the (unexpected)
output is saved to **$seq***.out.bad*.

Central to this model is the fact that **$seq** must produce
deterministic output, independent of hostname, filesystem pathname
differences, date, locale, platform differences, variations in output
from non-PCP applications, etc. This is achieved by "filtering" command
output and log files to remove lines or fields that are not
deterministic, and replace variable text with constants.

The tests scripts are expected to exit with status 0, but may exit with
a non-zero status in cases of catastrophic failure, e.g. some service to
be exercised did not start correctly, so there is nothing to test.

As tests are developed and evolve, the [**remake**](#idx+cmds+remake)
script is used to
generate updated versions of **$seq***.out*.

To assist with test failure triage, all tests also generate a
**$seq***.full* file that contains additional diagnostics and unfiltered
output.

<a id="creating-a-new-test"></a>
# 3 Creating a new test

"Good" QA tests are ones that typically:

- are focused on one area of functionality or previous regression (complex tests are more likely to pass in multiple subtests but failing a single subtest makes the whole test fail, and complex tests are harder to debug)
- run quickly -- the entire QA suite already takes a long time to run
- are resilient to platform and distribution changes
- don't check something that's already covered in another test
- when exercising complex parts of core PCP functionality we'd like to see both a non-valgrind and a valgrind version of the test (see [**new**](#the-new-script) and [**new-grind**](#new-grind) below).

And "learning by example" is the well-trusted model that pre-dates AI ... there are thousands of
existing tests, so lots of worked examples for you to choose from.

Always use **new** to create a skeletal test. In addition to creating
the skeletal test, this will **git** **add** the new test and the
**.out** file, and update the *group* file, so when you've finished the
script development you need to (at least):

```bash
$ remake $seq
$ git add $seq $seq.out
$ git commit
```

additional **git** commands and possibly *GNUmakefile* changes will be
needed if your test needs any additional new files, e.g. a new source
program or script below in the [*qa/src*](#src) directory or a new archive in
the [*qa/archives*](#archives) directory.

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
- **-R** *seqno* use *seqno* that was previously tagged as **:reserved** in the *group* file
- **-v** be verbose

When you exit **$EDITOR** you'll be prompted for groups to associate
the new test with, unless one or more *group* was specified on the command line.

<a id="dealing-with-the-known-unknowns"></a>
## 3.2 Dealing with the Known Unknowns

If tests are dealing with time intervals in terms of "today" or
"yesterday" or "4 hours ago", then running the test in the region of
midnight can be problematic. Similarly New Year's Eve is a time where
"this year" can change quite quickly.

More subtle are the points where daylight saving might start or stop,
leaving the system clock running but wallclock time suddenly misses an
hour or runs the same hour twice.

When this is makes a test non-deterministic, the defensive mechanisms
are to either use an appropriate guard with [**\_notrun**](#idx+funcs+notrun) or add the test
to the [*triaged*](#the-triaged-file) file.

<a id="check-script"></a>
# 4 <a id="idx+cmds+check">**check**</a> script

The **check** script is responsible for running one or more tests and
determining their outcome as follows:
<br>

|**Outcome**|**Description**|
|---|---|
|**pass**|test ran to completion, exit status is 0 and output is identical to the corresponding **.out** file|
|**notrun**|test called [**_notrun**](#idx+funcs+notrun)|
|**callback**|same as **pass** but [**check.callback**](#check.callback-script) was run and detected a problem|
|**fail**|test exit status was not 0|
|**triaged**|special kind of **fail**, see [the *triaged* file](#the-triaged-file) section below|

The non-option command line arguments identify tests to
be run using one or more *seqno* or a range *seqno*-*seqno*.
Tests will be run in numeric test number order, after any duplicates
are removed.
Leading zeroes may be omitted, so all of the following are equivalent.

```bash
$ check 010 00? # assume the shell's glob will expand this
$ check 010 009 008 007 006 005 004 003 002 001
$ check 0-10 000
$ check 000 1 002 3 4-9 10
```

If no *seqno* is specified, the default is to select all tests
in the *group* file that are not tagged **:retired** or **:reserved**.

<a id="check-setup"></a>
## 4.1 **check** setup

Unless the **-q** option is given (see below), **check** will perform
the following tasks before any test is run:<br>

- run [**mk.localconfig**](#idx+cmds+mk.localconfig)
- ensure **pmcd**(1) is running with the **-T 3** option to enable PDU tracing
- ensure **pmcd**(1) is running with the **-C 512** option to enable 512 [PMAPI](#idx+pmapi) contexts
- ensure the **sample**, **sampledso** and **simple** [PMDAs](#idx+pmda) are installed and working
- ensure **pmcd**(1), **pmproxy**(1) and **pmlogger**(1) are all configured to allow remote connections
- ensure the primary **pmlogger**(1) instance is running
- if [Distributed QA](#distributed-qa) has been enabled, check that the PCP setup on the remote systems is OK
- run `make setup` which will run `make setup` in multiple subdirectories, but most importantly *src* (so the QA apps are made), *tmparch* (so the transient archives are present) and *pmdas* (so the QA [PMDAs](#idx+pmda) are up to date)

<a id="command-line-options-for-check-"></a>
## 4.2 command line options for **check** 

The parsing of command line arguments is a little Neanderthal, so best
practice is to separate options with whitespace.
<br>

|**Option**|**Description**|
|---|---|
|**-c**|Before and after check for selected configuration files to ensure they have not been modified.|
|**-C**|Enable color mode to highlight outcomes (assumes interactive execution).|
|**-CI**|When QA tests run in the github infrastructure for the CI or QA actions, there are some tests that will not ever pass. The **-CI** option is shorthand for "**-x x11 -x remote -x not_in_ci**" and also sets <a id="idx+vars+pcpqainci">**$PCPQA_IN_CI**</a> to *yes* so individual tests can make local decisions if they are running in this environment.|
|**-g** *group*|Include all of the tests from the group *group*.|
|**-l**|When differences need to be displayed, the default is to try and use a "graphical diff" tool if one can be found; the **-l** option forces simple old **diff**(1) to be used.|
|**-s**|"Sssh" mode, no differences.|
|**-n**|Show me the selected test numbers, don't run anything.|
|**-q**|"Quick" mode, don't execute the setup steps. |
|**-r**|Include tests tagged **:reserved** and **:retired**.|
|**-T**|Output begin and end timestamps as each test is run.|
|**-TT**|Update epoch begin and end timestamps in *check.timings*.|
|**-x** *group*|Exclude all of the tests from the group *group*.|
|**-X** *seqno*|Exclude the specified tests (*seqno* may be a single test number, or a comma separated list of test numbers or a range of test numbers).|

<a id="check.callback-script"></a>
## 4.3 **check.callback** script

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
- Are all of the configured [PMDAs](#idx+pmda) still alive?
- Has the [PMNS](#idx+pmns) been trashed? This is delegated to **./1190** with **--check**.
- Are there any PCP configuration files that contain text to indicate they have been modified by a QA test, as opposed to the version installed from packages.

<a id="check.log-file"></a>
## 4.4 <a id="idx+files+check.log">*check.log*</a> file

Historical record of each execution of **check**, reporting what tests
were run, notrun, passed, triaged, failed, etc.

<a id="check.time-file"></a>
## 4.5 <a id="idx+files+check.time">*check.time*</a> file

Elapsed time for last successful execution of each test run by
**check**.

<a id="qahosts.primary-and-qahosts-files"></a>
## 4.6 <a id="idx+files+qahosts.primary">*qa_hosts.primary*</a> and <a id="idx+files+qahosts">*qa_hosts*</a> files

Refer to the [**mk.qa_hosts**](#mk.qahosts-script) section.

<a id="show-me-script"></a>
# 5 <a id="idx+cmds+show-me">**show-me**</a> script

Usage: **show-me** \[**-g** *group*] \[**-l**] \[**-n**] \[**-x** *group*] \[seqno ...]

The **show-me** script is responsible for displaying the differences
between the actual output (**$seq***.out.bad*) and the expected output
(**$seq***.out*) for selected tests.

The command line options are:

|**Option**|**Description**|
|---|---|
|**-g** *group*|Select the failed tests from the group *group*.|
|**-l**|Simple **diff**(1), the default is to use a graphical diff tool if one can be found|
|**-n**|Show me, just report *seqno* for failing tests, no diffs|
|**-x** *group*|Exclude the failed tests from the group *group*.|

If no **-g** option and no *seqno* is specified on the command line,
**show-me** will process all the **.out.bad** files in the current
directory.

<a id="common-shell-variables"></a>
# 6 Common shell variables

The common preamble in every test script source some *common\** scripts
and the following shell variables that may be used in your test script.

|**Variable**|**Description**|
|---|---|
|<a id="idx+vars+pcpstar">**$PCP_\***</a>|Everything from **$PCP_DIR***/etc/pcp.conf* is placed in the environment by calling **$PCP_DIR***/etc/pcp.env* from *common.rc*, so for example **$PCP_LOG_DIR** is always defined and **$PCP_AWK_PROG** should be used instead of **awk**.|
|<a id="idx+vars+here">**$here**</a>|Current directory tests are run from. Most useful after a test script **cd**'s someplace else and you need to **cd** back, or reference a file back in the starting directory.|
|<a id="idx+vars+seq">**$seq**</a>|The sequence number of the current test.|
|<a id="idx+vars+seqfull">**$seq_full**</a>|Proper pathname to the test's *.full* file. Always use this in preference to **$seq**.*full* because **$seq_full** works no matter where the test script might have **cd**'d to.|
|<a id="idx+vars+status">**$status**</a>| Exit status for the test script.|
|<a id="idx+vars+sudo">**$sudo**</a>|Proper invocation of **sudo**(1) that includes any per-platform additional command line options.|
|<a id="idx+vars+tmp">**$tmp**</a>|Unique prefix for temporary files or directory. Use **$tmp***.foo* or `$ mkdir $tmp` and then use **$tmp***/foo* or both. The standard **trap** cleanup in each test will remove all these files and directories automatically when the test finishes, so save anything useful to **$seq_full**.|

<a id="coding-style-suggestions-for-tests"></a>
# 7 Coding style suggestions for tests

<a id="take-control-of-stdout-and-stderr"></a>
## 7.1 Take control of stdout and stderr

If an application used in a test may produce output on either stdout or
stderr or both, the test may need to take control to capture all the
output and prevent platform-specific or libc-version-specific buffer
flushing that makes the output non-deterministic.

Instead of

```bash
$ cmd | _filter
```

the test may need to do

```bash
$ cmd 2>&1 | _filter
```

or even

```bash
$ cmd >$tmp.out 2>$tmp.err
$ cat $tmp.err $tmp.out | _filter
```

<a id="seqfull-file-suggestions"></a>
## 7.2 <a id="idx+files+seqfull">**$seq_full**</a> file suggestions

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

```bash
$ echo ... >>$seq_full
$ cmd ... | tee -a $seq_full | ...
```

Remember that **$seq_full** translates to file **$seq***.full* (dot, not underscore) in the directory the **$seq** test is run from.

<a id="shell-functions-from-common.check"></a>
# 8 Shell functions from *common.check*

A large number of shell functions that are useful across
multiple test scripts are provided by *common.check* to assist with
test development and these are always available for tests created with
a standard preamble (as provided by the [**new**](#the-new-script) script.

In addition to defining the shell procedures (some of the more frequently
used ones are described in the table below), *common.check* also
handles:

- if necessary running [**mk.localconfig**](#idx+cmds+mk.localconfig)
- sourcing [*localconfig*](#idx+files+localconfig)
- setting <a id="idx+vars+pcpqasystemd">**$PCPQA_SYSTEMD**</a> to **yes** or **no** depending if services are controlled by **systemctl**(1) or not
<br>

|**Function**|**Description**|
|---|---|
|<a id="idx+funcs+allhostnames">**\_all_hostnames**</a>|TODO|
|<a id="idx+funcs+allipaddrs">**\_all_ipaddrs**</a>|TODO|
|<a id="idx+funcs+archstart">**\_arch_start**</a>|TODO|
|<a id="idx+funcs+availmetric">**\_avail_metric**</a>|TODO|
|<a id="idx+funcs+changeconfig">**\_change_config**</a>|TODO|
|<a id="idx+funcs+check64bitplatform">**\_check_64bit_platform**</a>|TODO|
|<a id="idx+funcs+checkagent">**\_check_agent**</a>|Usage: **\_check_agent** *pmda* \[*verbose*]<br>Checks that the *pmda* [PMDA](#idx+pmda) is installed and responding to metric requests. Returns 0 if all is well, else returns 1 and emits diagnostics on stdout to explain why. If *verbose* is **true** emit diagnostics independent of return value.|
|<a id="idx+funcs+checkcore">**\_check_core**</a>|TODO|
|<a id="idx+funcs+checkdisplay">**\_check_display**</a>|TODO|
|<a id="idx+funcs+checkfreespace">**\_check_freespace**</a>|Usage: **\_check_freespace** *need*<br> Returns 0 if there is more that *need* Mbytes of free space in the filesystem for the current working directory, else returns 1.|
|<a id="idx+funcs+checkjobscheduler">**\_check_job_scheduler**</a>|TODO|
|<a id="idx+funcs+checkkeyserver">**\_check_key_server**</a>|TODO|
|<a id="idx+funcs+checkkeyserverping">**\_check_key_server_ping**</a>|TODO|
|<a id="idx+funcs+checkkeyserverversion">**\_check_key_server_version**</a>|TODO|
|<a id="idx+funcs+checkkeyserverversionoffline">**\_check_key_server_version_offline**</a>|TODO|
|<a id="idx+funcs+checklocalprimaryarchive">**\_check_local_primary_archive**</a>|TODO|
|<a id="idx+funcs+checkmetric">**\_check_metric**</a>|TODO|
|<a id="idx+funcs+checkpurify">**\_check_purify**</a>|TODO|
|<a id="idx+funcs+checksearch">**\_check_search**</a>|TODO|
|<a id="idx+funcs+checkseries">**\_check_series**</a>|TODO|
|<a id="idx+funcs+cleandisplay">**\_clean_display**</a>|TODO|
|<a id="idx+funcs+cleanuppmda">**\_cleanup_pmda**</a>|Usage: **\_cleanup_pmda** *pmda* \[*install-config*]<br>Called at the end of a test to restore the state of the *pmda* [PMDA](#idx+pmda) to the state it was in when the companion function [**\_prepare_pmda**](#idx+funcs+preparepmda) was called. *install-config* is an optional input file for the PMDA's **Install** script to be used if the PMDA needs to be re-installed (default is */dev/tty*).|
|<a id="idx+funcs+disableloggers">**\_disable_loggers**</a>|TODO|
|<a id="idx+funcs+domainname">**\_domain_name**</a>|TODO|
|<a id="idx+funcs+exit">**\_exit**</a>|Usage: **\_exit** *status*<br>Set [$status](#idx+vars+status) to *status* and force test exit.|
|<a id="idx+funcs+fail">**\_fail**</a>|Usage: **\_fail** *message*<br>Emit *message* on stderr and force failure exit of test.|
|<a id="idx+funcs+filesize">**\_filesize**</a>|Usage: **\_filesize** *file*<br>Output the size of *file* in bytes on stdout.|
|<a id="idx+funcs+filterinitdistro">**\_filter_init_distro**</a>|TODO|
|<a id="idx+funcs+filterpurify">**\_filter_purify**</a>|TODO|
|<a id="idx+funcs+findfreeport">**\_find_free_port**</a>|TODO|
|<a id="idx+funcs+findkeyservermodules">**\_find_key_server_modules**</a>|TODO|
|<a id="idx+funcs+findkeyservername">**\_find_key_server_name**</a>|TODO|
|<a id="idx+funcs+findkeyserversearch">**\_find_key_server_search**</a>|TODO|
|<a id="idx+funcs+getconfig">**\_get_config**</a>|TODO|
|<a id="idx+funcs+getendian">**\_get_endian**</a>|TODO|
|<a id="idx+funcs+getfqdn">**\_get_fqdn**</a>|TODO|
|<a id="idx+funcs+getlibpcpconfig">**\_get_libpcp_config**</a>|TODO|
|<a id="idx+funcs+getport">**\_get_port**</a>|TODO|
|<a id="idx+funcs+getprimaryloggerpid">**\_get_primary_logger_pid**</a>|TODO|
|<a id="idx+funcs+getwordsize">**\_get_word_size**</a>|TODO|
|<a id="idx+funcs+hosttofqdn">**\_host_to_fqdn**</a>|TODO|
|<a id="idx+funcs+hosttoipaddr">**\_host_to_ipaddr**</a>|TODO|
|<a id="idx+funcs+hosttoipv6addrs">**\_host_to_ipv6addrs**</a>|TODO|
|<a id="idx+funcs+ipaddrtohost">**\_ipaddr_to_host**</a>|TODO|
|<a id="idx+funcs+ipv6localhost">**\_ipv6_localhost**</a>|TODO|
|<a id="idx+funcs+libvirtisok">**\_libvirt_is_ok**</a>|TODO|
|<a id="idx+funcs+machineid">**\_machine_id**</a>|TODO|
|<a id="idx+funcs+makehelptext">**\_make_helptext**</a>|TODO|
|<a id="idx+funcs+makeprocstat">**\_make_proc_stat**</a>|TODO|
|<a id="idx+funcs+needmetric">**\_need_metric**</a>|TODO|
|<a id="idx+funcs+notrun">**\_notrun**</a>|Usage: **\_notrun** *message*<br>Not all tests are expected to be able to run on all platforms. Reasons might include: won't work at all a certain operating system, application required by the test is not installed, metric required by the test is not available from **pmcd**(1), etc.<br>In these cases, the test should include a guard that captures the required precondition and call **\_notrun** with a helpful *message* if the guard fails. For example.<br>&nbsp;&nbsp;&nbsp;`which pmrep >/dev/null 2>&1 || _notrun "pmrep not installed"`|
|<a id="idx+funcs+pathreadable">**\_path_readable**</a>|TODO|
|<a id="idx+funcs+pidincontainer">**\_pid_in_container**</a>|TODO|
|<a id="idx+funcs+preparepmda">**\_prepare_pmda**</a>|Usage: **\_prepare_pmda** *pmda* \[*name*]<br>Called before any [PMDA](#idx+pmda) changes are made to nsure the state of the *pmda* PMDA will be restored at the end of the test when the companion function [**\_cleanup_pmda**](#idx+funcs+cleanuppmda) is called in **_cleanup**. *name* is the name of a metric in the [PMNS](#idx+pmns) that belongs to the *pmda* PMDA, so it can be used to probe the PMDA; if *name* is not provided, it defaults to *pmda*.|
|<a id="idx+funcs+preparepmdainstall">**\_prepare_pmda_install**</a>|TODO|
|<a id="idx+funcs+preparepmdammv">**\_prepare_pmda_mmv**</a>|TODO|
|<a id="idx+funcs+privatepmcd">**\_private_pmcd**</a>|TODO|
|<a id="idx+funcs+pstcpport">**\_ps_tcp_port**</a>|TODO|
|<a id="idx+funcs+pstreeall">**\_pstree_all**</a>|TODO|
|<a id="idx+funcs+pstreeoneline">**\_pstree_oneline**</a>|TODO|
|<a id="idx+funcs+removejobscheduler">**\_remove_job_scheduler**</a>|TODO|
|<a id="idx+funcs+restoreconfig">**\_restore_config**</a>|Usage: **\_restore_config** *target*<br> Reinstates a configuration file or directory (*target*) previously saved with **\_save_config**.|
|<a id="idx+funcs+restorejobscheduler">**\_restore_job_scheduler**</a>|TODO|
|<a id="idx+funcs+restoreloggers">**\_restore_loggers**</a>|TODO|
|<a id="idx+funcs+restorepmdainstall">**\_restore_pmda_install**</a>|TODO|
|<a id="idx+funcs+restorepmdammv">**\_restore_pmda_mmv**</a>|TODO|
|<a id="idx+funcs+restorepmloggercontrol">**\_restore_pmlogger_control**</a>|TODO|
|<a id="idx+funcs+restoreprimarylogger">**\_restore_primary_logger**</a>|TODO|
|<a id="idx+funcs+runpurify">**\_run_purify**</a>|TODO|
|<a id="idx+funcs+saveconfig">**\_save_config**</a>|Usage: **\_save_config** *target*<br>Save a configuration file or directory (*target*) with a name that uses [$seq](#idx+vars+seq) so that if a test aborts we know who was dinking with the configuration.<br>Operates in concert with **\_restore_config**.|
|<a id="idx+funcs+service">**\_service**</a>|Usage: **\_service** \[**-v**] *service* *action*<br>Controlling services like **pmcd**(1) or **pmlogger**(1) or ... may involve **init**(1) or **systemctl**(1) or something else. This complexity is hidden behind the **\_service** function which should be used whenever as test wants to control a PCP service.<br> Supported values for *service* are: **pmcd**, **pmlogger**, **pmproxy** **pmie**.<br>*action* is one of **stop**, **start** (may be no-op if already started) or **restart** (force stop if necessary, then start).<br>Use **-v** for more verbosity.|
|<a id="idx+funcs+setdsosuffix">**\_set_dsosuffix**</a>|TODO|
|<a id="idx+funcs+setuppurify">**\_setup_purify**</a>|TODO|
|<a id="idx+funcs+sighuppmcd">**\_sighup_pmcd**</a>|TODO|
|<a id="idx+funcs+startuppmlogger">**\_start_up_pmlogger**</a>|TODO|
|<a id="idx+funcs+stopautorestart">**\_stop_auto_restart**</a>|Usage: **\_stop_auto_restart** *service*<br>When testing error handling or timeout conditions for services it may be important to ensure the system does not try to restart a failed service (potentially leading to an hard loop of retry-fail-retry). **\_stop_auto_start** will change configuration to prevent restarting for *service* if the system supports this function.<br>Use <a id="idx+funcs+restoreautorestart">**\_restore_auto_restart**</a> with the same *service* to reinstate the configuration.|
|<a id="idx+funcs+systemctlstatus">**\_systemctl_status**</a>|TODO|
|<a id="idx+funcs+triagepmcd">**\_triage_pmcd**</a>|TODO|
|<a id="idx+funcs+triagewaitpoint">**\_triage_wait_point**</a>|See the [**\_triage_wait_point**](#triagewaitpoint) section below.|
|<a id="idx+funcs+trypmlc">**\_try_pmlc**</a>|TODO|
|<a id="idx+funcs+waitforpmcd">**\_wait_for_pmcd**</a>|TODO|
|<a id="idx+funcs+waitforpmcdstop">**\_wait_for_pmcd_stop**</a>|TODO|
|<a id="idx+funcs+waitforpmie">**\_wait_for_pmie**</a>|TODO|
|<a id="idx+funcs+waitforpmlogger">**\_wait_for_pmlogger**</a>|TODO|
|<a id="idx+funcs+waitforpmproxy">**\_wait_for_pmproxy**</a>|TODO|
|<a id="idx+funcs+waitforpmproxylogfile">**\_wait_for_pmproxy_logfile**</a>|TODO|
|<a id="idx+funcs+waitforpmproxymetrics">**\_wait_for_pmproxy_metrics**</a>|TODO|
|<a id="idx+funcs+waitforport">**\_wait_for_port**</a>|TODO|
|<a id="idx+funcs+waitpmcdend">**\_wait_pmcd_end**</a>|TODO|
|<a id="idx+funcs+waitpmieend">**\_wait_pmie_end**</a>|TODO|
|<a id="idx+funcs+waitpmlogctl">**\_wait_pmlogctl**</a>|TODO|
|<a id="idx+funcs+waitpmloggerend">**\_wait_pmlogger_end**</a>|TODO|
|<a id="idx+funcs+waitpmproxyend">**\_wait_pmproxy_end**</a>|TODO|
|<a id="idx+funcs+waitprocessend">**\_wait_process_end**</a>|TODO|
|<a id="idx+funcs+webapiheaderfilter">**\_webapi_header_filter**</a>|TODO|
|<a id="idx+funcs+webapiresponsefilter">**\_webapi_response_filter**</a>|TODO|
|<a id="idx+funcs+withintolerance">**\_within_tolerance**</a>|TODO|
|<a id="idx+funcs+writableprimarylogger">**\_writable_primary_logger**</a>|TODO|

<a id="triagewaitpoint"></a>
## 8.1 **\_triage_wait_point**

Usage: **\_triage_wait_point** [*message*]

If you have a QA test that needs triaging after it has done some setup
(e.g. start a QA version of a daemon, install a PMDA, or unpack a
"fake" set of kernel stats files, create an archive), then add a call
to **\_triage_wait_point** in the test once the setup has been done.

If *message* is specified it will be echoed and
this allows the test to disclose useful information from the setup, e.g. a process ID or the path to where the magic files have been unpacked.


Now, to triage the test:

```bash
$ touch $seq.wait
$ ./$seq
```

and when **\_triage_wait_point** is called it will emit *message*
(if specified) on stdout, and go into a sleep-check loop waiting
for **$seq***.wait* to disappear.
So at this point the test is suspended and you can go poke a process, look
at a log file, check an archive, ...

When the triage is done,

```bash
$ rm $seq.wait
```
and the test will resume.

<a id="shell-functions-from-common.filter"></a>
# 9 Shell functions from *common.filter*

Because filtering output to produce deterministic results is such a
key part of the PCP QA methodology, a number of common filtering
functions are provided by *common.filter* as described below.

Except where noted, these functions as classical Unix "filters" reading
from standard input and writing to standard output.
<br>

|**Function**|**Input**|
|---|---|
|<a id="idx+funcs+cullduplines">**_cull_dup_lines**</a>|TODO|
|<a id="idx+funcs+filterallpcpstart">**_filterall_pcp_start**</a>|TODO|
|<a id="idx+funcs+filtercompilerbabble">**_filter_compiler_babble**</a>|TODO|
|<a id="idx+funcs+filterconsole">**_filter_console**</a>|TODO|
|<a id="idx+funcs+filtercronscripts">**_filter_cron_scripts**</a>|TODO|
|<a id="idx+funcs+filterdbg">**_filter_dbg**</a>|TODO|
|<a id="idx+funcs+filterdumpresult">**_filter_dumpresult**</a>|TODO|
|<a id="idx+funcs+filterinstall">**_filter_install**</a>|TODO|
|<a id="idx+funcs+filterls">**_filter_ls**</a>|TODO|
|<a id="idx+funcs+filteroptionallabels">**_filter_optional_labels**</a>|TODO|
|<a id="idx+funcs+filteroptionalpmdainstances">**_filter_optional_pmda_instances**</a>|TODO|
|<a id="idx+funcs+filteroptionalpmdas">**_filter_optional_pmdas**</a>|TODO|
|<a id="idx+funcs+filterpcprestart">**_filter_pcp_restart**</a>|TODO|
|<a id="idx+funcs+filterpcpstartdistro">**_filter_pcp_start_distro**</a>|TODO|
|<a id="idx+funcs+filterpcpstart">**_filter_pcp_start**</a>|TODO|
|<a id="idx+funcs+filterpcpstop">**_filter_pcp_stop**</a>|TODO|
|<a id="idx+funcs+filterpmcdlog">**_filter_pmcd_log**</a>|a *pmcd.log* file from **pmcd**(1).|
|<a id="idx+funcs+filterpmdainstall">**_filter_pmda_install**</a>|TODO|
|<a id="idx+funcs+filterpmdaremove">**_filter_pmda_remove**</a>|TODO|
|<a id="idx+funcs+filterpmdumplog">**_filter_pmdumplog**</a>|TODO|
|<a id="idx+funcs+filterpmdumptext">**_filter_pmdumptext**</a>|TODO|
|<a id="idx+funcs+filterpmielog">**_filter_pmie_log**</a>|a *pmie.log* file from **pmie**(1).|
|<a id="idx+funcs+filterpmiestart">**_filter_pmie_start**</a>|TODO|
|<a id="idx+funcs+filterpmiestop">**_filter_pmie_stop**</a>|TODO|
|<a id="idx+funcs+filterpmloggerlog">**_filter_pmlogger_log**</a>|a *pmlogger.log* file from **pmlogger**(1).|
|<a id="idx+funcs+filterpmproxylog">**_filter_pmproxy_log**</a>|a *pmproxy.log* file from **pmproxy**(1).|
|<a id="idx+funcs+filterpmproxystart">**_filter_pmproxy_start**</a>|TODO|
|<a id="idx+funcs+filterpmproxystop">**_filter_pmproxy_stop**</a>|TODO|
|<a id="idx+funcs+filterpost">**_filter_post**</a>|TODO|
|<a id="idx+funcs+filterslowpmie">**_filter_slow_pmie**</a>|TODO|
|<a id="idx+funcs+filtertoppmns">**_filter_top_pmns**</a>|TODO|
|<a id="idx+funcs+filtertortureapi">**_filter_torture_api**</a>|TODO|
|<a id="idx+funcs+filterviews">**_filter_views**</a>|TODO|
|<a id="idx+funcs+instancesfilterany">**_instances_filter_any**</a>|TODO|
|<a id="idx+funcs+instancesfilterexact">**_instances_filter_exact**</a>|TODO|
|<a id="idx+funcs+instancesfilternonzero">**_instances_filter_nonzero**</a>|TODO|
|<a id="idx+funcs+instvaluefilter">**_inst_value_filter**</a>|TODO|
|<a id="idx+funcs+quotefilter">**_quote_filter**</a>|TODO|
|<a id="idx+funcs+showpmieerrors">**_show_pmie_errors**</a>|TODO|
|<a id="idx+funcs+showpmieexit">**_show_pmie_exit**</a>|TODO|
|<a id="idx+funcs+sortpmdumplogd">**_sort_pmdumplog_d**</a>|TODO|
|<a id="idx+funcs+valuefilterany">**_value_filter_any**</a>|TODO|
|<a id="idx+funcs+valuefilternonzero">**_value_filter_nonzero**</a>|TODO|

<a id="control-files"></a>
# 10 Control files

There are several files that augment the test scripts and control
how QA tests are executed.

<a id="the-group-file"></a>
## 10.1 The <a id="idx+files+group">*group*</a> file

Each test belongs to one or more "groups" and the
*group* file is used to record
the set of known groups and the mapping between each test
and the associated groups of tests.

Groups are defined for applications, [PMDAs](#idx+pmda), services, general
features or functional areas (e.g. **archive**, **pmns**, **getopt**, ...)
and testing type (e.g. **remote**, **local**, **not_in_ci**, ...).

The format of the *group* file is:

- comment lines begin with **#**
- blank lines are ignored
- lines beginning with a non-numeric name a group
- lines beginning with a number associate groups with a test
- an optional tag may be appended to a test number to indicate the entry requires special treatment; the tag may be **:reserved** (the test number is allocated but the test development is not yet completed) or **:retired** (no longer active and not expected to be run)

Comments within the file provide further information as to format.

<a id="the-triaged-file"></a>
## 10.2 The <a id="idx+files+triaged">*triaged*</a> file

Some tests may fail in ways that after careful analysis are deemed to be
a "test" failure, rather than a PCP failure or regression. Causes might be
timing issues that are impossible to control or failures on slow VMs or
caused by non-PCP code that's failing.

The *traiged* file provides a mechanism that to describe failures for
specific tests on
particular hosts or operating system versions or CPU architures, or ... that
have been analyzed and should not be considered a hard PCP QA failure.
Comments at the head of the file describe the required format for entries.

**check** consults *triaged* after a test failure, and if a match is found
the test outcome is considered to be **triaged** not **fail** and the text "**\[triaged]**"
is appended to the **.out.bad** file.

<a id="the-localconfig-file"></a>
## 10.3 The <a id="idx+files+localconfig">*localconfig*</a> file

The *localconfig* file is generated by the **mk.localconfig** script.
It defines the shell variables
*localconfig* is sourced from **common.check** so every test script has access to these shell variables.

<a id="helper-scripts"></a>
# 11 Helper scripts

There are a large number of shell scripts in the QA directory that are
intended for common QA development and triage tasks beyond simply
running tests with **check**.

<a id="summary"></a>
## Summary

|**Command**|**Description**|
|---|---|
|<a id="idx+cmds+all-by-group">**all-by-group**</a>|Report all tests (excluding those tagged **:retired** or **:reserved**) in *group* sorted by group.|
|<a id="idx+cmds+appchange">**appchange**</a>|Usage: **appchange** \[**-c**] *app1* \[*app2* ...]<br>Recheck all QA tests that appear to use the test application src/*app1* or *src/app2* or ... *${TMPDIR:-/tmp}/appcache* is a cache of mappings between test sequence numbers and uses, for all applications in *src/* \... **appchange** will build the cache if it is not already there, use **-c** to clear and rebuild the cache.|
|<a id="idx+cmds+bad-by-group">**bad-by-group**</a>|Use the *\*.out.bad* files to report failures by group.|
|<a id="idx+cmds+check.app.ok">**check.app.ok**</a>|Usage: **check.app.ok** *app*<br>When the test application *src/app.c* (or similar) has been changed, this script<br>(a) remakes the application and checks **make**(1) status, and<br>(b) finds all the tests that appear to run the *src/app* application and runs **check** for these tests.|
|<a id="idx+cmds+check-auto">**check-auto**</a>|Usage: **check-auto** \[*seqno* ...]<br>Check that if a QA script uses **\_stop_auto_restart** for a (**systemd**) service, it also uses **\_restore_auto_restart** (preferably in \_cleanup()). If no *seqno* options are given then check all tests.|
|<a id="idx+cmds+check-flakey">**check-flakey**</a>|Usage: **check-flakey** \[*seqno* ...\]<br>Recheck failed tests and try to classify them as "flakey" if they pass now, or determine if the failure is "hard" (same *seqno.out.bad*) or some other sort of non-deterministic failure. If no *seqno* options are given then check all tests with a *\*.out.bad* file.|
|<a id="idx+cmds+check-group">**check-group**</a>|Usage: **check-group** *query*<br>Check the *group* file and test scripts for a specific *query* that is assumed to be **both** the name of a command that appears in the test scripts (or part of a command, e.g. **purify** in **\_setup_purify**) and the name of a group in the *group* file. Report differences, e.g. *command* appears in the *group* file for a specific test but is not apparently used in that test, or *command* is used in a specific test but is not included in the *group* file entry for that test.<br>There are some special cases to handle the pcp-foo commands, aliases and [PMDAs](#idx+pmda) ... refer to **check-group** for details.<br>Special control lines like:<br>`# check-group-include: group ...`<br>`# check-group-exclude: group ...`<br>may be embedded in test scripts to over-ride the heuristics used by **check-group**.|
|<a id="idx+cmds+check-pdu-coverage">**check-pdu-coverage**</a>|Check that PDU-related QA apps in *src* provide full coverage of all current PDU types.|
|<a id="idx+cmds+check-setup">**check-setup**</a>|Check QA environment is as expected. Documented in *README* but not used otherwise.|
|<a id="idx+cmds+check-vars">**check-vars**</a>|Check shell variables across the *common\** "include" files and the scripts used to run and manage QA. For the most part, the *common\** files should use a "\_\_" prefix for shell variables\[2] to insulate them from the use of arbitrarily name shell variables in the QA tests themselves (all of which "source" multiple of the *common\** files). **check-vars** also includes some exceptions which are a useful cross-reference.|
|<a id="idx+cmds+cull-pmlogger-config">**cull-pmlogger-config**</a>|Cull he default **pmlogger**(1) configuration (**$PCP_VAR_DIR***/config/pmlogger/config.default) *to remove any **zeroconf** proc metric logging that threatens to fill the filesystem on small QA machines.|
|<a id="idx+cmds+daily-cleanup">**daily-cleanup**</a>|Run from **check**, this script will try to make sure the **pmlogger_daily**(1) work has really been done; this is important for QA VMs that are only booted for QA and tend to miss the nightly **pmlogger_daily**(1) run.|
|<a id="idx+cmds+find-app">**find-app**</a>|Usage: **find-app** \[**-f**] *app* ...<br>Find and report tests that use any of the QA applications *src/app* ...<br>The **-f** argument changes the interpretation of *app* from *src/app* to "all the *src/\** programs" that call the function *app*.|
|<a id="idx+cmds+find-bound">**find-bound**</a>|Usage: **find-bound** *archive* *timestamp* *metric* \[*instance*]<br>Scan *archive* for values of *metric* (optionally constrained to the one *instance*) within the interval *timestamp* (in the format HH:MM:SS, as per **pmlogdump**(1) and assuming a timezone as per **-z**).|
|<a id="idx+cmds+find-metric">**find-metric**</a>|Usage: **find-metric** \[**-a**\|**-h**] *pattern* ...<br>Search for metrics with name or metadata that matches *pattern*. With **-h** interrogate the local **pmcd**(1), else with **-a** (the default) search all the QA archives in the directories *archive* and *tmparch*.<br>Multiple pattern arguments are treated as a disjunction in the search which uses **grep**(1) style regular expressions. Metadata matches are against the **pminfo**(1) **-d** output for the type, instance domain, semantics, and units.|
|<a id="idx+cmds+flakey-summary">**flakey-summary**</a>|Assuming the output from **check-flakey** has been kept for multiple QA runs across multiple hosts and saved in a file called *flakey*, this script will summarize the test failure classifications.|
|<a id="idx+cmds+getpmcdhosts">**getpmcdhosts**</a>|Usage: **getpmcdhosts** *lots-of-options*<br>Find a remote host matching a selection criteria based on hardware, operating system, installed [PMDA](#idx+pmda), primary logger running, etc. Use<br>`$ getpmcdhosts -?`<br>to see all options.|
|<a id="idx+cmds+grind">**grind**</a>|Usage: **grind** *seqno* \[...]<br>Run select test(s) in an loop until one of them fails and produces a **.out.bad** file. Stop with Ctl-C or for a more orderly end after the current iteration<br>`$ touch grind.stop`|
|<a id="idx+cmds+grind-pmda">**grind-pmda**</a>|Usage: **grind-pmda** *pmda* *seqno* \[...]<br> Exercise the *pmda* [PMDA](#idx+pmda) by running the PMDA's **Install** script, then using **check** to run all the selected tests, checking that the PMDA is still installed, running the PMDA's **Remove** script, then running the selected tests again and checking that the PMDA is still **not** installed.|
|<a id="idx+cmds+group-stats">**group-stats**</a>|Report test frequency by group, and report any group name anomalies.|
|<a id="idx+cmds+mk.localconfig">**mk.localconfig**</a>|Recreate the *localconfig* file that provides the platform and PCP version information and the *src/localconfig.h* file that can be used by C programs in the *src* directory.|
|**mk.logfarm**|See the [**mk.logfarm**](#mk.logfarm-script) section.|
|**mk.qa_hosts**|See the [**mk.qa_hosts**](#mk.qahosts-script) section below.|
|<a id="idx+cmds+mk.variant">**mk.variant**</a>|Usage: **mk.variant** *seqno*<br>Sometimes a test has no choice other than to produce different output on different platforms. This script may be used to convert an existing test to accommodate multiple *seqno.out* files.|
|**new**|See the [**new**](#the-new-script) section above.|
|<a id="idx+cmds+new-dup">**new-dup**</a>|Usage: **new-dup** \[**-n**] *seqno*<br>Make a copy of the test *seqno* using a new test number as assigned by [**new**](#the-new-script), including rewriting the old *seqno* in the new test and its new *.out* file. **-n** is "show me" mode and no changes are made.|
|<a id="new-grind"></a><a id="idx+cmds+new-grind">**new-grind**</a>|Usage: **new-grind** \[**-n**] \[**-v**] *seqno*<br>Make a copy of the test *seqno* using a new test number as assigned by [**new**](#the-new-script) and arrange matters so the new test runs the old test but selects the **valgrind**(1) sections of that test. **-n** is "show me" mode and no changes are made, use **-v** for more verbosity.|
|<a id="idx+cmds+new-seqs">**new-seqs**</a>|Report the unallocated blocks of test sequence numbers from the *group* file.|
|<a id="idx+cmds+really-retire">**really-retire**</a>|Usage: **really-retire** *seqno* \[...]<br>Mark the selected tests as **:retired** in the *group* file and then replace the test and its *.out* file with boilerplate text that explains what has happened and unilaterally calls **_notrun** (in case the test is ever really run).|
|<a id="idx+cmds+recheck">**recheck**</a>|Usage: **recheck** \[**-t**] \[*options*] \[*seqno* ...\]<br>Run **check** again for failed tests. If no *seqno* options are given then check all tests with a *\*.out.bad* file. By default tests that failed last time and were classified as **triaged** will not be rerun, but **-t** overrides this. Other *options* are any command line options that **check** understands.|
|<a id="idx+cmds+remake">**remake**</a>|Usage: **remake** \[*options*] *seqno* \[...]<br>Remake the *.out* file for the specified test(s). Command line parsing is the same as **check** so *seqno* can be a single test sequence number, or a range, or a **-g** *group* specification. Similarly **-l** selects **diff**(1) rather than a graphical diff tool to show the differences.<br>Since the *seqno.out* files are precious and reflect the state of the qualified and expected output, they should typically not be changed unless some change has been made to the *seqno* test or the applications the test runs produce different output or the filters in the test have changed.|
|<a id="idx+cmds+sameas">**sameas**</a>|Usage: **sameas** *seqno* \[...]<br>See if *seqno.out* and *seqno.out.bad* are identical except for line ordering. Useful to detect cases where non-determinism is caused by the order in which subtests were run, e.g. sensitive to directory entry order in the filesystem or metric name order in the [PMNS](#idx+pmns).|
|<a id="idx+cmds+var-use">**var-use**</a>|Usage: **var-use** *var* \[*seqno* ...]<br>Find assignment and uses of the shell variable \[**$**]*var* in tests. If *seqno* not specified, search all tests.|

<br>
\[2] If all shells supported the **local** keyword for variables we could use that, but that's not the case across all the platforms PCP runs on, so the "\_\_" prefix model is a weak substitute for proper variable scoping.

<a id="mk.logfarm-script"></a>
## 11.1 <a id="idx+cmds+mk.logfarm">**mk.logfarm**</a> script

Usage: **mk.logfarm** \[**-c** *config*] *rootdir*

The **mk.logfarm** script creates a forest of archives suitable for
use with **pmlogger_daily**(1) in tests.

The forest is rooted at the directory *rootdir* which must exist. Most
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

```bash
thishost        archives/foo+   20011005
thishost        archives/foo+   20011006.00.10
thishost        archives/foo+   20011007
otherhost       archives/ok-foo 20011002.00.10
otherhost       archives/ok-foo 20011002.00.10-00
```

<a id="mk.qahosts-script"></a>
## 11.2 <a id="idx+cmds+mk.qahosts">**mk.qa_hosts**</a> script

The **mk.qa_hosts** script includes heuristics for selecting
and sorting the list of potential remote PCP QA hosts 
*qa_hosts.primary*).
Refer to the comments in *qa_hosts.primary*,
and make appropriate changes.

The heuristics use the domain name for
the current host to choose a set of hosts that can be considered
when running distributed tests, e.g. **pmlogger**(1) locally and
**pmcd**(1) on a remote host. Anyone wishing to do this sort of
testing (it does not happen in the github CI and QA actions) will
need to figure out how to append control lines in the
*qa_hosts.primary* file.

**mk.qa_hosts** is run from *GNUmakefile* so once created, *qa_hosts*
will tend to hang around.

<a id="qa-subdirectories"></a>
# 12 qa subdirectories

Below "qa" there are a number of important subdirectories.

<a id="src"></a>
## 12.1 *src*

The source for most of the QA applications live here along with
the executables that are run from the tests.

Adding a new QA application written in C involves these steps:

1. copy *template.c* as the framework for the new QA application
2. edit the copy at will
3. update GNUlocaldefs with stanzas to match all the places **template** appears in this file
4. `$ make`
5. add the new executable to *src/.gitignore*

<a id="archives"></a>
## 12.2 *archives*

This directory contains stable PCP archives (in the git repo) that can be used to provide
deterministic PCP archives for tests to operate on.

<a id="tmparch"></a>
## 12.3 *tmparch*

This directory contains PCP archives that are created as required and
are used by tests checking the operation of **pmlogger**(1) and the
associated configurations and installed [PMDAs](#idx+pmda) on the local host.

Once created, the archives are not automatically re-created; to force creation of
a new set of archives:<br>
`$ ( cd tmparch; make clean setup )`

<a id="pmdas"></a>
## 12.4 *pmdas*

TODO

<a id="admin"></a>
## 12.5 *admin*

TODO

<a id="using-valgrind"></a>
# 13 Using valgrind

TODO. Suppressions via valgrind-suppress or
valgrind-suppress-\<version\> or insitu.

**common.check** includes the following shell functions to assist when
using **valgrind**(1) in a QA test.

|**Function**|**Description**|
|---|---|
|<a id="idx+funcs+checkvalgrind">**\_check_valgrind**</a>|TODO|
|<a id="idx+funcs+filtervalgrind">**\_filter_valgrind**</a>|TODO|
|<a id="idx+funcs+prefervalgrind">**\_prefer_valgrind**</a>|TODO|
|<a id="idx+funcs+runvalgrind">**\_run_valgrind**</a>|TODO|
|<a id="idx+funcs+filtervalgrindpossibly">**_filter_valgrind_possibly**</a>|TODO|

<a id="using-helgrind"></a>
# 14 Using helgrind

TODO. helgrind-suppress

**common.check** includes the following shell functions to assist when
using **helgrind**(1) in a QA test.

|**Function**|**Description**|
|---|---|
|<a id="idx+funcs+checkhelgrind">**\_check_helgrind**</a>|TODO|
|<a id="idx+funcs+filterhelgrind">**\_filter_helgrind**</a>|TODO|
|<a id="idx+funcs+runhelgrind">**\_run_helgrind**</a>|TODO|


<a id="common-and-common.-files"></a>
# 15 <a id="idx+files+common">*common*</a> and <a id="idx+files+common.star">*common.\**</a> files

TODO brief description in general terms, esp adding common.foo

<a id="selinux-considerations"></a>
# 16 Selinux considerations

TODO pcp-testsuite.fc, pcp-testsuite.if and pcp-testsuite.te

TODO change and install

<a id="package-lists"></a>
# 17 Package lists

package-list dir

other-packages/manifest et al

**admin/list-packages** ... -c ... -m ... -n ... -v ...

<a id="the-last-word"></a>
# 18 The last word

If you find something that does not work or seem right, then
either send email to  <a href="mailto:pcp@groups.io">pcp@groups.io</a>
or join the Performance Co-Pilot chat at
[www.performancecopilot.slack.com](https://www.performancecopilot.slack.com/) and
and post to the **#pcpqa** channel.

Better still, if you can fix the problem or create
additional QA tests please commit these to **git** and open
a Pull Request at
[https://github.com/performancecopilot/pcp](https://github.com/performancecopilot/pcp)


<a id="initial-setup-appendix"></a>
# Initial Setup Appendix

<a id="sudo-setup"></a>
## **sudo** setup

The PCP tests are designed to be run by a non-root user. Where "root"
privileges are needed, e.g. to stop or start **pmcd**(1), **Install** or **Remove**
PMDAs, etc. the **sudo**(1) application is used. When using **sudo** for QA,
your current user login needs to be able to execute commands as
root without being prompted for a password. This can be achieved by
adding the following line to the */etc/sudoers* file:

```
<your login>   ALL=(ALL) NOPASSWD: ALL
```
and checked with

```bash
$ sudo id
```

<a id="distributed-qa"></a>
## Distributed QA

PCP employs a cient-server architecture, and so some parts of the
QA infrastructure involve testing one component running on the local
system and another component running on a remote system.  For example

```bash
$ pmrep -h remote
```
runs **pmrep**(1) locally which interacts with **pmcd**(1) on the
host `remote`.

To make this work, some additional effort is required in the QA setup
both locally and on the remote system(s).

Refer to the [**mk.qahosts** script](#mk.qahosts-script) section
to see how the remote QA hosts are configured.

For each of the potential remote PCP QA hosts, the following must be
set up:

1. PCP installed from packages,
2. **pmcd**(1) running,
3. a login for the user **pcpqa** needs to be created, and then set up in such a way that **ssh**(1) and **scp**(1) will work without the need for any password, i.e. these sorts of commands<br>`$ ssh pcpqa@pcp-qa-host some-command`<br>`$ scp some-file pcpqa@pcp-qa-host:some-dir`<br>must work correctly when run from the local host with no interactive input and no Password: prompt<br><br>On Selinux systems it may be necessary to execute the following command to make this work:<br>`$ sudo chcon -R unconfined_u:object_r:ssh_home_t:s0 ~pcpqa/.ssh`<br> so that the ssh_home_t attribute is set on ~pcpqa/.ssh and all the files below there.<br><br>The **pcpqa** user's environment must also be initialized so that their shell's path includes all of the PCP binary directories (identify these with `$ grep BIN /etc/pcp.conf`), so that all PCP commands are executable without full pathnames.  Of most concern would be auxilliary directory (usually */usr/lib/pcp/bin*, */usr/share/pcp/bin* or */usr/libexec/pcp/bin*) where commands like **pmlogger_daily**(1), **pmnsadd**(1), **newhelp**(1), **mkaf**(1)etc.) are installed.<br><br>And finally, the **pcpqa** user needs to be included in the group **pcp** in */etc/group*.

Once you've setup the remote PCP QA hosts and modified *common.config*
and *qa_hosts.primary* locally, then run validate the setup using [check-setup](#check-setup):

```bash
$ ./check-setup
```

<a id="firewall-setup"></a>
## Firewall setup

Network firewalls can get in the way, especially if you're attempting
any {distributed QA](#distributed-qa).

In addition to the standard **pmcd**(1) port(s)
(TCP ports 44321, 44322 and 44323) one needs to open ports to allow
incoming connections and outgoing connections on a range of ports
for **pmdatrace**(1), **pmlogger**(1) connections via **pmlc**(1), and some QA tests.
Opening the TCP range 4320 to 4350 (inclusive) should suffice for these
additional ones.

<a id="common.config-files"></a>
## <a id="idx+files+common.config">*common.config*</a> files

This script uses heuristics to set a number of
interesting variables, specifically:

|**Shell Variable**|**Description**|
|---|---|
|<a id="idx+vars+pcpqaclosexserver">**$PCPQA_CLOSE_X_SERVER**</a>|The **$DISPLAY** setting for an X server that is willing to accept connections from X clients running on the local machine. This is optional, and if not set any QA tests dependent on this will be skipped.|
|<a id="idx+vars+pcpqafarpmcd">**$PCPQA_FAR_PMCD**</a>|The hostname for a host running **pmcd**(1), but the host is preferably a long way away (in terms of TCP/IP latency) for timing tests. This is optional, and if not set any QA tests dependent on this will be skipped.|
|<a id="idx+vars+pcpqahyphenhost">**$PCPQA_HYPHEN_HOST**</a>|The hostname for a host running **pmcd**(1), with a hyphen (-) in the hostname. This is optional, and if not set any QA tests dependent on this will be skipped.|

If relevant, edit this file to provide suitable settings for the local environment.

<a id="some-special-test-cases"></a>
## Some special test cases

For test 051 we need five local hostnames that are valid, although PCP
does not need to be installed there, nor pmcd(1) running.  The five
hosts listed in 051.hosts (the comments at the start of this file
explain what is required) should suffice for most installations.

Some tests are graphical, and wish to make use of your display.
For authentication to succeed, you may need to perform some
access list updates, e.g. "xhost +local:" for such tests to pass
(e.g. test 325).

<a id="take-it-for-a-test-drive"></a>
## Take it for a test drive

You can now verify your QA setup, by running:

```bash
$ ./check 000
```

The first time you run [**check**](#check-script)  it will descend into the
[*src*](#src) directory and make all of the QA test programs and then
descend into the [*tmparch*](#tmparch) directory and recreate
the transient PCP archives, so some patience may be required.

If test 000 fails, it may be that you have locally developed PMDAs
or optional PMDAs installed.  Edit *common.filter* and modify the
[**_filter_top_pmns**](#idx+funcs+filtertoppmns) function to strip the top-level name components
for any new metric names (there are lots of examples already there)
... if these are distributed (shipped) PMDAs, please update the list
in *common.filter* and commit the changes to **git**.

<a id="pcp-acronyms-appendix"></a>
# PCP Acronyms Appendix

|**Acronym**|**Description**|
|---|---|
|<a id="idx+pcp">**PCP**</a>|**P**erformance **C**o-**P**ilot|
|<a id="idx+pmapi">**PMAPI**</a>|**P**erformance **M**etrics **A**pplication **I**nterface: the public interfaces supported by *libpcp*|
|<a id="idx+pmcd">**PMCD**</a>|**P**erformance **M**etrics **C**ollection **D**aemon: aka **pmcd**(1), the source of all performance metric metadata and data on the local host, although the real work is delegated to the PMDAs|
|<a id="idx+pmda">**PMDA**</a>|**P**erformance **M**etrics **D**omain **A**gent: a "plugin" for **pmcd**(1) that is responsible for an independent subset of the available performance metrics|
|<a id="idx+pmns">**PMNS**</a>|**P**erformance **M**etrics **N**ame **S**pace: all of the metric names in a PCP archive or known to **pmcd**(1)|

<!--
.\" control lines for scripts/man-spell -- need to fake troff comment here
.\" +ok+ _restore_auto_restart _stop_auto_restart _setup_purify
.\" +ok+ PCP_AWK_PROG getpmcdhosts localconfig pcpversion
.\" +ok+ tmparch wallclock datestamp testsuite timeshift not_in_ci
.\" +ok+ appchange otherhost qa_hosts valgrind _cleanup PCP_star helgrind
.\" +ok+ seq_full zeroconf thishost appcache precheck _service
.\" +ok+ selinux Selinux _notrun logfarm rootdir sameas idxctl github
.\" +ok+ flakey TMPDIR insitu notrun seqno _exit mkdir funcs
.\" +ok+ nbsp cd'd cd's seqs libc cmds TODO pdu idx seq dir cmd tmp
.\" +ok+ VMs src Ctl dup qa rc HH mk al VM
.\" +ok+ _check_key_server_version_offline _filter_optional_pmda_instances
.\" +ok+ _check_local_primary_archive _check_key_server_version
.\" +ok+ _filter_valgrind_possibly _instances_filter_nonzero
.\" +ok+ _restore_pmlogger_control _wait_for_pmproxy_logfile
.\" +ok+ _wait_for_pmproxy_metrics _filter_pcp_start_distro
.\" +ok+ _find_key_server_modules _writable_primary_logger
.\" +ok+ _filter_compiler_babble _filter_optional_labels
.\" +ok+ _find_key_server_search _get_primary_logger_pid
.\" +ok+ _instances_filter_exact _restore_primary_logger
.\" +ok+ _webapi_response_filter _check_key_server_ping _filter_optional_pmdas
.\" +ok+ _restore_job_scheduler _filter_pmproxy_start _find_key_server_name
.\" +ok+ _instances_filter_any _prepare_pmda_install _remove_job_scheduler
.\" +ok+ _restore_pmda_install _value_filter_nonzero _webapi_header_filter
.\" +ok+ _check_job_scheduler _filterall_pcp_start _filter_cron_scripts
.\" +ok+ _filter_pmda_install _filter_pmlogger_log _filter_pmproxy_stop
.\" +ok+ _filter_init_distro _filter_pcp_restart _filter_pmda_remove
.\" +ok+ _filter_pmproxy_log _filter_torture_api _wait_for_pmcd_stop
.\" +ok+ _filter_dumpresult _filter_pmdumptext _filter_pmie_start
.\" +ok+ _get_libpcp_config _inst_value_filter _start_up_pmlogger
.\" +ok+ _triage_wait_point _wait_for_pmlogger _wait_pmlogger_end
.\" +ok+ _check_key_server _filter_pcp_start _filter_pmdumplog
.\" +ok+ _filter_pmie_stop _filter_slow_pmie _pid_in_container
.\" +ok+ _prepare_pmda_mmv _restore_pmda_mmv _show_pmie_errors
.\" +ok+ _sort_pmdumplog_d _systemctl_status _value_filter_any
.\" +ok+ _wait_for_pmproxy _wait_pmproxy_end _wait_process_end
.\" +ok+ _within_tolerance _check_freespace _disable_loggers _filter_helgrind
.\" +ok+ _filter_pcp_stop _filter_pmcd_log _filter_pmie_log _filter_top_pmns
.\" +ok+ _filter_valgrind _prefer_valgrind _restore_loggers _stop_auto_start
.\" +ok+ _check_helgrind _check_valgrind _cull_dup_lines _filter_console
.\" +ok+ _filter_install _find_free_port _host_to_ipaddr _ipaddr_to_host
.\" +ok+ _make_proc_stat _pstree_oneline _restore_config _show_pmie_exit
.\" +ok+ _all_hostnames _change_config _check_display _clean_display
.\" +ok+ _filter_purify _get_word_size _libvirt_is_ok _make_helptext
.\" +ok+ _path_readable _set_dsosuffix _wait_for_pmcd _wait_for_pmie
.\" +ok+ _wait_for_port _wait_pmcd_end _wait_pmie_end _wait_pmlogctl
.\" +ok+ _avail_metric _check_metric _check_purify _check_search _check_series
.\" +ok+ _cleanup_pmda _filter_views _host_to_fqdn _prepare_pmda
.\" +ok+ _private_pmcd _quote_filter _run_helgrind _run_valgrind _all_ipaddrs
.\" +ok+ _check_agent _domain_name _filter_post _host_to_ipv
.\" +ok+ _need_metric GNUlocaldefs _ps_tcp_port _save_config _sighup_pmcd
.\" +ok+ _triage_pmcd _arch_start _check_core
.\" +ok+ _filter_dbg
.\" +ok+ _get_config _get_endian _machine_id _pstree_all _run_purify
.\" +ok+ _filter_ls _localhost _filesize _get_fqdn _get_port gitignore
.\" +ok+ _try_pmlc _check_ dinking addrs _fail repo _ipv Sssh pre TT
.\" +ok+ br {from <br>}
.\" +ok+ fc te {selinux file suffixes}
.\" +ok+ PCPQA_SYSTEMD {from $PCPQA_SYSTEMD}
.\" +ok+ PCPQA_IN_CI {from $PCPQA_IN_CI}
.\" +ok+ bit_platform {from _check_64bit_platform }

-->

<!--idxctl
General Index|Commands and Scripts|Shell Functions|Shell Variables|Files
!|cmds|funcs|vars|files
-->
<a id="index"></a>
# Index

|**General Index**|**Shell Functions ...**|**Shell Functions ...**|**Shell Functions ...**|
|---|---|---|---|
|[PCP](#idx+pcp)|[\_check_key_server_ping](#idx+funcs+checkkeyserverping)|[\_find_key_server_modules](#idx+funcs+findkeyservermodules)|[_sort_pmdumplog_d](#idx+funcs+sortpmdumplogd)|
|[PMAPI](#idx+pmapi)|[\_check_key_server_version](#idx+funcs+checkkeyserverversion)|[\_find_key_server_name](#idx+funcs+findkeyservername)|[\_start_up_pmlogger](#idx+funcs+startuppmlogger)|
|[PMCD](#idx+pmcd)|[\_check_key_server_version_offline](#idx+funcs+checkkeyserverversionoffline)|[\_find_key_server_search](#idx+funcs+findkeyserversearch)|[\_stop_auto_restart](#idx+funcs+stopautorestart)|
|[PMDA](#idx+pmda)|[\_check_local_primary_archive](#idx+funcs+checklocalprimaryarchive)|[\_get_config](#idx+funcs+getconfig)|[\_systemctl_status](#idx+funcs+systemctlstatus)|
|[PMNS](#idx+pmns)|[\_check_metric](#idx+funcs+checkmetric)|[\_get_endian](#idx+funcs+getendian)|[\_triage_pmcd](#idx+funcs+triagepmcd)|
|**Commands and Scripts**|[\_check_purify](#idx+funcs+checkpurify)|[\_get_fqdn](#idx+funcs+getfqdn)|[\_triage_wait_point](#idx+funcs+triagewaitpoint)|
|[all-by-group](#idx+cmds+all-by-group)|[\_check_search](#idx+funcs+checksearch)|[\_get_libpcp_config](#idx+funcs+getlibpcpconfig)|[\_try_pmlc](#idx+funcs+trypmlc)|
|[appchange](#idx+cmds+appchange)|[\_check_series](#idx+funcs+checkseries)|[\_get_port](#idx+funcs+getport)|[_value_filter_any](#idx+funcs+valuefilterany)|
|[bad-by-group](#idx+cmds+bad-by-group)|[\_check_valgrind](#idx+funcs+checkvalgrind)|[\_get_primary_logger_pid](#idx+funcs+getprimaryloggerpid)|[_value_filter_nonzero](#idx+funcs+valuefilternonzero)|
|[check](#idx+cmds+check)|[\_clean_display](#idx+funcs+cleandisplay)|[\_get_word_size](#idx+funcs+getwordsize)|[\_wait_for_pmcd](#idx+funcs+waitforpmcd)|
|[check.app.ok](#idx+cmds+check.app.ok)|[\_cleanup_pmda](#idx+funcs+cleanuppmda)|[\_host_to_fqdn](#idx+funcs+hosttofqdn)|[\_wait_for_pmcd_stop](#idx+funcs+waitforpmcdstop)|
|[check-auto](#idx+cmds+check-auto)|[_cull_dup_lines](#idx+funcs+cullduplines)|[\_host_to_ipaddr](#idx+funcs+hosttoipaddr)|[\_wait_for_pmie](#idx+funcs+waitforpmie)|
|[check-flakey](#idx+cmds+check-flakey)|[\_disable_loggers](#idx+funcs+disableloggers)|[\_host_to_ipv6addrs](#idx+funcs+hosttoipv6addrs)|[\_wait_for_pmlogger](#idx+funcs+waitforpmlogger)|
|[check-group](#idx+cmds+check-group)|[\_domain_name](#idx+funcs+domainname)|[_instances_filter_any](#idx+funcs+instancesfilterany)|[\_wait_for_pmproxy](#idx+funcs+waitforpmproxy)|
|[check-pdu-coverage](#idx+cmds+check-pdu-coverage)|[\_exit](#idx+funcs+exit)|[_instances_filter_exact](#idx+funcs+instancesfilterexact)|[\_wait_for_pmproxy_logfile](#idx+funcs+waitforpmproxylogfile)|
|[check-setup](#idx+cmds+check-setup)|[\_fail](#idx+funcs+fail)|[_instances_filter_nonzero](#idx+funcs+instancesfilternonzero)|[\_wait_for_pmproxy_metrics](#idx+funcs+waitforpmproxymetrics)|
|[check-vars](#idx+cmds+check-vars)|[\_filesize](#idx+funcs+filesize)|[_inst_value_filter](#idx+funcs+instvaluefilter)|[\_wait_for_port](#idx+funcs+waitforport)|
|[cull-pmlogger-config](#idx+cmds+cull-pmlogger-config)|[_filterall_pcp_start](#idx+funcs+filterallpcpstart)|[\_ipaddr_to_host](#idx+funcs+ipaddrtohost)|[\_wait_pmcd_end](#idx+funcs+waitpmcdend)|
|[daily-cleanup](#idx+cmds+daily-cleanup)|[_filter_compiler_babble](#idx+funcs+filtercompilerbabble)|[\_ipv6_localhost](#idx+funcs+ipv6localhost)|[\_wait_pmie_end](#idx+funcs+waitpmieend)|
|[find-app](#idx+cmds+find-app)|[_filter_console](#idx+funcs+filterconsole)|[\_libvirt_is_ok](#idx+funcs+libvirtisok)|[\_wait_pmlogctl](#idx+funcs+waitpmlogctl)|
|[find-bound](#idx+cmds+find-bound)|[_filter_cron_scripts](#idx+funcs+filtercronscripts)|[\_machine_id](#idx+funcs+machineid)|[\_wait_pmlogger_end](#idx+funcs+waitpmloggerend)|
|[find-metric](#idx+cmds+find-metric)|[_filter_dbg](#idx+funcs+filterdbg)|[\_make_helptext](#idx+funcs+makehelptext)|[\_wait_pmproxy_end](#idx+funcs+waitpmproxyend)|
|[flakey-summary](#idx+cmds+flakey-summary)|[_filter_dumpresult](#idx+funcs+filterdumpresult)|[\_make_proc_stat](#idx+funcs+makeprocstat)|[\_wait_process_end](#idx+funcs+waitprocessend)|
|[getpmcdhosts](#idx+cmds+getpmcdhosts)|[\_filter_helgrind](#idx+funcs+filterhelgrind)|[\_need_metric](#idx+funcs+needmetric)|[\_webapi_header_filter](#idx+funcs+webapiheaderfilter)|
|[grind](#idx+cmds+grind)|[\_filter_init_distro](#idx+funcs+filterinitdistro)|[\_notrun](#idx+funcs+notrun)|[\_webapi_response_filter](#idx+funcs+webapiresponsefilter)|
|[grind-pmda](#idx+cmds+grind-pmda)|[_filter_install](#idx+funcs+filterinstall)|[\_path_readable](#idx+funcs+pathreadable)|[\_within_tolerance](#idx+funcs+withintolerance)|
|[group-stats](#idx+cmds+group-stats)|[_filter_ls](#idx+funcs+filterls)|[\_pid_in_container](#idx+funcs+pidincontainer)|[\_writable_primary_logger](#idx+funcs+writableprimarylogger)|
|[mk.localconfig](#idx+cmds+mk.localconfig)|[_filter_optional_labels](#idx+funcs+filteroptionallabels)|[\_prefer_valgrind](#idx+funcs+prefervalgrind)|**Shell Variables**|
|[mk.logfarm](#idx+cmds+mk.logfarm)|[_filter_optional_pmda_instances](#idx+funcs+filteroptionalpmdainstances)|[\_prepare_pmda](#idx+funcs+preparepmda)|[$here](#idx+vars+here)|
|[mk.qa_hosts](#idx+cmds+mk.qahosts)|[_filter_optional_pmdas](#idx+funcs+filteroptionalpmdas)|[\_prepare_pmda_install](#idx+funcs+preparepmdainstall)|[$PCP_\*](#idx+vars+pcpstar)|
|[mk.variant](#idx+cmds+mk.variant)|[_filter_pcp_restart](#idx+funcs+filterpcprestart)|[\_prepare_pmda_mmv](#idx+funcs+preparepmdammv)|[$PCPQA_CLOSE_X_SERVER](#idx+vars+pcpqaclosexserver)|
|[new](#idx+cmds+new)|[_filter_pcp_start](#idx+funcs+filterpcpstart)|[\_private_pmcd](#idx+funcs+privatepmcd)|[$PCPQA_FAR_PMCD](#idx+vars+pcpqafarpmcd)|
|[new-dup](#idx+cmds+new-dup)|[_filter_pcp_start_distro](#idx+funcs+filterpcpstartdistro)|[\_ps_tcp_port](#idx+funcs+pstcpport)|[$PCPQA_HYPHEN_HOST](#idx+vars+pcpqahyphenhost)|
|[new-grind](#idx+cmds+new-grind)|[_filter_pcp_stop](#idx+funcs+filterpcpstop)|[\_pstree_all](#idx+funcs+pstreeall)|[$PCPQA_IN_CI](#idx+vars+pcpqainci)|
|[new-seqs](#idx+cmds+new-seqs)|[_filter_pmcd_log](#idx+funcs+filterpmcdlog)|[\_pstree_oneline](#idx+funcs+pstreeoneline)|[$PCPQA_SYSTEMD](#idx+vars+pcpqasystemd)|
|[really-retire](#idx+cmds+really-retire)|[_filter_pmda_install](#idx+funcs+filterpmdainstall)|[_quote_filter](#idx+funcs+quotefilter)|[$seq](#idx+vars+seq)|
|[recheck](#idx+cmds+recheck)|[_filter_pmda_remove](#idx+funcs+filterpmdaremove)|[\_remove_job_scheduler](#idx+funcs+removejobscheduler)|[$seq_full](#idx+vars+seqfull)|
|[remake](#idx+cmds+remake)|[_filter_pmdumplog](#idx+funcs+filterpmdumplog)|[\_restore_auto_restart](#idx+funcs+restoreautorestart)|[$status](#idx+vars+status)|
|[sameas](#idx+cmds+sameas)|[_filter_pmdumptext](#idx+funcs+filterpmdumptext)|[\_restore_config](#idx+funcs+restoreconfig)|[$sudo](#idx+vars+sudo)|
|[show-me](#idx+cmds+show-me)|[_filter_pmie_log](#idx+funcs+filterpmielog)|[\_restore_job_scheduler](#idx+funcs+restorejobscheduler)|[$tmp](#idx+vars+tmp)|
|[var-use](#idx+cmds+var-use)|[_filter_pmie_start](#idx+funcs+filterpmiestart)|[\_restore_loggers](#idx+funcs+restoreloggers)|**Files**|
|**Shell Functions**|[_filter_pmie_stop](#idx+funcs+filterpmiestop)|[\_restore_pmda_install](#idx+funcs+restorepmdainstall)|[$seq_full](#idx+files+seqfull)|
|[\_all_hostnames](#idx+funcs+allhostnames)|[_filter_pmlogger_log](#idx+funcs+filterpmloggerlog)|[\_restore_pmda_mmv](#idx+funcs+restorepmdammv)|[check.log](#idx+files+check.log)|
|[\_all_ipaddrs](#idx+funcs+allipaddrs)|[_filter_pmproxy_log](#idx+funcs+filterpmproxylog)|[\_restore_pmlogger_control](#idx+funcs+restorepmloggercontrol)|[check.time](#idx+files+check.time)|
|[\_arch_start](#idx+funcs+archstart)|[_filter_pmproxy_start](#idx+funcs+filterpmproxystart)|[\_restore_primary_logger](#idx+funcs+restoreprimarylogger)|[common](#idx+files+common)|
|[\_avail_metric](#idx+funcs+availmetric)|[_filter_pmproxy_stop](#idx+funcs+filterpmproxystop)|[\_run_helgrind](#idx+funcs+runhelgrind)|[common.\*](#idx+files+common.star)|
|[\_change_config](#idx+funcs+changeconfig)|[_filter_post](#idx+funcs+filterpost)|[\_run_purify](#idx+funcs+runpurify)|[common.config](#idx+files+common.config)|
|[\_check_64bit_platform](#idx+funcs+check64bitplatform)|[\_filter_purify](#idx+funcs+filterpurify)|[\_run_valgrind](#idx+funcs+runvalgrind)|[group](#idx+files+group)|
|[\_check_agent](#idx+funcs+checkagent)|[_filter_slow_pmie](#idx+funcs+filterslowpmie)|[\_save_config](#idx+funcs+saveconfig)|[localconfig](#idx+files+localconfig)|
|[\_check_core](#idx+funcs+checkcore)|[_filter_top_pmns](#idx+funcs+filtertoppmns)|[\_service](#idx+funcs+service)|[qa_hosts](#idx+files+qahosts)|
|[\_check_display](#idx+funcs+checkdisplay)|[_filter_torture_api](#idx+funcs+filtertortureapi)|[\_set_dsosuffix](#idx+funcs+setdsosuffix)|[qa_hosts.primary](#idx+files+qahosts.primary)|
|[\_check_freespace](#idx+funcs+checkfreespace)|[\_filter_valgrind](#idx+funcs+filtervalgrind)|[\_setup_purify](#idx+funcs+setuppurify)|[triaged](#idx+files+triaged)|
|[\_check_helgrind](#idx+funcs+checkhelgrind)|[_filter_valgrind_possibly](#idx+funcs+filtervalgrindpossibly)|[_show_pmie_errors](#idx+funcs+showpmieerrors)||
|[\_check_job_scheduler](#idx+funcs+checkjobscheduler)|[_filter_views](#idx+funcs+filterviews)|[_show_pmie_exit](#idx+funcs+showpmieexit)||
|[\_check_key_server](#idx+funcs+checkkeyserver)|[\_find_free_port](#idx+funcs+findfreeport)|[\_sighup_pmcd](#idx+funcs+sighuppmcd)||
