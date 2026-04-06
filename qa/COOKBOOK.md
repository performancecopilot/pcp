# PCP QA Cookbook
![Alt text](../images/pcplogo-80.png)

# Table of contents
<br>[1 Preamble](#preamble)
<br>[2 The basic model](#the-basic-model)
<br>[3 Creating a new test](#creating-a-new-test)
<br>&nbsp;&nbsp;&nbsp;[3.1 The **new** script](#the-new-script)
<br>&nbsp;&nbsp;&nbsp;[3.2 Dealing with the Known Unknowns](#dealing-with-the-known-unknowns)
<br>[4 **check** script](#check-script)
<br>&nbsp;&nbsp;&nbsp;[4.1 command line options for **check**](#command-line-options-for-check)
<br>&nbsp;&nbsp;&nbsp;[4.2 **check** setup](#check-setup)
<br>&nbsp;&nbsp;&nbsp;[4.3 **check.callback** script](#check.callback-script)
<br>&nbsp;&nbsp;&nbsp;[4.4 *check.log* file](#check.log-file)
<br>&nbsp;&nbsp;&nbsp;[4.5 *check.time* file](#check.time-file)
<br>&nbsp;&nbsp;&nbsp;[4.6 *qa\_hosts.primary* and *qa\_hosts* files](#qahosts.primary-and-qahosts-files)
<br>[5 **show-me** script](#show-me-script)
<br>[6 Common shell variables](#common-shell-variables)
<br>[7 Coding style suggestions for tests](#coding-style-suggestions-for-tests)
<br>&nbsp;&nbsp;&nbsp;[7.1 Take control of stdout and stderr](#take-control-of-stdout-and-stderr)
<br>&nbsp;&nbsp;&nbsp;[7.2 **$seq\_full** file suggestions](#seqfull-file-suggestions)
<br>[8 Control files](#control-files)
<br>&nbsp;&nbsp;&nbsp;[8.1 The *group* file](#the-group-file)
<br>&nbsp;&nbsp;&nbsp;[8.2 The *triaged* file](#the-triaged-file)
<br>&nbsp;&nbsp;&nbsp;[8.3 The *localconfig* file](#the-localconfig-file)
<br>[9 Shell functions from *common.check*](#shell-functions-from-common.check)
<br>&nbsp;&nbsp;&nbsp;[9.1 PMDA Install and Remove](#pmda-install-and-remove)
<br>&nbsp;&nbsp;&nbsp;[Plan A](#plan-a)
<br>&nbsp;&nbsp;&nbsp;[Plan B](#plan-b)
<br>&nbsp;&nbsp;&nbsp;[9.2 **\_private\_pmcd**](#privatepmcd)
<br>&nbsp;&nbsp;&nbsp;[9.3 **\_triage\_wait\_point**](#triagewaitpoint)
<br>[10 Shell functions from *common.filter*](#shell-functions-from-common.filter)
<br>[11 Helper scripts](#helper-scripts)
<br>&nbsp;&nbsp;&nbsp;[Summary](#summary)
<br>&nbsp;&nbsp;&nbsp;[11.1 **mk.logfarm** script](#mk.logfarm-script)
<br>&nbsp;&nbsp;&nbsp;[11.2 **mk.qa\_hosts** script](#mk.qahosts-script)
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
*/var/lib/pcp/testsuite* directory that is packaged and installed),
you're using a non-root login and
let's assume that from the base of the git tree you've already done:
```bash
$ cd qa
```

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

When run under the control of [**check**](#check-script), **$seq** is executed and the
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

- Are focused on one area of functionality or previous regression (complex tests are more likely to pass in multiple subtests but failing a single subtest makes the whole test fail, and complex tests are harder to debug).
- Run quickly -- the entire QA suite already takes a long time to run.
- Are resilient to platform and distribution changes.
- Don't check something that's already covered in another test.
- When exercising complex parts of core PCP functionality we'd like to see both a non-valgrind and a valgrind version of the test (see [**new**](#the-new-script) and [**new-grind**](#new-grind) below).
- Be sensitive to system state, so if a test needs a particular service or configuration setting it should take control no matter what the system state is at the start of the test and return the system to the original state before exiting (even on error paths).  This is particularly important for PMDA installation and changes to the configuration files for key PCP services like **pmcd**(1) and **pmlogger**(1). 

And "learning by example" is the well-trusted model that pre-dates AI ... there are thousands of
existing tests, so lots of worked examples for you to choose from.

Always use **new** to create a skeletal test. In addition to creating
the skeletal test, this will **git** **add** the new test and the
*.out* file, and update the *group* file, so when you've finished the
script development you need to (at least):

```bash
$ ./remake $seq
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
[*group*](#the-group-file) file; the starting test number is randomly generated
(based on a range hard-coded in **new**, your login name magically converted to a number,
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
|**pass**|test ran to completion, exit status is 0 and output is identical to the corresponding *.out* file|
|**notrun**|test called [**\_notrun**](#idx+funcs+notrun)|
|**callback**|same as **pass** but [**check.callback**](#check.callback-script) was run and detected a problem|
|**fail**|test exit status was not 0|
|**triaged**|special kind of **fail**, see [the *triaged* file](#the-triaged-file) section below|

The non-option command line arguments identify tests to
be run using one or more *seqno* or a range *seqno*-*seqno*.
Tests will be run in numeric test number order, after any duplicates
are removed.
Leading zeroes may be omitted, so all of the following are equivalent.

```bash
$ ./check 010 00? # assume the shell's glob will expand this
$ ./check 010 009 008 007 006 005 004 003 002 001
$ ./check 0-10 000
$ ./check 000 1 002 3 4-9 10
```

If no *seqno* is specified, the default is to select all tests
in the *group* file that are not tagged **:retired** or **:reserved**.

<a id="command-line-options-for-check"></a>
## 4.1 command line options for **check**

The parsing of command line arguments is a little Neanderthal, so best
practice is to separate options with whitespace.
<br>

|**Option**|**Description**|
|---|---|
|**-c**|Before and after check for selected configuration files to ensure they have not been modified.|
|**-C**|Enable color mode to highlight outcomes (assumes interactive execution).|
|**-CI**|When QA tests run in the github infrastructure for the CI or QA actions, there are some tests that will never pass. The **-CI** option is shorthand for "**-x x11 -x remote -x not\_in\_ci**" and also sets <a id="idx+vars+pcpqainci">**$PCPQA\_IN\_CI**</a> to **yes** so individual tests can make local decisions if they are running in this environment.|
|**-g** *group*|Include all of the tests from the group *group*.|
|**-l**|When differences need to be displayed, the default is to try and use a "graphical diff" tool if one can be found; the **-l** option forces simple old **diff**(1) to be used.|
|**-s**|"Sssh" mode, no differences.|
|**-n**|Show me the selected test numbers, don't run anything.|
|**-q**|"Quick" mode, don't execute the setup steps. |
|**-r**|Include tests tagged **:reserved** and **:retired**.|
|**-T**|Output begin and end timestamps as each test is run.|
|**-TT**|Update epoch begin and end timestamps in *check.timings*.|
|**-x** *group*|Exclude all of the tests from the group *group*.|
|**-X** *seqno*|Exclude the specified tests (*seqno* may be a single test number or a comma separated list of test numbers).|

<a id="check-setup"></a>
## 4.2 **check** setup

Unless the **-q** option is given, **check** will perform
the following tasks before any test is run:<br>

- run [**mk.localconfig**](#idx+cmds+mk.localconfig)
- ensure **pmcd**(1) is running with the **-T 3** option to enable PDU tracing and the **-C 512** option to enable 512 [PMAPI](#idx+pmapi) contexts
- ensure the **sample**, **sampledso** and **simple** [PMDAs](#idx+pmda) are installed and working
- ensure **pmcd**(1), **pmproxy**(1) and **pmlogger**(1) are all configured to allow remote connections
- ensure the primary **pmlogger**(1) instance is running
- if [Distributed QA](#distributed-qa) has been enabled, check that the PCP setup on the remote systems is OK
- run `make setup` which will run `make setup` in multiple subdirectories, but most importantly *src* (so the QA apps are made), *tmparch* (so the transient archives are present) and *pmdas* (so the QA [PMDAs](#idx+pmda) are up to date)

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
**check.callback**, or more simply used as is:
```bash
$ ln -s check.callback.sample check.callback
```

Based on **check.callback.sample** the sort of things tested here might
include:

- Pre and post capture of AVCs on a SELinux system to detect new AVCs triggered by a specific test.
- Did **pmlogger\_daily** get run as expected?
- Is **pmcd** healthy? This is delegated to **./941** with **\--check**.
- Is **pmlogger** healthy? This is delegated to **./870** with **--check**.
- Are all of the configured [PMDAs](#idx+pmda) still alive?
- Has the [PMNS](#idx+pmns) been trashed? This is delegated to **./1190** with **--check**.
- Are there any PCP configuration files that contain text to indicate they have been modified by a QA test, as opposed to the version installed from packages.

<a id="check.log-file"></a>
## 4.4 <a id="idx+files+check.log">*check.log*</a> file

Historical record of each execution of [**check**]((#check-script), reporting what tests
were run, notrun, passed, triaged, failed, etc.

<a id="check.time-file"></a>
## 4.5 <a id="idx+files+check.time">*check.time*</a> file

Elapsed time for last successful execution of each test run by
**check**.

<a id="qahosts.primary-and-qahosts-files"></a>
## 4.6 <a id="idx+files+qahosts.primary">*qa\_hosts.primary*</a> and <a id="idx+files+qahosts">*qa\_hosts*</a> files

Refer to the [**mk.qa\_hosts**](#mk.qahosts-script) section.

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
**show-me** will process all the *.out.bad* files in the current
directory.

<a id="common-shell-variables"></a>
# 6 Common shell variables

The common preamble in every test script source some *common\** scripts
and the following shell variables that may be used in your test script.

|**Variable**|**Description**|
|---|---|
|<a id="idx+vars+pcpstar">**$PCP\_\***</a>|Everything from **$PCP\_DIR***/etc/pcp.conf* is placed in the environment by calling **$PCP\_DIR***/etc/pcp.env* from *common.rc*, so for example **$PCP\_LOG\_DIR** is always defined and **$PCP\_AWK\_PROG** should be used instead of **awk**.|
|<a id="idx+vars+here">**$here**</a>|Current directory tests are run from. Most useful after a test script **cd**'s someplace else and you need to **cd** back, or reference a file back in the starting directory.|
|<a id="idx+vars+seq">**$seq**</a>|The sequence number of the current test.|
|<a id="idx+vars+seqfull">**$seq\_full**</a>|Proper pathname to the test's *.full* file. Always use this in preference to **$seq**.*full* because **$seq\_full** works no matter where the test script might have **cd**'d to.|
|<a id="idx+vars+status">**$status**</a>| Exit status for the test script.|
|<a id="idx+vars+sudo">**$sudo**</a>|Proper invocation of **sudo**(1) that includes any per-platform additional command line options.|
|<a id="idx+vars+tmp">**$tmp**</a>|Unique prefix for temporary files or directory. Use **$tmp***.foo* or `$ mkdir $tmp` and then use **$tmp***/foo* or both. The standard **trap** cleanup function in each test will remove all these files and directories automatically when the test finishes, so save anything useful to **$seq\_full**.|

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
$ cmd 2>&1 | \_filter
```

or even

```bash
$ cmd >$tmp.out 2>$tmp.err
$ cat $tmp.err $tmp.out | _filter
```

<a id="seqfull-file-suggestions"></a>
## 7.2 <a id="idx+files+seqfull">**$seq\_full**</a> file suggestions

Assume your test is going to fail at some point, so be defensive up
front. In particular ensure that your test appends the following
sort of information to **$seq\_full**:

- values of critical shell variables that are not hard-coded, e.g. remote host, port being used for testing
- unfiltered output (before it goes to the *.out* file) especially if the filtering is complex or aggressive to meet determinism requirements
- on error paths, output from commands that might help explain the error cause
- context that helps match up test cases in the script with the output in both the *.out* and *.full* files, e.g. this is a common pattern:<br>
`$ echo "--- subtest foobar ---" | tee -a $seq_full`
- log files that are unsuitable for inclusion in *.out *

The common preamble for all tests will ensure **$seq\_full** is removed at the start of each test, so you can safely use constructs like:

```bash
$ echo ... >>$seq_full
$ cmd ... | tee -a $seq_full | ...
```

Remember that **$seq\_full** translates to file **$seq***.full* (dot, not underscore) in the directory the **$seq** test is run from.

<a id="control-files"></a>
# 8 Control files

There are several files that augment the test scripts and control
how QA tests are executed.

<a id="the-group-file"></a>
## 8.1 The <a id="idx+files+group">*group*</a> file

Each test belongs to one or more "groups" and the
*group* file is used to record
the set of known groups and the mapping between each test
and the associated groups of tests.

Groups are defined for applications, [PMDAs](#idx+pmda), services, general
features or functional areas (e.g. **archive**, **pmns**, **getopt**, ...)
and testing type (e.g. **remote**, **local**, **not\_in\_ci**, ...).

The format of the *group* file is:

- comment lines begin with **#**
- blank lines are ignored
- lines beginning with a non-numeric name a group
- lines beginning with a number associate groups with a test
- an optional tag may be appended to a test number to indicate the entry requires special treatment; the tag may be **:reserved** (the test number is allocated but the test development is not yet completed) or **:retired** (no longer active and not expected to be run)

Comments within the file provide further information as to format.

<a id="the-triaged-file"></a>
## 8.2 The <a id="idx+files+triaged">*triaged*</a> file

Some tests may fail in ways that after careful analysis are deemed to be
a "test" failure, rather than a PCP failure or regression. Causes might be
timing issues that are impossible to control or failures on slow VMs or
caused by non-PCP code that's failing.

The *traiged* file provides a mechanism that to describe failures for
specific tests on
particular hosts or operating system versions or CPU architectures, or ... that
have been analyzed and should not be considered a hard PCP QA failure.
Comments at the head of the file describe the required format for entries.

**check** consults *triaged* after a test failure, and if a match is found
the test outcome is considered to be **triaged** not **fail** and the text "**\[triaged]**"
is appended to the *.out.bad* file.

<a id="the-localconfig-file"></a>
## 8.3 The <a id="idx+files+localconfig">*localconfig*</a> file

The *localconfig* file is generated by the **mk.localconfig** script.
It defines the shell variables
*localconfig* is sourced from **common.check** so every test script has access to these shell variables.

<a id="shell-functions-from-common.check"></a>
# 9 Shell functions from *common.check*

A large number of shell functions that are useful across
multiple test scripts are provided by *common.check* to assist with
test development and these are always available for tests created with
a standard preamble provided by the [**new**](#the-new-script) script.

In addition to defining the shell procedures (some of the more frequently
used ones are described in the table below), *common.check* also
handles:

- if necessary running [**mk.localconfig**](#idx+cmds+mk.localconfig)
- sourcing [*localconfig*](#idx+files+localconfig)
- setting <a id="idx+vars+pcpqasystemd">**$PCPQA\_SYSTEMD**</a> to **yes** or **no** depending if services are controlled by **systemctl**(1) or not

In the descriptions below "output" means output to stdout.
<br>

|**Function**|**Description**|
|---|---|
|<a id="idx+funcs+allhostnames">**\_all\_hostnames**</a>|Usage: **\_all\_hostnames** *hostname*<br>Generate a comma separated list of all hostnames or IP addresses associated with active network interfaces for the host *hostname*.<br>Requires **ssh**(1) access for the user **pcpqa** to *hostname*.|
|<a id="idx+funcs+allipaddrs">**\_all\_ipaddrs**</a>|Usage: **\_all\_ipaddrs** *hostname*<br>Generate a comma separated list of all IP addresses associated with active network interfaces for the host *hostname*.<br>Requires **ssh**(1) access for the user **pcpqa** to *hostname*.|
|<a id="idx+funcs+archstart">**\_arch\_start**</a>|Usage: **\_arch\_start** *archive* \[*offset*]<br>Output the time of the first real metrics in *archive* in the format **@HH:MM:SS.SSS** (so suitable for use with a **-O** or **-S** command line option to PCP reporting tools). The time is in the timezone of the archive, so it is expected that it will be used with a **-z** command line option.<br>If present *offset* is interpreted as a number of seconds to be added to the time before it is printed.<br>Use this function to safely skip over the *preamble* **pmResult** at the start of a PCP archive.|
|<a id="idx+funcs+availmetric">**\_avail\_metric**</a>|Usage: **\_avail\_metric** *metric*<br>Check if *metric* is available from the local **pmcd**(1). If available return **0** else return **1**. Use this check for a *metric* that is optionally required by a QA test but *metric* is not universally available, e.g. kernel metrics that are not present on all platforms.<br>See also the [**\_check\_metric**](#idx+funcs+checkmetric) and [**\_need\_metric**](#idx+funcs+needmetric) functions below.|
|<a id="idx+funcs+changeconfig">**\_change\_config**</a>|Usage: **\_change\_config** *service* *onoff*<br>The PCP services like **pmcd**, **pmproxy**, **pmlogger** and **pmie** are typically enabled or disabled by some mechanism outside the PCP ecosystem, e.g. **init**(1) or **systemd**(1) and this influences whether each of the services is stopped or started during system shutdown and system boot. The **\_change\_config** function provides a platform-independent way of changing the setting for *service*. *onoff* must be **on** or **off**.<br>As a special case, *service* may be **verbose** to enable or disable verbosity for the underlying mechanism if that notion is supported.<br>See also [**\_get\_config**](#idx+funcs+getconfig).|
|<a id="idx+funcs+check64bitplatform">**\_check\_64bit\_platform**</a>|Usage: **\_check\_64bit\_platform**<br>If the test is **not** being run on a 64-bit platform, call [**\_notrun**](#idx+funcs+notrun) with an appropriate message.|
|<a id="idx+funcs+checkagent">**\_check\_agent**</a>|Usage: **\_check\_agent** *pmda* \[*verbose*]<br>Checks that the *pmda* [PMDA](#idx+pmda) is installed and responding to metric requests. Returns 0 if all is well, else returns 1 and outputs diagnostics to explain why. If *verbose* is **true** emit diagnostics independent of return value.|
|<a id="idx+funcs+checkcore">**\_check\_core**</a>|Usage: **\_check\_core** \[*dir\*]<br>List any "core" files in *dir* (defaults to *.*), so no output if there are none.|
|<a id="idx+funcs+checkdisplay">**\_check\_display**</a>|Usage: **\_check\_display**<br>For applications that need an X11 display (like **pmchart**(1)), call [**\_notrun**](#idx+funcs+notrun) with an appropriate message if [**$PCPQA\_CLOSE\_X\_SERVER**](#idx+vars+pcpqaclosexserver) is not set or **xdpyinfo**(1) cannot be run successfully.<br>If **\_check\_display** believes the X11 display is accessible and [**$PCPQA\_DESKTOP\_HACK**](#idx+vars+pcpqadesktophack) is set to **true** then the directory **$tmp***/xdg-runtime* is also created and **$XDG\_RUNTIME\_DIR** is set to the path to this directory.  This may be needed to suppress warning babble from some Qt applications. See also [**\_clean\_display**](#idx+funcs+cleandisplay).|
|<a id="idx+funcs+checkfreespace">**\_check\_freespace**</a>|Usage: **\_check\_freespace** *need*<br>Returns 0 if there is more that *need* Mbytes of free space in the filesystem for the current working directory, else returns 1.|
|<a id="idx+funcs+checkjobscheduler">**\_check\_job\_scheduler**</a>|Usage: **\_check\_job\_scheduler**<br>Many of the PCP services require periodic actions to check health, rotate logs, rotate **pmlogger**(1) archives, daily report generation, etc. and these depend on **systemd**(1) timers or **cron**(1). This function tests if one of the required underlying mechanisms is available, else call [**\_notrun**](#idx+funcs+notrun) with the (somewhat cryptic) message "No crontab binary found". Tests that rely on these periodic actions are (a) rare, but (b) likely to fail when QA is running in a container, hence the need for this function.<br>When the timers need to be disabled for a QA test, the usual sequence is to call **\_check\_job\_scheduler** to ensure the underlying mechanism is available, then call [**\_remove\_job\_scheduler**](#idx+funcs+removejobscheduler) to disable the timer(s) and then call [**\_restore\_job\_scheduler**](#idx+funcs+restorejobscheduler) at the end of the test to re-enable the timer(s).|
|<a id="idx+funcs+checkkeyserver">**\_check\_key\_server**</a>|Usage: **\_check\_key\_server** \[*port*]<br>Check if a key server is installed and running locally and the version is not too old. Optional *port* parameter defaults to 6379. Uses [**\_check\_key\_server\_version**](#idx+funcs+checkkeyserverversion).|
|<a id="idx+funcs+checkkeyserverping">**\_check\_key\_server\_ping**</a>|Usage: **\_check\_key\_server\_ping** *port*<br>Send "ping" requests to the local key server on port *port* until we see the expected response.  Will timeout after 1.25 secs and output from key server will be printed, else no output or return value.|
|<a id="idx+funcs+checkkeyserverversion">**\_check\_key\_server\_version**</a>|Usage: **\_check\_key\_server\_version** *port*<br> Check key server version on port *port*. Assumes [**\_check\_series**](#idx+funcs+checkseries) already called and a key server is running locally. If a problem is encountered, [**\_notrun**](#idx+funcs+notrun) is called with an appropriate message.|
|<a id="idx+funcs+checkkeyserverversionoffline">**\_check\_key\_server\_version\_offline**</a>|Usage: **\_check\_key\_server\_version\_offline**<br>Check key server version without contacting key server, uses **--version** argument to key server executable. If a problem is encountered, [**\_notrun**](#idx+funcs+notrun) is called with an appropriate message.|
|<a id="idx+funcs+checklocalprimaryarchive">**\_check\_local\_primary\_archive**</a>|Usage: **\_check\_local\_primary\_archive**<br>Check if the primary **pmlogger**(1) is writing to a local file (as opposed to "logpush" via http to a remote **pmproxy**(1).  Returns 0 if true, else returns 1.|
|<a id="idx+funcs+checkmetric">**\_check\_metric**</a>|Usage: **\_check\_metric** *metric* \[*hostname*]<br>Check if *metric* is available from the host *hostname* (defaults to **local:**). If available return **0** else emit an error message and return **1**. Use this check for a *metric* that should have been made available by a recent PMDA installation. See also the [**\_avail\_metric**](#idx+funcs+availmetric) function above and the [**\_need\_metric**](#idx+funcs+needmetric) function below.|
|<a id="idx+funcs+checksearch">**\_check\_search**</a>|Usage: **\_check\_search**<br>Check if the key server search module is installed. If OK, silently returns, else [**\_notrun**](#idx+funcs+notrun) is called with an appropriate message.<br>Uses [**\_find\_key\_server\_search**](#idx+funcs+findkeyserversearch) and [**\_find\_key\_server\_modules**](#idx+funcs+findkeyservermodules).|
|<a id="idx+funcs+checkseries">**\_check\_series**</a>|Usage: **\_check\_series**<br>Check we have **pmseries**(1) and key server **-cli** and **-server** executables installed. Call [**\_notrun**](#idx+funcs+notrun) with an appropriate message otherwise.<br>See also [**\_check\_key\_server**](#idx+funcs+checkkeyserver).|
|<a id="idx+funcs+cleandisplay">**\_clean\_display**</a>|Usage: **\_clean\_display**<br>Remove the directory **$tmp***/xdg-runtime*, probably created by a previous call to [**\_check\_display**](#idx+funcs+checkdisplay).|
|**\_cleanup\_pmda**|Refer to the [PMDA Install and Remove](#pmda-install-and-remove) section below.|
|<a id="idx+funcs+disableloggers">**\_disable\_loggers**</a>|Usage: **\_disable\_loggers**<br>Replaces the control files for all **pmlogger**(1) instances with a simple one that starts a primary logger with */dev/null* as the configuration file so it makes **no** requests to **pmcd**(1).  In effect, this "disables" all loggers.<br>Restore the control files by calling [**\_restore\_loggers**](#idx+funcs+restoreloggers) before exiting the test.|
|<a id="idx+funcs+domainname">**\_domain\_name**</a>|Usage: **\_domain\_name**<br>Output the local host's domain name, else **localdomain** if the real domain name cannot be found.|
|<a id="idx+funcs+exit">**\_exit**</a>|Usage: **\_exit** *status*<br>Set [$status](#idx+vars+status) to *status* and force test exit.|
|<a id="idx+funcs+fail">**\_fail**</a>|Usage: **\_fail** *message*<br>Emit *message* on stderr and force failure exit of test.|
|<a id="idx+funcs+filesize">**\_filesize**</a>|Usage: **\_filesize** *file*<br>Output the size of *file* in bytes.|
|<a id="idx+funcs+filterinitdistro">**\_filter\_init\_distro**</a>|Usage: **\_filter\_init\_distro**<br>Distro-specific filtering for **init**(1), "rc" scripts, etc.|
|<a id="idx+funcs+findfreeport">**\_find\_free\_port**</a>|Usage: **\_find\_free\_port** \[*baseport*]<br>Find an unused local TCP port starting at *baseport* (defaults to 54321). Search proceeds by incrementing the port number by 1 after each failed probe. On success, output the port number, else after 100 attempts give up, emit an error message on stderr and call [**\_exit**](#idx+funcs+exit).|
|<a id="idx+funcs+findkeyservermodules">**\_find\_key\_server\_modules**</a>|Usage: **\_find\_key\_server\_modules**<br>Outputs the path to the "modules" directory for the local key server, if any.|
|<a id="idx+funcs+findkeyservername">**\_find\_key\_server\_name**</a>|Usage: **\_find\_key\_server\_name**<br>Output the name of the installed key server, either **valkey** or **redis** or "".|
|<a id="idx+funcs+findkeyserversearch">**\_find\_key\_server\_search**</a>|Usage: **\_find\_key\_server\_search**<br>Output the name of the installed key server search application, either **valkeysearch** or **redisearch** or "".|
|<a id="idx+funcs+getconfig">**\_get\_config**</a>|Usage: **\_get\_config**<br>PCP services like **pmcd**, **pmproxy**, **pmlogger** and **pmie** are typically enabled or disabled by some mechanism outside the PCP ecosystem, e.g. **init**(1) or **systemd**(1) and this influences whether each of the services is stopped or started during system shutdown and system boot. The **\_get\_config** function provides a platform-independent way of interrogating the state for *service*. If the state can be determined, **\_get\_config** outputs **on** or **off** and returns 0.  Otherwise the return value is 1 and an explanatory message is output. As a special case, *service* may be **verbose** to interrogate the verbosity for the underlying mechanism if that notion is supported.<br>See also [**\_change\_config**](#idx+funcs+changeconfig).|
|<a id="idx+funcs+getendian">**\_get\_endian**</a>|Usage: **\_get\_endian**<br>Output **be** (big endian) or **le** (little endian) for the host where the QA test is running.|
|<a id="idx+funcs+getfqdn">**\_get\_fqdn**</a>|Usage: **\_get\_fqdn**<br>Output the FQDN (fully qualified host name) for the host running the QA test by calling [**\_host\_to\_fqdn**](#idx+funcs+hosttofqdn).|
|<a id="idx+funcs+getlibpcpconfig">**\_get\_libpcp\_config**</a>|Usage: **\_get\_libpcp\_config**<br>Calls **pmconfig**(1) (with the **-L** and **-s** options) and sets *all* of the PCP library configuration variables as shell variables in the environment.|
|<a id="idx+funcs+getport">**\_get\_port**</a>|Usage: **\_get\_port** *proto* *lowport* *highport*<br>For IP protocol *proto* (**tcp** or **udp**) output the first unused port in the range *lowport* to *highport*, else no output if an unused port cannot be found.|
|<a id="idx+funcs+getprimaryloggerpid">**\_get\_primary\_logger\_pid**</a>|Usage: **\_get\_primary\_logger\_pid**<br>Output the PID of the running primary **pmlogger**(1), if any.|
|<a id="idx+funcs+getwordsize">**\_get\_word\_size**</a>|Usage: **\_get\_word\_size**<br>If known, output the word size (in bits) for the host running the QA test and return 0. Otherwise output **0** and return 1.|
|<a id="idx+funcs+hosttofqdn">**\_host\_to\_fqdn**</a>|Usage: **\_host\_to\_fqdn** *hostname*<br>Output the FQDN (fully qualified host name) for *hostname*.|
|<a id="idx+funcs+hosttoipaddr">**\_host\_to\_ipaddr**</a>|Usage: **\_host\_to\_ipaddr** *hostname*<br>Output the IPV4 address of *hostname*.|
|<a id="idx+funcs+hosttoipv6addrs">**\_host\_to\_ipv6addrs**</a>|Usage: **\_host\_to\_ipv6addrs** *hostname*<br>Output all the IPV6 connections strings (excluding loopback) for the host *hostname* which must be runnning a contactable **pmcd**(1).|
|<a id="idx+funcs+ipaddrtohost">**\_ipaddr\_to\_host**</a>|Usage: **\_ipaddr\_to\_host** *ipaddr*<br>Output the hostname associated with the IPv4 address *ipaddr*.|
|<a id="idx+funcs+ipv6localhost">**\_ipv6\_localhost**</a>|Usage: **\_ipv6\_localhost**<br>Output the IPV6 connection string for localhost. Emit an error message on stderr if this cannot be found.|
|<a id="idx+funcs+libvirtisok">**\_libvirt\_is\_ok**</a>|Usage: **\_libvirt\_is\_ok**<br>Check if *libvirt* and in particular the Python wrapper for *libvirt* seems to be OK ... historically some versions were prone to core dumps. Returns 1 for known to be bad, else 0.|
|<a id="idx+funcs+machineid">**\_machine\_id**</a>|Usage: **\_machine\_id**<br>Output the machine id signature for the host running QA.  For Linux systems this is the SHA from */etc/machine-id* else the dummy value **localmachine**.|
|<a id="idx+funcs+makehelptext">**\_make\_helptext**</a>|Usage: **\_make\_helptext** *pmda*<br>Each PMDA provides "help" text for the metrics it exports.  For some PMDAs this is in text files which need to be converted into the format required by the library routines that underpin each PMDA. This function optional runs **newhelp**(1), checks that the necessary files are in place for the *pmda* PMDA, and returns 0 if all is well. If there is a problem, error messages are output and the return value is 1.|
|<a id="idx+funcs+makeprocstat">**\_make\_proc\_stat**</a>|Usage: **\_make\_proc\_stat** *path* *ncpu*<br>Generate dummy lines for a Linux */proc/stat* file and write them to the file *path*. The **cpu** and **cpu***N* lines are generated for a machine with *ncpu* CPUs.|
|<a id="idx+funcs+needmetric">**\_need\_metric**</a>|Usage: **\_need\_metric** *metric*<br>Check if *metric* is available from the local **pmcd**(1). If not available, call [**\_notrun**](#idx+funcs+notrun) with an appropriate message.<br>Use this check for a *metric* that is required by a QA test but *metric* is not universally available, e.g. kernel metrics that are not present on all platforms.<br>See also the [**\_avail\_metric**](#idx+funcs+availmetric) and [**\_check\_metric**](#idx+funcs+checkmetric) functions above.|
|<a id="idx+funcs+notrun">**\_notrun**</a>|Usage: **\_notrun** *message*<br>Not all tests are expected to be able to run on all platforms. Reasons might include: won't work at all a certain operating system, application required by the test is not installed, metric required by the test is not available from **pmcd**(1), etc.<br>In these cases, the test should include a guard that captures the required precondition and call **\_notrun** with a helpful *message* if the guard fails. For example.<br>&nbsp;&nbsp;&nbsp;`which pmrep >/dev/null 2>&1 || \_notrun "pmrep not installed"`|
|<a id="idx+funcs+pathreadable">**\_path\_readable**</a>|Usage: **\_path\_readable** *user* *path*<br>Determine if the file *path* is readable by the login *user*. Return 0 if OK, else returns 1 after emitting reason(s) on stderr.|
|<a id="idx+funcs+pidincontainer">**\_pid\_in\_container**</a>|Usage: **\_pid\_in\_container** *pid*<br>Return 0 if process *pid* is definitely running in a container (assumes Linux and some heuristic pattern matching against */proc/pid/cgroup*), otherwise return 1.|
|**\_prepare\_pmda**|Refer to the [PMDA Install and Remove](#pmda-install-and-remove) section below.|
|**\_prepare\_pmda\_install**|Refer to the [PMDA Install and Remove](#pmda-install-and-remove) section below.|
|<a id="idx+funcs+preparepmdammv">**\_prepare\_pmda\_mmv**</a>|Usage: **\_prepare\_pmda\_mmv**<br>Save the directory of mmap'd files used by the **mmv** PMDA, pending installation of a new configuration for this PMDA.<br>The test should call [**\_restore\_pmda\_mmv**](#idx+funcs+restorepmdammv) before exiting.|
|<a id="idx+funcs+privatepmcd">**\_private\_pmcd**</a>|See the [**\_private\_pmcd**](#privatepmcd) section below.|
|<a id="idx+funcs+pstcpport">**\_ps\_tcp\_port**</a>|Usage: **\_ps\_tcp\_port** *port*<br>Output details of processes listening on TCP port *port*.|
|<a id="idx+funcs+pstreeall">**\_pstree\_all**</a>|Usage: **\_pstree\_all** *pid*<br>Show all the ancestors and descendent of the process with PID *pid*. Hides the platform-specific differences in how **pstree**(1) needs to be called.|
|<a id="idx+funcs+pstreeoneline">**\_pstree\_oneline**</a>|Usage: **\_pstree\_oneline**<br>One line summary version of **\_pstree\_all**.|
|<a id="idx+funcs+removejobscheduler">**\_remove\_job\_scheduler**</a>|Usage: **\_remove\_job\_scheduler** *cron* *systemd* *sudo*<br>Disable all PCP service timers (assumes [**\_check\_job\_scheduler**](#idx+funcs+checkjobscheduler) was called earlier). Prior **cron**(1) state (if any) is saved in the file *cron*. Prior **systemd**(1) state (if any) is saved in the file *systemd*. Uses *sudo* as the **sudo**(1) command.<br>Refer to [**\_check\_job\_scheduler**](#idx+funcs+checkjobscheduler) for details.|
|<a id="idx+funcs+restoreconfig">**\_restore\_config**</a>|Usage: **\_restore\_config** *target*<br>Reinstates a configuration file or directory *target* previously saved with [**\_save\_config**](#idx+funcs+saveconfig).|
|<a id="idx+funcs+restorejobscheduler">**\_restore\_job\_scheduler**</a>|Usage: **\_restore\_job\_scheduler** *cron* *systemd* *sudo*<br>Re-enable PCP service timers (assumes [**\_remove\_job\_scheduler**](#idx+funcs+removejobscheduler) was called earlier). Desired **cron**(1) state (if any) was previously saved in the file *cron*. Desired **systemd**(1) state (if any) was previously saved in the file *systemd*. Uses *sudo* as the **sudo**(1) command.<br>Refer to [**\_check\_job\_scheduler**](#idx+funcs+checkjobscheduler) for details.|
|<a id="idx+funcs+restoreloggers">**\_restore\_loggers**</a>|Usage: **\_restore\_loggers**<br>Reverses the changes from [**\_disable\_loggers**](#idx+funcs+disableloggers), see above.|
|**\_restore\_pmda\_install**|Refer to the [PMDA Install and Remove](#pmda-install-and-remove) section below.|
|<a id="idx+funcs+restorepmdammv">**\_restore\_pmda\_mmv**</a>|Usage: **\_prepare\_pmda\_mmv**<br>Restore the directory of mmap'd files used by the **mmv** PMDA saved in an earlier call to [**\_prepare\_pmda\_mmv**](#idx+funcs+preparepmdammv).|
|<a id="idx+funcs+restorepmloggercontrol">**\_restore\_pmlogger\_control**</a>|Usage: **\_restore\_pmlogger\_control**<br>Edit the various **pmlogger**(1) control files below the **$PCP_SYSCONF_DIR***/pmlogger* directory to ensure that only the primary **pmlogger** is enabled.<br>Only the control files ar changed, the caller needs to follow up with<br>`$ _service pmlogger restart`<br>for any change to take effect.|
|<a id="idx+funcs+restoreprimarylogger">**\_restore\_primary\_logger**</a>|Usage: **\_restore\_primary\_logger**<br>Ensure the configuration file for the primary **pmlogger**(1) does *not* allows pmlc(1) to make state changes to dynamically add or delete metrics to be logged (this is the default after PCP installation).<br>This function effectively reverses the changes made by [**\_writable\_primary\_logger**](#idx+funcs+writableprimarylogger).|
|<a id="idx+funcs+saveconfig">**\_save\_config**</a>|Usage: **\_save\_config** *target*<br>Save a configuration file or directory *target* with a name that uses [$seq](#idx+vars+seq) so that if a test aborts we know who was dinking with the configuration.<br>Operates in concert with [**\_restore\_config**](#idx+funcs+restoreconfig).|
|<a id="idx+funcs+service">**\_service**</a>|Usage: **\_service** \[**-v**] *service* *action*<br>Controlling services like **pmcd**(1) or **pmlogger**(1) or ... may involve **init**(1) or **systemctl**(1) or something else. This complexity is hidden behind the **\_service** function which should be used whenever a test wants to control a PCP service.<br>Supported values for *service* are: **pmcd**, **pmlogger**, **pmproxy** **pmie**.<br>*action* is one of **stop**, **start** (may be no-op if already started) or **restart** (force stop if necessary, then start).<br>Use **-v** for more verbosity.|
|<a id="idx+funcs+setdsosuffix">**\_set\_dsosuffix**</a>|Usage: **\_set\_dsosuffix**<br>Set the shell variable **$DSOSUFFIX** to the suffix used for shared libraries. This is platform-specfic, but for Linux it is **so**.|
|<a id="idx+funcs+sighuppmcd">**\_sighup\_pmcd**</a>|Usage: **\_sighup\_pmcd**<br>Send **pmcd**(1) a SIGHUP signal and reliably check that it received (at least) one.<br>Returns 0 on success, else the retrun value is 1 and an explanatory message is output.|
|<a id="idx+funcs+startuppmlogger">**\_start\_up\_pmlogger**</a>|Usage: **\_start\_up\_pmlogger** *arg* ...<br>Start a new **pmlogger**(1) instance in the background with appropriate privileges so that it can create the portmap files in **$PCP_TMP_DIR** and thus be managed by **pmlogctl**(1) or **pmlc**(1).  All of the *arg* arguments are passed to the new **pmlogger** and the process ID of the new **pmlogger** is returned in **$pid** (which will be empty if **pmlogger** was not started).<br>The process will be run as the user **pcp** (or **root** if **pcp** is not available), so the current directory needs to be writable by that user and the test's **_cleanup** function needs to use **$sudo** to remove the files created by **pmlogger**.|
|<a id="idx+funcs+stopautorestart">**\_stop\_auto\_restart**</a>|Usage: **\_stop\_auto\_restart** *service*<br>When testing error handling or timeout conditions for services it may be important to ensure the system does not try to restart a failed service (potentially leading to an hard loop of retry-fail-retry). **\_stop\_auto\_start** will change configuration to prevent restarting for *service* if the system supports this function.<br>Use <a id="idx+funcs+restoreautorestart">**\_restore\_auto\_restart**</a> with the same *service* to reinstate the configuration.|
|<a id="idx+funcs+systemctlstatus">**\_systemctl\_status**</a>|Usage: **\_systemctl\_status** *service*<br>If the service *service* is being managed by **systemd**(1) then call **systemctl**(1) and **journalctl**(1) to provide a verbose report on the status of the service.  Mostly used by other functions in this group in the event of failure to start or stop *service*.|
|<a id="idx+funcs+triagepmcd">**\_triage\_pmcd**</a>|Usage: **\_triage\_pmcd**<br>Produce a triage report for a failing **pmcd**(1).|
|<a id="idx+funcs+triagewaitpoint">**\_triage\_wait\_point**</a>|See the [**\_triage\_wait\_point**](#triagewaitpoint) section below.|
|<a id="idx+funcs+trypmlc">**\_try\_pmlc**</a>|Usage: **\_try\_pmlc** \[*expect*]<br>The **pmlc**(1) application interrogates and controls **pmlogger**(1) instances, but it may not succeed if some other **pmlc** is interacting with the target **pmlogger** (this is is expected when **pmlogger** timer services are running concurrently with QA).<br>On entry to this function, the desired **pmlc** commands (including the **connect** command to identify the **pmlogger** of interest) must be in the file **$tmp**.pmlc. This function will then try up to 10 times (with 0.1 sec sleeps between tries) to run **pmlc**. A record of the successes or failures is appended to **$seq_full**. On failure after 10 attempts, if the optional argument *expect* is **expect-failure** the function quiety returns, else an error message and the last output (stdout and stderr) from **pmlc** is reported.|
|<a id="idx+funcs+waitforpmcd">**\_wait\_for\_pmcd**</a>|Usage: **\_wait\_for\_pmcd** \[*maxdelay* \[*host* \[*port*]]]<br>Wait for **pmcd**(1) to start. The arguments are optional, but the parsing is a bit Neanderthal so if you need to specify an argument you need to specify the all the preceeding arguments in the order above. The defaults are *maxdelay* **20** (seconds), *host* **localhost** and *port* "" (so the default **$PMCD_PORT**).<br>**pmcd** is considered "started" when it returns a value for the `pmcd.numclient` metric and **\_wait\_for\_pmcd** returns 0. If, after *maxdelay* iterations (each with a 1 sec sleep), **pmcd** does not respond, then a detailed triage report is produced and **\_wait\_for\_pmcd** returns 1.|
|<a id="idx+funcs+waitforpmcdstop">**\_wait\_for\_pmcd\_stop**</a>|Usage: **\_wait\_for\_pmcd\_stop** \[*maxdelay*]<br>Wait for **pmcd**(1) to stop. *maxdelay* defaults to **20** (seconds).<br>**pmcd** is considered "stopped" when it does not return a value for the `pmcd.numclient` metric and **\_wait\_for\_pmcd\_stop** returns 0. If, after *maxdelay* iterations (each with a 1 sec sleep), **pmcd** is still responding, then a detailed triage report is produced and **\_wait\_for\_pmcd\_stop** returns 1.<br>See also [**\_wait\_pmcd\_end**](#idx+funcs+waitpmcdend).|
|<a id="idx+funcs+waitforpmie">**\_wait\_for\_pmie**</a>|Usage: **\_wait\_for\_pmie**<br>Wait for the primary **pmie**(1) process to get started as indicated by the presence of the **$PCP_RUN_DIR***/pmie.pid* file. Return 0 on success, else returns 1 with a status report on failure after 10 secs.|
|<a id="idx+funcs+waitforpmlogger">**\_wait\_for\_pmlogger**</a>|Usage: **\_wait\_for\_pmlogger** \[*pid* \[*logfile* \[*maxdelay*]]]<br>Wait for a **pmlogger**(1) instance to start. The arguments are optional, but the parsing is a bit Neanderthal so if you need to specify an argument you need to specify the all the preceeding arguments in the order above. The defaults are *pid* **-P** (the primary **pmlogger** instance), *logfile* **$PCP_ARCHIVE_DIR***/$(hostname)/pmlogger.log* and *maxdelay* **20** (seconds).<br>The designated **pmlogger** is considered "started" when it can be contacted via **pmlc**(1). On success **\_wait\_for\_pmlogger** returns 0. If, after *maxdelay* iterations (each with a 1 sec sleep), **pmlogger** does not respond, then a detailed triage report is produced including the contents of *logfile* and **\_wait\_for\_pmlogger** returns 1.|
|<a id="idx+funcs+waitforpmproxy">**\_wait\_for\_pmproxy**</a>|Usage: **\_wait\_for\_pmproxy** \[*port*] \[*logfile*]]<br>Wait for **pmproxy**(1) to start. The arguments are optional, but the parsing is a bit Neanderthal so if you need to specify an argument you need to specify the all the preceeding arguments in the order above. The defaults are *port* **44322** and *logfile* **$PCP_LOG_DIR***/pmproxy/pmproxy.log*.<br>**pmproxy** is considered "started" when it accepts TCP connections on port *port* and **\_wait\_for\_pmproxy** returns 0. If, after 20 iterations (each with a 1 sec sleep), **pmproxy** does not respond then a status report is produced and **\_wait\_for\_pmproxy** returns 1.<br>See also [**\_wait\_for\_pmproxy\_logfile**](#idx+funcs+waitforpmproxylogfile) and [**\_wait\_for\_pmproxy\_metrics**](#idx+funcs+waitforpmproxymetrics).|
|<a id="idx+funcs+waitforpmproxylogfile">**\_wait\_for\_pmproxy\_logfile**</a>|Usage: **\_wait\_for\_pmproxy\_logfile** *logfile*<br>Called after **\_wait\_for\_pmproxy**, this function waits up to 5 secs for the file *logfile* to appear. Returns 0 on success, else returns 1.|
|<a id="idx+funcs+waitforpmproxymetrics">**\_wait\_for\_pmproxy\_metrics**</a>|Usage: **\_wait\_for\_pmproxy\_metrics**<br>Called after **\_wait\_for\_pmproxy**, this function waits up to 5 secs for values of the metrics `pmproxy.pid`, `pmproxy.cpu` and `pmproxy.map.instance.size` be available. Returns 0 on success, else returns 1.|
|<a id="idx+funcs+waitforport">**\_wait\_for\_port**</a>|Usage: **\_wait\_for\_port** *port*<br>Wait up to 5 secs for a process running on the local system to be accepting connections on TCP port *port*. Returns 0 for success, else returns 1.|
|<a id="idx+funcs+waitpmcdend">**\_wait\_pmcd\_end**</a>|Usage: **\_wait\_pmcd\_end**<br>Wait up to 10 secs until **pmcd**(1) is no longer running, usually called after `_service pmcd stop`. If the process is running, calls [**\_wait\_process\_end**](#idx+funcs+waitprocessend).<br>Returns 0 on success, else returns 1.<br>See also [**\_wait\_for\_pmcd\_stop**](#idx+funcs+waitforpmcdstop).|
|<a id="idx+funcs+waitpmieend">**\_wait\_pmie\_end**</a>|Usage: **\_wait\_pmie\_end**<br>Wait up to 10 secs until **pmie**(1) is no longer running, usually called after `_service pmie stop`. If the process is running, calls [**\_wait\_process\_end**](#idx+funcs+waitprocessend).<br>Returns 0 on success, else returns 1.|
|<a id="idx+funcs+waitpmlogctl">**\_wait\_pmlogctl**</a>|Usage: **\_wait\_pmlogctl**<br>Wait up to 60 secs until **pmlogctl**(1)'s lock file (**$PCP_ETC_DIR***/pcp/pmlogger/lock*) has been removed.|
|<a id="idx+funcs+waitpmloggerend">**\_wait\_pmlogger\_end**</a>|Usage: **\_wait\_pmlogger\_end** \[*pid*]<br>Wait up to 10 secs until a **pmlogger**(1) instance is no longer running, usually called after `_service pmlogger stop`. The pmlogger instance is identified by *pid* else the primary pmlogger if *pid* is not specified. If the process is running, calls [**\_wait\_process\_end**](#idx+funcs+waitprocessend).<br>Returns 0 on success, else returns 1.|
|<a id="idx+funcs+waitpmproxyend">**\_wait\_pmproxy\_end**</a>|Usage: **\_wait\_pmproxy\_end**<br>Wait up to 10 secs until **pmproxy**(1) is no longer running, usually called after `_service pmproxy stop`. If the process is running, calls [**\_wait\_process\_end**](#idx+funcs+waitprocessend).<br>Returns 0 on success, else returns 1.|
|<a id="idx+funcs+waitprocessend">**\_wait\_process\_end**</a>|Usage: **\_wait\_process\_end** \[*tag*] *pid*<br>Wait up to 10 seconds for process *pid* to vanish. *tag* is used as a prefix for any output and defaults to **\_wait\_process\_end**. If process *pid* vanishes, return 0, else return 1 and output a message. If process *pid* has exited, but has not yet gone away because it has not been harvested by its parent process, then return 0 and further details are appended to [**$seq_full**](#idx+vars+seqfull).|
|<a id="idx+funcs+webapiheaderfilter">**\_webapi\_header\_filter**</a>|Usage: **\_webapi\_header\_filter**<br>This filter makes HTTP header responses from **pmproxy**(1) deterministic by replacing variable components (sizes, dates, context numbers, version numbers, ...) by constant strings and deleting some optional header lines.<br>The unfiltered input to **\_webapi\_header\_filter** is appended to [**$seq\_full**](#idx+vars+seqfull).|
|<a id="idx+funcs+webapiresponsefilter">**\_webapi\_response\_filter**</a>|Usage: **\_webapi\_response\_filter**<br>This filter makes HTTP responses from **pmproxy**(1) deterministic by replacing variable components (hostnames, ip addresses, sizes, dates, version numbers, ...) by constant strings and deleting some optional header lines and diagnostics.<br>The unfiltered input to **\_webapi\_response\_filter** is appended to [**$seq\_full**](#idx+vars+seqfull).|
|<a id="idx+funcs+withintolerance">**\_within\_tolerance**</a>|Usage: **\_within\_tolerance** *name* *observed* *expected* *mintol* \[*maxtol*] \[*-v*]<br>When tests report the values of performance metrics, especially from live systems, the acceptable values may fall within a range and in this case outputing the metric's value makes the test non-deterministic. This function determines if the *observed* value is within an acceptable range as defined by *expected* minus *mintol* to *expected* plus *maxtol*.<br>*maxtol* defaults to *mintol* and these arguments are percentages (with an optional **%** suffix to aid readability).<br>If the *observed* value is with the specified range the function returns 0 else the return value is 1.<br>The *-v* option outputs an explanation as well.|
|<a id="idx+funcs+writableprimarylogger">**\_writable\_primary\_logger**</a>|Usage: **\_writable\_primary\_logger**<br>Ensure the configuration file for the primary **pmlogger**(1) allows **pmlc**(1) to make state changes to dynamically add or delete metrics to be logged.<br>Assumes the configuration file is the default one generated by **pmlogconf**(1) and calls [**\_save\_config**](#idx+funcs+saveconfig) to save the old configuration file, so the test *must* call either [**\_restore\_\_primary\_logger**](#idx+funcs+restoreprimarylogger) or [**\_restore\_config**](#idx+funcs+restoreconfig) with the argument **$PCP_VAR_DIR***/config/pmlogger/config.default* before exiting.<br>Does not restart the primary **pmlogger**(1) instance, so the caller must do that before the changes take effect.|

<a id="pmda-install-and-remove"></a>
## 9.1 PMDA Install and Remove

Many QA tests are designed to exercise an individual [PMDA](#idx+pmda)
and each PMDA has its own **Install** and **Remove** script to
handle installation and removal (optional building, creation of help
text files, updating **pmcd**(1)'s configuration file and signalling
**pmcd**).  But most PMDAs are *not* installed by default, so special
care needs to be taken in these tests to:

- Preserve the state of the PMDA and its configuration if it is already installed.
- Install the PMDA with the configuration expected by the test.
- Remove the PMDA after testing is done.
- Re-install the PMDA with its original configuration if it was installed when the test started.

There are two templates used in QA tests, as described below.  Choose
one that matches your needs, and do **not** mix calls across Plan A and
Plan B.  If you have a choice, Plan A is favoured.

<a id="plan-a"></a>
### Plan A

Initially, call

<a id="idx+funcs+preparepmda">**\_prepare\_pmda**</a><br>
Usage: **\_prepare\_pmda** *pmda* \[*name*]<br>
Called before any PMDA changes are made to ensure the
state of the *pmda* PMDA will be restored at the end of the test when
the companion function [**\_cleanup\_pmda**](#idx+funcs+cleanuppmda)
is called in the test's **\_cleanup** function. *name* is the name of a metric in the
[PMNS](#idx+pmns) that belongs to the *pmda* PMDA, so it can be used to
probe the PMDA; if *name* is not provided, it defaults to *pmda*.

This will:

- note if the *pmda* PMDA is already installed
- call [**\_save\_config**](#idx+funcs+saveconfig) to save **pmcd**'s configuration file (**$PCP_PMCDCONF_PATH**)

And before the test exits, call

<a id="idx+funcs+cleanuppmda">**\_cleanup\_pmda**</a><br>
Usage: **\_cleanup\_pmda** *pmda* \[*install-config*]<br>
Called at the end of a test to restore the state of the *pmda*
PMDA to the state it was in when the companion
function [**\_prepare\_pmda**](#idx+funcs+preparepmda) was
called. *install-config* is an optional input file for the PMDA's
**Install** script to be used if the PMDA needs to be re-installed
(default is */dev/tty*).

This will:

- check and call `_exit 1` if **\_cleanup\_pmda** is called without a prior call to **\_prepare\_pmda**
- check and call `_exit 1` if **\_cleanup\_pmda** is called twice
- call [**\_restore\_config**](#idx+funcs+restoreconfig) to reinstate **pmcd**'s configuration file (**$PCP_PMCDCONF_PATH**)
- call [**\_restore\_auto\_restart**](#idx+funcs+restoreautorestart), [**\_service**](#idx+funcs+service) and [**\_wait\_for\_pmcd**](#idx+funcs+waitforpmcd) to restart **pmcd**
- call [**\_service**](#idx+funcs+service) and [**\_wait\_for\_pmlogger**](#idx+funcs+waitforpmlogger) to restart (the primary) **pmlogger**
- reinstall the PMDA if needed

<a id="plan-b"></a>
### Plan B

Initially, call

<a id="idx+funcs+preparepmdainstall">**\_prepare\_pmda\_install**</a><br>
Usage: **\_prepare\_pmda\_install** *pmda*<br>
Called before any PMDA changes are made to ensure the
state of the *pmda* PMDA will be restored at the end of the test when
the companion function [**\_restore\_pmda\_install**](#idx+funcs+restorepmdainstall)
is called in the test's **\_cleanup** function.

This will:

- call [**\_save\_config**](#idx+funcs+saveconfig) to save **pmcd**'s configuration file (**$PCP_PMCDCONF_PATH**)
- **cd** into the PMDA's directory (**$PCP_PMDAS_DIR/***pmda*)
- if there is a *Makefile*, run **make**(1) and return 1 if this fails
- run *./Remove* to remove the PMDA if it is already installed
- save **$PCP_VAR_DIR/config/***pmda***/***pmda***.conf** if it exists
- return 0

Note this leaves the calling shell in a different directory to the one
it was in when **\_prepare\_pmda\_install** was called.

And before the test exits, call

<a id="idx+funcs+restorepmdainstall">**\_restore\_pmda\_install**</a><br>
Usage: **\_restore\_pmda** *pmda* \[*reinstall*]<br>
Called at the end of a test to (sort of) restore the state of the *pmda*
PMDA to the state it was in when the companion
function [**\_prepare\_pmda\_install**](#idx+funcs+preparepmdainstall) was
called. If *reinstall* is not specified the PMDA's
**Install** script will be used if the PMDA needs to be re-installed with
input coming from */dev/tty*, else do not re-install the PMDA.

<a id="privatepmcd"></a>
## 9.2 **\_private\_pmcd**

Usage: **\_private\_pmcd**

This function starts a new **pmcd**(1) running privately as a non-daemon background process
listening on a non-standard port.  This **pmcd** is completely isolated, so other services
can continue to use the system's **pmcd** concurrent with testing of the private **pmcd**.
The new **pmcd** runs with the user id of the caller (not **pcp** or **root**).

On entry **$pmcd_args** is optionally set to any additional arguments for **pmcd**.

On return

- **$pmcd_pid** is the PID of the new **pmcd**.
- **$pmcd_port** is the port **pmcd** is listening on and this is also set in the environment as **$PMCD_PORT**.
- **$tmp***/pmcd.socket Unix domain socket for **pmcd** requests
- **$tmp***/pmcd.conf **pmcd**'s configuration file
- **$tmp***/pmcd.out stdout for **pmcd**
- **$tmp***/pmcd.err stderr for **pmcd**
- **$tmp***/pmcd.log  **pmcd**'s log file

To cleanup and terminate the private **pmcd** the test should do the following before exiting:
```bash
$ kill -TERM $pmcd_pid
```

<a id="triagewaitpoint"></a>
## 9.3 **\_triage\_wait\_point**

Usage: **\_triage\_wait\_point** \[*file*] *message*

If you have a QA test that needs triaging after it has done some setup
(e.g. start a QA version of a daemon, install a PMDA, or unpack a
"fake" set of kernel stats files, create an archive), then add a call
to **\_triage\_wait\_point** in the test once the setup has been done.

The *message* must be specified it will be echoed;
this allows the test to disclose useful information from the setup, e.g. a process ID or the path to where the magic files have been unpacked.

If *file* is missing, the default file name is **$seq***.wait*.  Specifying *file* allows for multiple wait points with different files as the guard so a test can be paused at more than one point of interest.

Now, to triage the test:

```bash
$ touch $seq.wait
$ ./$seq
```

and when **\_triage\_wait\_point** is called it will output *message*
and go into a sleep-check loop waiting
for **$seq***.wait* to disappear.
So at this point the test is paused and you can go poke a process, look
at a log file, check an archive, ...

When the triage is done,

```bash
$ rm $seq.wait
```
and the test will resume.

<a id="shell-functions-from-common.filter"></a>
# 10 Shell functions from *common.filter*

Because filtering output to produce deterministic results is such a
key part of the PCP QA methodology, a number of common filtering
functions are provided by *common.filter* as described below.

Except where noted, these functions as classical Unix "filters" reading
from standard input and writing to standard output.
<br>

|**Function**|**Input**|
|---|---|
|<a id="idx+funcs+cullduplines">**\_cull\_dup\_lines**</a>|TODO|
|<a id="idx+funcs+filterallpcpstart">**\_filterall\_pcp\_start**</a>|TODO|
|<a id="idx+funcs+filtercompilerbabble">**\_filter\_compiler\_babble**</a>|TODO|
|<a id="idx+funcs+filterconsole">**\_filter\_console**</a>|TODO|
|<a id="idx+funcs+filtercronscripts">**\_filter\_cron\_scripts**</a>|TODO|
|<a id="idx+funcs+filterdbg">**\_filter\_dbg**</a>|TODO|
|<a id="idx+funcs+filterdumpresult">**\_filter\_dumpresult**</a>|TODO|
|<a id="idx+funcs+filterinstall">**\_filter\_install**</a>|TODO|
|<a id="idx+funcs+filterls">**\_filter\_ls**</a>|TODO|
|<a id="idx+funcs+filteroptionallabels">**\_filter\_optional\_labels**</a>|TODO|
|<a id="idx+funcs+filteroptionalpmdainstances">**\_filter\_optional\_pmda\_instances**</a>|TODO|
|<a id="idx+funcs+filteroptionalpmdas">**\_filter\_optional\_pmdas**</a>|TODO|
|<a id="idx+funcs+filterpcprestart">**\_filter\_pcp\_restart**</a>|TODO|
|<a id="idx+funcs+filterpcpstartdistro">**\_filter\_pcp\_start\_distro**</a>|TODO|
|<a id="idx+funcs+filterpcpstart">**\_filter\_pcp\_start**</a>|TODO|
|<a id="idx+funcs+filterpcpstop">**\_filter\_pcp\_stop**</a>|TODO|
|<a id="idx+funcs+filterpmcdlog">**\_filter\_pmcd\_log**</a>|a *pmcd.log* file from **pmcd**(1).|
|<a id="idx+funcs+filterpmdainstall">**\_filter\_pmda\_install**</a>|TODO|
|<a id="idx+funcs+filterpmdaremove">**\_filter\_pmda\_remove**</a>|TODO|
|<a id="idx+funcs+filterpmdumplog">**\_filter\_pmdumplog**</a>|TODO|
|<a id="idx+funcs+filterpmdumptext">**\_filter\_pmdumptext**</a>|TODO|
|<a id="idx+funcs+filterpmielog">**\_filter\_pmie\_log**</a>|a *pmie.log* file from **pmie**(1).|
|<a id="idx+funcs+filterpmiestart">**\_filter\_pmie\_start**</a>|TODO|
|<a id="idx+funcs+filterpmiestop">**\_filter\_pmie\_stop**</a>|TODO|
|<a id="idx+funcs+filterpmproxylog">**\_filter\_pmproxy\_log**</a>|a *pmproxy.log* file from **pmproxy**(1).|
|<a id="idx+funcs+filterpmproxystart">**\_filter\_pmproxy\_start**</a>|TODO|
|<a id="idx+funcs+filterpmproxystop">**\_filter\_pmproxy\_stop**</a>|TODO|
|<a id="idx+funcs+filterpost">**\_filter\_post**</a>|TODO|
|<a id="idx+funcs+filterslowpmie">**\_filter\_slow\_pmie**</a>|TODO|
|<a id="idx+funcs+filtertoppmns">**\_filter\_top\_pmns**</a>|TODO|
|<a id="idx+funcs+filtertortureapi">**\_filter\_torture\_api**</a>|TODO|
|<a id="idx+funcs+filterviews">**\_filter\_views**</a>|TODO|
|<a id="idx+funcs+instancesfilterany">**\_instances\_filter\_any**</a>|TODO|
|<a id="idx+funcs+instancesfilterexact">**\_instances\_filter\_exact**</a>|TODO|
|<a id="idx+funcs+instancesfilternonzero">**\_instances\_filter\_nonzero**</a>|TODO|
|<a id="idx+funcs+instvaluefilter">**\_inst\_value\_filter**</a>|TODO|
|<a id="idx+funcs+quotefilter">**\_quote\_filter**</a>|TODO|
|<a id="idx+funcs+showpmieerrors">**\_show\_pmie\_errors**</a>|TODO|
|<a id="idx+funcs+showpmieexit">**\_show\_pmie\_exit**</a>|TODO|
|<a id="idx+funcs+sortpmdumplogd">**\_sort\_pmdumplog\_d**</a>|TODO|
|<a id="idx+funcs+valuefilterany">**\_value\_filter\_any**</a>|TODO|
|<a id="idx+funcs+valuefilternonzero">**\_value\_filter\_nonzero**</a>|TODO|

<a id="helper-scripts"></a>
# 11 Helper scripts

There are a large number of shell scripts in the QA directory that are
intended for common QA development and triage tasks beyond simply
running tests with [**check**]((#check-script).

<a id="summary"></a>
## Summary

|**Command**|**Description**|
|---|---|
|<a id="idx+cmds+all-by-group">**all-by-group**</a>|Report all tests (excluding those tagged **:retired** or **:reserved**) in *group* sorted by group.|
|<a id="idx+cmds+appchange">**appchange**</a>|Usage: **appchange** \[**-c**] *app1* \[*app2* ...]<br>Recheck all QA tests that appear to use the test application src/*app1* or *src/app2* or ... *${TMPDIR:-/tmp}/appcache* is a cache of mappings between test sequence numbers and uses, for all applications in *src/* \... **appchange** will build the cache if it is not already there, use **-c** to clear and rebuild the cache.|
|<a id="idx+cmds+bad-by-group">**bad-by-group**</a>|Use the *\*.out.bad* files to report failures by group.|
|<a id="idx+cmds+check.app.ok">**check.app.ok**</a>|Usage: **check.app.ok** *app*<br>When the test application *src/app.c* (or similar) has been changed, this script<br>(a) remakes the application and checks **make**(1) status, and<br>(b) finds all the tests that appear to run the *src/app* application and runs [**check**](#check-script) for these tests.|
|<a id="idx+cmds+check-auto">**check-auto**</a>|Usage: **check-auto** \[*seqno* ...]<br>Check that if a QA script uses **\_stop\_auto\_restart** for a (**systemd**) service, it also uses **\_restore\_auto\_restart** (preferably in \_cleanup()). If no *seqno* options are given then check all tests.|
|<a id="idx+cmds+check-flakey">**check-flakey**</a>|Usage: **check-flakey** \[*seqno* ...\]<br>Recheck failed tests and try to classify them as "flakey" if they pass now, or determine if the failure is "hard" (same *seqno.out.bad*) or some other sort of non-deterministic failure. If no *seqno* options are given then check all tests with a *\*.out.bad* file.|
|<a id="idx+cmds+check-group">**check-group**</a>|Usage: **check-group** *query*<br>Check the *group* file and test scripts for a specific *query* that is assumed to be **both** the name of a command that appears in the test scripts (or part of a command, e.g. **valgrind** in **\_run\_valgrind**) and the name of a group in the *group* file. Report differences, e.g. *command* appears in the *group* file for a specific test but is not apparently used in that test, or *command* is used in a specific test but is not included in the *group* file entry for that test.<br>There are some special cases to handle the pcp-foo commands, aliases and [PMDAs](#idx+pmda) ... refer to **check-group** for details.<br>Special control lines like:<br>`# check-group-include: group ...`<br>`# check-group-exclude: group ...`<br>may be embedded in test scripts to over-ride the heuristics used by **check-group**.|
|<a id="idx+cmds+check-pdu-coverage">**check-pdu-coverage**</a>|Check that PDU-related QA apps in *src* provide full coverage of all current PDU types.|
|<a id="idx+cmds+check-setup">**check-setup**</a>|Check QA environment is as expected. Documented in *README* but not used otherwise.|
|<a id="idx+cmds+check-vars">**check-vars**</a>|Check shell variables across the *common\** "include" files and the scripts used to run and manage QA. For the most part, the *common\** files should use a "\_\_" prefix for shell variables\[2] to insulate them from the use of arbitrarily name shell variables in the QA tests themselves (all of which "source" multiple of the *common\** files). **check-vars** also includes some exceptions which are a useful cross-reference.|
|<a id="idx+cmds+cull-pmlogger-config">**cull-pmlogger-config**</a>|Cull he default **pmlogger**(1) configuration (**$PCP\_VAR\_DIR***/config/pmlogger/config.default) *to remove any **zeroconf** proc metric logging that threatens to fill the filesystem on small QA machines.|
|<a id="idx+cmds+daily-cleanup">**daily-cleanup**</a>|Run from [**check**](#check-script), this script will try to make sure the **pmlogger\_daily**(1) work has really been done; this is important for QA VMs that are only booted for QA and tend to miss the nightly **pmlogger\_daily**(1) run and this may lead to QA test failure.|
|<a id="idx+cmds+find-app">**find-app**</a>|Usage: **find-app** \[**-f**] *app* ...<br>Find and report tests that use any of the QA applications *src/app* ...<br>The **-f** argument changes the interpretation of *app* from *src/app* to "all the *src/\** programs" that call the function *app*.|
|<a id="idx+cmds+find-bound">**find-bound**</a>|Usage: **find-bound** *archive* *timestamp* *metric* \[*instance*]<br>Scan *archive* for values of *metric* (optionally constrained to the one *instance*) within the interval *timestamp* (in the format HH:MM:SS, as per **pmlogdump**(1) and assuming a timezone as per **-z**).|
|<a id="idx+cmds+find-metric">**find-metric**</a>|Usage: **find-metric** \[**-a**\|**-h**] *pattern* ...<br>Search for metrics with name or metadata that matches *pattern*. With **-h** interrogate the local **pmcd**(1), else with **-a** (the default) search all the QA archives in the directories *archive* and *tmparch*.<br>Multiple pattern arguments are treated as a disjunction in the search which uses **grep**(1) style regular expressions. Metadata matches are against the **pminfo**(1) **-d** output for the type, instance domain, semantics, and units.|
|<a id="idx+cmds+flakey-summary">**flakey-summary**</a>|Assuming the output from **check-flakey** has been kept for multiple QA runs across multiple hosts and saved in a file called *flakey*, this script will summarize the test failure classifications.|
|<a id="idx+cmds+getpmcdhosts">**getpmcdhosts**</a>|Usage: **getpmcdhosts** *lots-of-options*<br>Find a remote host matching a selection criteria based on hardware, operating system, installed [PMDA](#idx+pmda), primary logger running, etc. Use<br>`$ ./getpmcdhosts -?`<br>to see all options.|
|<a id="idx+cmds+grind">**grind**</a>|Usage: **grind** *seqno* \[...]<br>Run select test(s) in an loop until one of them fails and produces a *.out.bad* file. Stop with Ctl-C or for a more orderly end after the current iteration<br>`$ touch grind.stop`|
|<a id="idx+cmds+grind-pmda">**grind-pmda**</a>|Usage: **grind-pmda** *pmda* *seqno* \[...]<br>Exercise the *pmda* [PMDA](#idx+pmda) by running the PMDA's **Install** script, then using [**check**](#check-script) to run all the selected tests, checking that the PMDA is still installed, running the PMDA's **Remove** script, then running the selected tests again and checking that the PMDA is still **not** installed.|
|<a id="idx+cmds+group-stats">**group-stats**</a>|Report test frequency by group, and report any group name anomalies.|
|<a id="idx+cmds+mk.localconfig">**mk.localconfig**</a>|Recreate the *localconfig* file that provides the platform and PCP version information and the *src/localconfig.h* file that can be used by C programs in the *src* directory.|
|**mk.logfarm**|See the [**mk.logfarm**](#mk.logfarm-script) section.|
|**mk.qa\_hosts**|See the [**mk.qa\_hosts**](#mk.qahosts-script) section below.|
|<a id="idx+cmds+mk.variant">**mk.variant**</a>|Usage: **mk.variant** *seqno*<br>Sometimes a test has no choice other than to produce different output on different platforms. This script may be used to convert an existing test to accommodate multiple *seqno.out* files.|
|**new**|See the [**new**](#the-new-script) section above.|
|<a id="idx+cmds+new-dup">**new-dup**</a>|Usage: **new-dup** \[**-n**] *seqno*<br>Make a copy of the test *seqno* using a new test number as assigned by [**new**](#the-new-script), including rewriting the old *seqno* in the new test and its new *.out* file. **-n** is "show me" mode and no changes are made.|
|<a id="new-grind"></a><a id="idx+cmds+new-grind">**new-grind**</a>|Usage: **new-grind** \[**-n**] \[**-v**] *seqno*<br>Make a copy of the test *seqno* using a new test number as assigned by [**new**](#the-new-script) and arrange matters so the new test runs the old test but selects the **valgrind**(1) sections of that test. **-n** is "show me" mode and no changes are made, use **-v** for more verbosity.|
|<a id="idx+cmds+new-seqs">**new-seqs**</a>|Report the unallocated blocks of test sequence numbers from the *group* file.|
|<a id="idx+cmds+really-retire">**really-retire**</a>|Usage: **really-retire** *seqno* \[...]<br>Mark the selected tests as **:retired** in the *group* file and then replace the test and its *.out* file with boilerplate text that explains what has happened and unilaterally calls [**\_notrun**](#idx+funcs+notrun) (in case the test is ever really run).|
|<a id="idx+cmds+recheck">**recheck**</a>|Usage: **recheck** \[**-t**] \[*options*] \[*seqno* ...\]<br>Run [**check**](#check-script) again for failed tests. If no *seqno* options are given then check all tests with a *\*.out.bad* file. By default tests that failed last time and were classified as **triaged** will not be rerun, but **-t** overrides this. Other *options* are any command line options that [**check**](#check-script) understands.|
|<a id="idx+cmds+remake">**remake**</a>|Usage: **remake** \[*options*] *seqno* \[...]<br>Remake the *.out* file for the specified test(s). Command line parsing is the same as [**check**](#check-script) so *seqno* can be a single test sequence number, or a range, or a **-g** *group* specification. Similarly **-l** selects **diff**(1) rather than a graphical diff tool to show the differences.<br>Since the *seqno.out* files are precious and reflect the state of the qualified and expected output, they should typically not be changed unless some change has been made to the *seqno* test or the applications the test runs produce different output or the filters in the test have changed.|
|<a id="idx+cmds+sameas">**sameas**</a>|Usage: **sameas** *seqno* \[...]<br>See if *seqno.out* and *seqno.out.bad* are identical except for line ordering. Useful to detect cases where non-determinism is caused by the order in which subtests were run, e.g. sensitive to directory entry order in the filesystem or metric name order in the [PMNS](#idx+pmns).|
|<a id="idx+cmds+var-use">**var-use**</a>|Usage: **var-use** *var* \[*seqno* ...]<br>Find assignment and uses of the shell variable \[**$**]*var* in tests. If *seqno* not specified, search all tests.|

<br>
\[2] If all shells supported the **local** keyword for variables we could use that, but that's not the case across all the platforms PCP runs on, so the "\_\_" prefix model is a weak substitute for proper variable scoping.

<a id="mk.logfarm-script"></a>
## 11.1 <a id="idx+cmds+mk.logfarm">**mk.logfarm**</a> script

Usage: **mk.logfarm** \[**-c** *config*] *rootdir*

The **mk.logfarm** script creates a forest of archives suitable for
use with **pmlogger\_daily**(1) in tests.

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
## 11.2 <a id="idx+cmds+mk.qahosts">**mk.qa\_hosts**</a> script

The **mk.qa\_hosts** script includes heuristics for selecting
and sorting the list of potential remote PCP QA hosts
*qa\_hosts.primary*).
Refer to the comments in *qa\_hosts.primary*,
and make appropriate changes.

The heuristics use the domain name for
the current host to choose a set of hosts that can be considered
when running distributed tests, e.g. **pmlogger**(1) locally and
**pmcd**(1) on a remote host. Anyone wishing to do this sort of
testing (it does not happen in the github CI and QA actions) will
need to figure out how to append control lines in the
*qa\_hosts.primary* file.

**mk.qa\_hosts** is run from *GNUmakefile* so once created, *qa\_hosts*
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
|<a id="idx+funcs+checkvalgrind">**\_check\_valgrind**</a>|Usage: **\_check\_valgrind**<br>Check if **valgrind**(1) is installated and call [**\_notrun**](#idx+funcs+notrun) if not. Must be called after [**$seq**](#idx+vars+seq) is assigned and before calling [**\_check\_helgrind**](#idx+funcs+checkhelgrind), or [**\_run\_valgrind**](#idx+funcs+runvalgrind), or [**\_run\_helgrind**](#idx+funcs+runhelgrind), or using [**$valgrind\_clean\_assert**](#idx+vars+valgrindcleanassert).|
|<a id="idx+funcs+filtervalgrind">**\_filter\_valgrind**</a>|TODO|
|<a id="idx+funcs+prefervalgrind">**\_prefer\_valgrind**</a>|TODO|
|<a id="idx+funcs+runvalgrind">**\_run\_valgrind**</a>|TODO|
|<a id="idx+funcs+filtervalgrindpossibly">**\_filter\_valgrind\_possibly**</a>|TODO|
|<a id="idx+vars+valgrindcleanassert">**$\_valgrind\_filter\_assert**</a>|A shell variable, not a function, but provides similar functionality to [**\_run\_valgrind**](#idx+funcs+runvalgrind).<br>Typical usage would be:<br>`\_check\_valgrind`<br>`...`<br>`$valgrind\_filter\_assert TODO`|

<a id="using-helgrind"></a>
# 14 Using helgrind

TODO. helgrind-suppress

**common.check** includes the following shell functions to assist when
using **helgrind**(1) in a QA test.

|**Function**|**Description**|
|---|---|
|<a id="idx+funcs+checkhelgrind">**\_check\_helgrind**</a>|TODO|
|<a id="idx+funcs+filterhelgrind">**\_filter\_helgrind**</a>|TODO|
|<a id="idx+funcs+runhelgrind">**\_run\_helgrind**</a>|TODO|


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

PCP employs a client-server architecture, and so some parts of the
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
3. a login for the user **pcpqa** needs to be created, and then set up in such a way that **ssh**(1) and **scp**(1) will work without the need for any password, i.e. these sorts of commands<br>`$ ssh pcpqa@pcp-qa-host some-command`<br>`$ scp some-file pcpqa@pcp-qa-host:some-dir`<br>must work correctly when run from the local host with no interactive input and no Password: prompt<br><br>On Selinux systems it may be necessary to execute the following command to make this work:<br>`$ sudo chcon -R unconfined_u:object_r:ssh_home_t:s0 ~pcpqa/.ssh`<br>so that the ssh_home\_t attribute is set on ~pcpqa/.ssh and all the files below there.<br><br>The **pcpqa** user's environment must also be initialized so that their shell's path includes all of the PCP binary directories (identify these with `$ grep BIN /etc/pcp.conf`), so that all PCP commands are executable without full pathnames.  Of most concern would be auxiliary directory (usually */usr/lib/pcp/bin*, */usr/share/pcp/bin* or */usr/libexec/pcp/bin*) where commands like **pmlogger\_daily**(1), **pmnsadd**(1), **newhelp**(1), **mkaf**(1)etc.) are installed.<br><br>And finally, the **pcpqa** user needs to be included in the group **pcp** in */etc/group*.

Once you've setup the remote PCP QA hosts and modified *common.config*
and *qa\_hosts.primary* locally, then run validate the setup using [check-setup](#check-setup):

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
|<a id="idx+vars+pcpqaclosexserver">**$PCPQA\_CLOSE\_X\_SERVER**</a>|The **$DISPLAY** setting for an X server that is willing to accept connections from X clients running on the local machine. This is optional, and if not set any QA tests dependent on this will be skipped. See also the [**\_check\_display**](#idx+funcs+checkdisplay) shell function.|
|<a id="idx+vars+pcpqadesktophack">**$PCPQA\_DESKTOP\_HACK**</a>|Set to **true** to enable a workaround for babble from Qt applications on some platforms, see the [**\_check\_display**](#idx+funcs+checkdisplay) and [**\_clean\_display**](#idx+funcs+cleandisplay) shell functions.|
|<a id="idx+vars+pcpqafarpmcd">**$PCPQA\_FAR\_PMCD**</a>|The hostname for a host running **pmcd**(1), but the host is preferably a long way away (in terms of TCP/IP latency) for timing tests. This is optional, and if not set any QA tests dependent on this will be skipped.|
|<a id="idx+vars+pcpqahyphenhost">**$PCPQA\_HYPHEN\_HOST**</a>|The hostname for a host running **pmcd**(1), with a hyphen (-) in the hostname. This is optional, and if not set any QA tests dependent on this will be skipped.|

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
[**\_filter\_top\_pmns**](#idx+funcs+filtertoppmns) function to strip the top-level name components
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
.\" +ok+ _restore_auto_restart _stop_auto_restart
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
.\" +ok+ _get_word_size _libvirt_is_ok _make_helptext
.\" +ok+ _path_readable _set_dsosuffix _wait_for_pmcd _wait_for_pmie
.\" +ok+ _wait_for_port _wait_pmcd_end _wait_pmie_end _wait_pmlogctl
.\" +ok+ _avail_metric _check_metric _check_search _check_series
.\" +ok+ _cleanup_pmda _filter_views _host_to_fqdn _prepare_pmda
.\" +ok+ _private_pmcd _quote_filter _run_helgrind _run_valgrind _all_ipaddrs
.\" +ok+ _check_agent _domain_name _filter_post _host_to_ipv
.\" +ok+ _need_metric GNUlocaldefs _ps_tcp_port _save_config _sighup_pmcd
.\" +ok+ _triage_pmcd _arch_start _check_core
.\" +ok+ _filter_dbg
.\" +ok+ _get_config _get_endian _machine_id _pstree_all
.\" +ok+ _filter_ls _localhost _filesize _get_fqdn _get_port gitignore
.\" +ok+ _try_pmlc _check_ dinking addrs _fail repo _ipv Sssh pre TT
.\" +ok+ br {from <br>}
.\" +ok+ fc te {selinux file suffixes}
.\" +ok+ PCPQA_SYSTEMD {from $PCPQA_SYSTEMD}
.\" +ok+ PCPQA_IN_CI {from $PCPQA_IN_CI}
.\" +ok+ bit_platform {from _check_64bit_platform }
.\" +ok+ Makepkgs sudoers NOPASSWD {sudo config} aka
.\" +ok+ object_r ssh_home ssh_home_t unconfined_u {AVC}
.\" +ok+ pcpqa PCPQA qahosts {from mk.qahosts} traiged {from triaged file}
.\" +ok+ scp {command} seqfull {from $seq_full} SSS {from MM.SSS}
.\" +ok+ xdpyinfo {command} chcon {command} cli {from -cli} href mailto {html}
.\" +ok+ subtests xdg {from xdg-runtime} xhost {command}

.\" +ok+ ${functionnamesstrippedofunderscores in {}(...) link}
.\" +ok+ allhostnames allipaddrs availmetric bitplatform changeconfig
.\" +ok+ checkagent checkcore checkdisplay checkfreespace checkhelgrind
.\" +ok+ checkjobscheduler checkkeyserver checkkeyserverping
.\" +ok+ checkkeyserverversion checkkeyserverversionoffline
.\" +ok+ checklocalprimaryarchive checkmetric checksearch checkseries
.\" +ok+ checkvalgrind cleandisplay cleanuppmda cullduplines
.\" +ok+ disableloggers domainname filesize filterallpcpstart
.\" +ok+ filtercompilerbabble filterconsole filtercronscripts filterdbg
.\" +ok+ filterdumpresult filterhelgrind filterinitdistro filterinstall
.\" +ok+ filterls filteroptionallabels filteroptionalpmdainstances
.\" +ok+ filteroptionalpmdas filterpcprestart filterpcpstart
.\" +ok+ filterpcpstartdistro filterpcpstop filterpmcdlog
.\" +ok+ filterpmdainstall filterpmdaremove filterpmdumplog
.\" +ok+ filterpmdumptext filterpmielog filterpmiestart filterpmiestop
.\" +ok+ filterpmproxylog filterpmproxystart
.\" +ok+ filterpmproxystop filterpost filterslowpmie filtertoppmns
.\" +ok+ filtertortureapi filtervalgrind filtervalgrindpossibly
.\" +ok+ filterviews findfreeport findkeyservermodules
.\" +ok+ findkeyservername findkeyserversearch getconfig getendian
.\" +ok+ getfqdn getlibpcpconfig getport getprimaryloggerpid
.\" +ok+ getwordsize hosttofqdn hosttoipaddr hosttoipv installated
.\" +ok+ instancesfilterany instancesfilterexact instancesfilternonzero
.\" +ok+ instvaluefilter ipaddrtohost libexec libvirtisok logpush
.\" +ok+ machineid makehelptext makeprocstat needmetric
.\" +ok+ archstart

.\" +ok+ ${parts of function_names stripped of underscores => _names}
.\" +ok+ _agent _all _any _api _arch _archive _assert _auto _avail
.\" +ok+ _change _check _ci _clean _compiler _config _console _container
.\" +ok+ _control _core _cron _cull _d _daily _dbg _disable _writable
.\" +ok+ _word _within _webapi _wait _views _version _value _valgrind
.\" +ok+ _up _try _triage _torture _top _tolerance _to _tcp _systemctl
.\" +ok+ _stop _status _stat _start _sort _slow _size _sighup _show
.\" +ok+ _set _server _series _search _scripts _scheduler _save _run
.\" +ok+ _restore _restart _response _remove _readable _quote _pstree
.\" +ok+ _ps _process _proc _private _primary _prepare _prefer _post
.\" +ok+ _possibly _port _point _pmproxy _pmns _pmlogger _pmlogctl
.\" +ok+ _pmlc _pmie _pmdumptext _pmdumplog _pmdas _pmda _pmcd
.\" +ok+ _platform _ping _pid _pcp _path _optional _oneline _ok _offline
.\" +ok+ _nonzero _need _name _modules _mmv _metrics _metric _make
.\" +ok+ _machine _ls _loggers _logger _logfile _log _local _lines
.\" +ok+ _libvirt _libpcp _labels _key _job _is _ipaddrs _ipaddr
.\" +ok+ _instances _install _inst _init _in _id _hosts _hostnames
.\" +ok+ _host _helptext _helgrind _header _get _full _freespace _free
.\" +ok+ _fqdn _for _find _filterall _filter _exact _errors _endian
.\" +ok+ _end _dup _dumpresult _dsosuffix _domain _distro _display
.\" +ok+ _babble

.\" +ok+ ${parts of shell $VARIABLE_NAMES stripped of underscores => _NAMES}
.\" +ok+ _AWK _CI _CLOSE _DESKTOP _DIR _FAR _HACK _HOST _HYPHEN _IN
.\" +ok+ _LOG _PMCD _PROG _RUNTIME _SERVER _SYSTEMD _VAR _X XDG

.\" +ok+ {function arguments}
.\" +ok+ onoff
.\" +ok+ pathreadable pcpqaclosexserver pcpqadesktophack pcpqafarpmcd
.\" +ok+ pcpqahyphenhost pcpqainci pcpqasystemd pcpstar pidincontainer
.\" +ok+ prefervalgrind preparepmda preparepmdainstall preparepmdammv
.\" +ok+ privatepmcd pstcpport pstreeall pstreeoneline quotefilter
.\" +ok+ removejobscheduler restoreautorestart restoreconfig
.\" +ok+ restorejobscheduler restoreloggers restorepmdainstall
.\" +ok+ restorepmdammv restorepmloggercontrol restoreprimarylogger
.\" +ok+ runhelgrind runvalgrind saveconfig setdsosuffix
.\" +ok+ showpmieerrors showpmieexit sighuppmcd sortpmdumplogd
.\" +ok+ startuppmlogger stopautorestart systemctlstatus
.\" +ok+ triagepmcd triagewaitpoint trypmlc valgrindcleanassert
.\" +ok+ valuefilterany valuefilternonzero waitforpmcd
.\" +ok+ waitforpmcdstop waitforpmie waitforpmlogger waitforpmproxy
.\" +ok+ waitforpmproxylogfile waitforpmproxymetrics waitforport
.\" +ok+ waitpmcdend waitpmieend waitpmlogctl waitpmloggerend
.\" +ok+ waitpmproxyend waitprocessend webapiheaderfilter
.\" +ok+ webapiresponsefilter withintolerance writableprimarylogger

-->

<!--idxctl
General Index|Commands and Scripts|Shell Functions|Shell Variables|Files
!|cmds|funcs|vars|files
-->
<a id="index"></a>
# Index

|**General Index**|**Shell Functions ...**|**Shell Functions ...**|**Shell Functions ...**|
|---|---|---|---|
|[PCP](#idx+pcp)|[\_check\_key\_server](#idx+funcs+checkkeyserver)|[\_find\_key\_server\_name](#idx+funcs+findkeyservername)|[\_stop\_auto\_restart](#idx+funcs+stopautorestart)|
|[PMAPI](#idx+pmapi)|[\_check\_key\_server\_ping](#idx+funcs+checkkeyserverping)|[\_find\_key\_server\_search](#idx+funcs+findkeyserversearch)|[\_systemctl\_status](#idx+funcs+systemctlstatus)|
|[PMCD](#idx+pmcd)|[\_check\_key\_server\_version](#idx+funcs+checkkeyserverversion)|[\_get\_config](#idx+funcs+getconfig)|[\_triage\_pmcd](#idx+funcs+triagepmcd)|
|[PMDA](#idx+pmda)|[\_check\_key\_server\_version\_offline](#idx+funcs+checkkeyserverversionoffline)|[\_get\_endian](#idx+funcs+getendian)|[\_triage\_wait\_point](#idx+funcs+triagewaitpoint)|
|[PMNS](#idx+pmns)|[\_check\_local\_primary\_archive](#idx+funcs+checklocalprimaryarchive)|[\_get\_fqdn](#idx+funcs+getfqdn)|[\_try\_pmlc](#idx+funcs+trypmlc)|
|**Commands and Scripts**|[\_check\_metric](#idx+funcs+checkmetric)|[\_get\_libpcp\_config](#idx+funcs+getlibpcpconfig)|[\_value\_filter\_any](#idx+funcs+valuefilterany)|
|[all-by-group](#idx+cmds+all-by-group)|[\_check\_search](#idx+funcs+checksearch)|[\_get\_port](#idx+funcs+getport)|[\_value\_filter\_nonzero](#idx+funcs+valuefilternonzero)|
|[appchange](#idx+cmds+appchange)|[\_check\_series](#idx+funcs+checkseries)|[\_get\_primary\_logger\_pid](#idx+funcs+getprimaryloggerpid)|[\_wait\_for\_pmcd](#idx+funcs+waitforpmcd)|
|[bad-by-group](#idx+cmds+bad-by-group)|[\_check\_valgrind](#idx+funcs+checkvalgrind)|[\_get\_word\_size](#idx+funcs+getwordsize)|[\_wait\_for\_pmcd\_stop](#idx+funcs+waitforpmcdstop)|
|[check](#idx+cmds+check)|[\_clean\_display](#idx+funcs+cleandisplay)|[\_host\_to\_fqdn](#idx+funcs+hosttofqdn)|[\_wait\_for\_pmie](#idx+funcs+waitforpmie)|
|[check.app.ok](#idx+cmds+check.app.ok)|[\_cleanup\_pmda](#idx+funcs+cleanuppmda)|[\_host\_to\_ipaddr](#idx+funcs+hosttoipaddr)|[\_wait\_for\_pmlogger](#idx+funcs+waitforpmlogger)|
|[check-auto](#idx+cmds+check-auto)|[\_cull\_dup\_lines](#idx+funcs+cullduplines)|[\_host\_to\_ipv6addrs](#idx+funcs+hosttoipv6addrs)|[\_wait\_for\_pmproxy](#idx+funcs+waitforpmproxy)|
|[check-flakey](#idx+cmds+check-flakey)|[\_disable\_loggers](#idx+funcs+disableloggers)|[\_instances\_filter\_any](#idx+funcs+instancesfilterany)|[\_wait\_for\_pmproxy\_logfile](#idx+funcs+waitforpmproxylogfile)|
|[check-group](#idx+cmds+check-group)|[\_domain\_name](#idx+funcs+domainname)|[\_instances\_filter\_exact](#idx+funcs+instancesfilterexact)|[\_wait\_for\_pmproxy\_metrics](#idx+funcs+waitforpmproxymetrics)|
|[check-pdu-coverage](#idx+cmds+check-pdu-coverage)|[\_exit](#idx+funcs+exit)|[\_instances\_filter\_nonzero](#idx+funcs+instancesfilternonzero)|[\_wait\_for\_port](#idx+funcs+waitforport)|
|[check-setup](#idx+cmds+check-setup)|[\_fail](#idx+funcs+fail)|[\_inst\_value\_filter](#idx+funcs+instvaluefilter)|[\_wait\_pmcd\_end](#idx+funcs+waitpmcdend)|
|[check-vars](#idx+cmds+check-vars)|[\_filesize](#idx+funcs+filesize)|[\_ipaddr\_to\_host](#idx+funcs+ipaddrtohost)|[\_wait\_pmie\_end](#idx+funcs+waitpmieend)|
|[cull-pmlogger-config](#idx+cmds+cull-pmlogger-config)|[\_filterall\_pcp\_start](#idx+funcs+filterallpcpstart)|[\_ipv6\_localhost](#idx+funcs+ipv6localhost)|[\_wait\_pmlogctl](#idx+funcs+waitpmlogctl)|
|[daily-cleanup](#idx+cmds+daily-cleanup)|[\_filter\_compiler\_babble](#idx+funcs+filtercompilerbabble)|[\_libvirt\_is\_ok](#idx+funcs+libvirtisok)|[\_wait\_pmlogger\_end](#idx+funcs+waitpmloggerend)|
|[find-app](#idx+cmds+find-app)|[\_filter\_console](#idx+funcs+filterconsole)|[\_machine\_id](#idx+funcs+machineid)|[\_wait\_pmproxy\_end](#idx+funcs+waitpmproxyend)|
|[find-bound](#idx+cmds+find-bound)|[\_filter\_cron\_scripts](#idx+funcs+filtercronscripts)|[\_make\_helptext](#idx+funcs+makehelptext)|[\_wait\_process\_end](#idx+funcs+waitprocessend)|
|[find-metric](#idx+cmds+find-metric)|[\_filter\_dbg](#idx+funcs+filterdbg)|[\_make\_proc\_stat](#idx+funcs+makeprocstat)|[\_webapi\_header\_filter](#idx+funcs+webapiheaderfilter)|
|[flakey-summary](#idx+cmds+flakey-summary)|[\_filter\_dumpresult](#idx+funcs+filterdumpresult)|[\_need\_metric](#idx+funcs+needmetric)|[\_webapi\_response\_filter](#idx+funcs+webapiresponsefilter)|
|[getpmcdhosts](#idx+cmds+getpmcdhosts)|[\_filter\_helgrind](#idx+funcs+filterhelgrind)|[\_notrun](#idx+funcs+notrun)|[\_within\_tolerance](#idx+funcs+withintolerance)|
|[grind](#idx+cmds+grind)|[\_filter\_init\_distro](#idx+funcs+filterinitdistro)|[\_path\_readable](#idx+funcs+pathreadable)|[\_writable\_primary\_logger](#idx+funcs+writableprimarylogger)|
|[grind-pmda](#idx+cmds+grind-pmda)|[\_filter\_install](#idx+funcs+filterinstall)|[\_pid\_in\_container](#idx+funcs+pidincontainer)|**Shell Variables**|
|[group-stats](#idx+cmds+group-stats)|[\_filter\_ls](#idx+funcs+filterls)|[\_prefer\_valgrind](#idx+funcs+prefervalgrind)|[$here](#idx+vars+here)|
|[mk.localconfig](#idx+cmds+mk.localconfig)|[\_filter\_optional\_labels](#idx+funcs+filteroptionallabels)|[\_prepare\_pmda](#idx+funcs+preparepmda)|[$PCP\_\*](#idx+vars+pcpstar)|
|[mk.logfarm](#idx+cmds+mk.logfarm)|[\_filter\_optional\_pmda\_instances](#idx+funcs+filteroptionalpmdainstances)|[\_prepare\_pmda\_install](#idx+funcs+preparepmdainstall)|[$PCPQA\_CLOSE\_X\_SERVER](#idx+vars+pcpqaclosexserver)|
|[mk.qa\_hosts](#idx+cmds+mk.qahosts)|[\_filter\_optional\_pmdas](#idx+funcs+filteroptionalpmdas)|[\_prepare\_pmda\_mmv](#idx+funcs+preparepmdammv)|[$PCPQA\_DESKTOP\_HACK](#idx+vars+pcpqadesktophack)|
|[mk.variant](#idx+cmds+mk.variant)|[\_filter\_pcp\_restart](#idx+funcs+filterpcprestart)|[\_private\_pmcd](#idx+funcs+privatepmcd)|[$PCPQA\_FAR\_PMCD](#idx+vars+pcpqafarpmcd)|
|[new](#idx+cmds+new)|[\_filter\_pcp\_start](#idx+funcs+filterpcpstart)|[\_ps\_tcp\_port](#idx+funcs+pstcpport)|[$PCPQA\_HYPHEN\_HOST](#idx+vars+pcpqahyphenhost)|
|[new-dup](#idx+cmds+new-dup)|[\_filter\_pcp\_start\_distro](#idx+funcs+filterpcpstartdistro)|[\_pstree\_all](#idx+funcs+pstreeall)|[$PCPQA\_IN\_CI](#idx+vars+pcpqainci)|
|[new-grind](#idx+cmds+new-grind)|[\_filter\_pcp\_stop](#idx+funcs+filterpcpstop)|[\_pstree\_oneline](#idx+funcs+pstreeoneline)|[$PCPQA\_SYSTEMD](#idx+vars+pcpqasystemd)|
|[new-seqs](#idx+cmds+new-seqs)|[\_filter\_pmcd\_log](#idx+funcs+filterpmcdlog)|[\_quote\_filter](#idx+funcs+quotefilter)|[$seq](#idx+vars+seq)|
|[really-retire](#idx+cmds+really-retire)|[\_filter\_pmda\_install](#idx+funcs+filterpmdainstall)|[\_remove\_job\_scheduler](#idx+funcs+removejobscheduler)|[$seq\_full](#idx+vars+seqfull)|
|[recheck](#idx+cmds+recheck)|[\_filter\_pmda\_remove](#idx+funcs+filterpmdaremove)|[\_restore\_auto\_restart](#idx+funcs+restoreautorestart)|[$status](#idx+vars+status)|
|[remake](#idx+cmds+remake)|[\_filter\_pmdumplog](#idx+funcs+filterpmdumplog)|[\_restore\_config](#idx+funcs+restoreconfig)|[$sudo](#idx+vars+sudo)|
|[sameas](#idx+cmds+sameas)|[\_filter\_pmdumptext](#idx+funcs+filterpmdumptext)|[\_restore\_job\_scheduler](#idx+funcs+restorejobscheduler)|[$tmp](#idx+vars+tmp)|
|[show-me](#idx+cmds+show-me)|[\_filter\_pmie\_log](#idx+funcs+filterpmielog)|[\_restore\_loggers](#idx+funcs+restoreloggers)|[$\_valgrind\_filter\_assert](#idx+vars+valgrindcleanassert)|
|[var-use](#idx+cmds+var-use)|[\_filter\_pmie\_start](#idx+funcs+filterpmiestart)|[\_restore\_pmda\_install](#idx+funcs+restorepmdainstall)|**Files**|
|**Shell Functions**|[\_filter\_pmie\_stop](#idx+funcs+filterpmiestop)|[\_restore\_pmda\_mmv](#idx+funcs+restorepmdammv)|[$seq\_full](#idx+files+seqfull)|
|[\_all\_hostnames](#idx+funcs+allhostnames)|[\_filter\_pmproxy\_log](#idx+funcs+filterpmproxylog)|[\_restore\_pmlogger\_control](#idx+funcs+restorepmloggercontrol)|[check.log](#idx+files+check.log)|
|[\_all\_ipaddrs](#idx+funcs+allipaddrs)|[\_filter\_pmproxy\_start](#idx+funcs+filterpmproxystart)|[\_restore\_primary\_logger](#idx+funcs+restoreprimarylogger)|[check.time](#idx+files+check.time)|
|[\_arch\_start](#idx+funcs+archstart)|[\_filter\_pmproxy\_stop](#idx+funcs+filterpmproxystop)|[\_run\_helgrind](#idx+funcs+runhelgrind)|[common](#idx+files+common)|
|[\_avail\_metric](#idx+funcs+availmetric)|[\_filter\_post](#idx+funcs+filterpost)|[\_run\_valgrind](#idx+funcs+runvalgrind)|[common.\*](#idx+files+common.star)|
|[\_change\_config](#idx+funcs+changeconfig)|[\_filter\_slow\_pmie](#idx+funcs+filterslowpmie)|[\_save\_config](#idx+funcs+saveconfig)|[common.config](#idx+files+common.config)|
|[\_check\_64bit\_platform](#idx+funcs+check64bitplatform)|[\_filter\_top\_pmns](#idx+funcs+filtertoppmns)|[\_service](#idx+funcs+service)|[group](#idx+files+group)|
|[\_check\_agent](#idx+funcs+checkagent)|[\_filter\_torture\_api](#idx+funcs+filtertortureapi)|[\_set\_dsosuffix](#idx+funcs+setdsosuffix)|[localconfig](#idx+files+localconfig)|
|[\_check\_core](#idx+funcs+checkcore)|[\_filter\_valgrind](#idx+funcs+filtervalgrind)|[\_show\_pmie\_errors](#idx+funcs+showpmieerrors)|[qa\_hosts](#idx+files+qahosts)|
|[\_check\_display](#idx+funcs+checkdisplay)|[\_filter\_valgrind\_possibly](#idx+funcs+filtervalgrindpossibly)|[\_show\_pmie\_exit](#idx+funcs+showpmieexit)|[qa\_hosts.primary](#idx+files+qahosts.primary)|
|[\_check\_freespace](#idx+funcs+checkfreespace)|[\_filter\_views](#idx+funcs+filterviews)|[\_sighup\_pmcd](#idx+funcs+sighuppmcd)|[triaged](#idx+files+triaged)|
|[\_check\_helgrind](#idx+funcs+checkhelgrind)|[\_find\_free\_port](#idx+funcs+findfreeport)|[\_sort\_pmdumplog\_d](#idx+funcs+sortpmdumplogd)||
|[\_check\_job\_scheduler](#idx+funcs+checkjobscheduler)|[\_find\_key\_server\_modules](#idx+funcs+findkeyservermodules)|[\_start\_up\_pmlogger](#idx+funcs+startuppmlogger)||
