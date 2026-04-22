# ![Alt text](../images/pcplogo-80.png) Performance Co-Pilot QA Cookbook
Version 1.0, April 2026

<a id="table-of-contents"></a>
# Table of contents
<br>[1 Preamble](#preamble)
<br>[2 The basic model](#the-basic-model)
<br>[3 Creating a new test](#creating-a-new-test)
<br>&nbsp;&nbsp;&nbsp;[3.1 The **new** script](#the-new-script)
<br>[4 Control files](#control-files)
<br>&nbsp;&nbsp;&nbsp;[4.1 The _group_ file](#the-group-file)
<br>&nbsp;&nbsp;&nbsp;[4.2 The _localconfig_ file](#the-localconfig-file)
<br>[5 **check** script](#check-script)
<br>&nbsp;&nbsp;&nbsp;[5.1 Command line options for **check**](#command-line-options-for-check)
<br>&nbsp;&nbsp;&nbsp;[5.2 The _triaged_ file](#the-triaged-file)
<br>&nbsp;&nbsp;&nbsp;[5.3 **check** setup](#check-setup)
<br>&nbsp;&nbsp;&nbsp;[5.4 **check.callback** script](#check.callback-script)
<br>&nbsp;&nbsp;&nbsp;[5.5 _check.log_ file](#check.log-file)
<br>&nbsp;&nbsp;&nbsp;[5.6 _check.time_ file](#check.time-file)
<br>&nbsp;&nbsp;&nbsp;[5.7 _qa\_hosts.primary_ and _qa\_hosts_ files](#qahosts.primary-and-qahosts-files)
<br>[6 **show-me** script](#show-me-script)
<br>[7 Common shell variables](#common-shell-variables)
<br>[8 Coding style suggestions for tests](#coding-style-suggestions-for-tests)
<br>&nbsp;&nbsp;&nbsp;[8.1 Portability considerations](#portability-considerations)
<br>&nbsp;&nbsp;&nbsp;[8.2 Take control of stdout and stderr](#take-control-of-stdout-and-stderr)
<br>&nbsp;&nbsp;&nbsp;[8.3 **$seq\_full** file suggestions](#seqfull-file-suggestions)
<br>&nbsp;&nbsp;&nbsp;[8.4 Dealing with the known unknowns](#dealing-with-the-known-unknowns)
<br>[9 Shell functions from _common.check_](#shell-functions-from-common.check)
<br>&nbsp;&nbsp;&nbsp;[9.1 PMDA Install and Remove](#pmda-install-and-remove)
<br>&nbsp;&nbsp;&nbsp;[Plan A](#plan-a)
<br>&nbsp;&nbsp;&nbsp;[Plan B](#plan-b)
<br>&nbsp;&nbsp;&nbsp;[9.2 **\_private\_pmcd**](#privatepmcd)
<br>&nbsp;&nbsp;&nbsp;[9.3 **\_triage\_wait\_point**](#triagewaitpoint)
<br>[10 Shell functions from _common.filter_](#shell-functions-from-common.filter)
<br>[11 Helper scripts](#helper-scripts)
<br>&nbsp;&nbsp;&nbsp;[11.1 **mk.logfarm** script](#mk.logfarm-script)
<br>&nbsp;&nbsp;&nbsp;[11.2 **mk.qa\_hosts** script](#mk.qahosts-script)
<br>[12 qa subdirectories](#qa-subdirectories)
<br>&nbsp;&nbsp;&nbsp;[12.1 _src_](#src)
<br>&nbsp;&nbsp;&nbsp;[12.2 _archives_](#archives)
<br>&nbsp;&nbsp;&nbsp;[12.3 _badarchives_](#badarchives)
<br>&nbsp;&nbsp;&nbsp;[12.4 _tmparch_](#tmparch)
<br>&nbsp;&nbsp;&nbsp;[12.5 _pmdas_](#pmdas)
<br>&nbsp;&nbsp;&nbsp;[12.6 _admin_](#admin)
<br>[13 Other directories](#other-directories)
<br>[14 Using valgrind](#using-valgrind)
<br>[15 Using helgrind](#using-helgrind)
<br>[16 _common_ and _common.\*_ files](#common-and-common.-files)
<br>[17 SELinux considerations](#selinux-considerations)
<br>[18 Package lists](#package-lists)
<br>[19 The last word](#the-last-word)
<br>[Appendix: Initial setup](#appendix-initial-setup)
<br>&nbsp;&nbsp;&nbsp;[**sudo** setup](#sudo-setup)
<br>&nbsp;&nbsp;&nbsp;[Distributed QA](#distributed-qa)
<br>&nbsp;&nbsp;&nbsp;[Firewall setup](#firewall-setup)
<br>&nbsp;&nbsp;&nbsp;[_common.config_ file](#common.config-file)
<br>&nbsp;&nbsp;&nbsp;[Some special test cases](#some-special-test-cases)
<br>&nbsp;&nbsp;&nbsp;[Take it for a test drive](#take-it-for-a-test-drive)
<br>[Appendix: PCP acronyms](#appendix-pcp-acronyms)
<br>[Index](#index)

<a id="preamble"></a>
# 1 Preamble

These notes are designed to help with building, running and maintaining QA
(Quality Assurance) tests
for the Performance Co-Pilot ([PCP](#idx+pcp)) project
([www.pcp.io](https://www.pcp.io/) and
[https://github.com/performancecopilot/pcp](https://github.com/performancecopilot/pcp/)).

The PCP QA infrastructure is designed with a philosophy that aims to
exercise the PCP components in a context that is as close as possible
to that which an end-user would experience. For this reason, the PCP
software to be tested should be installed in the "usual" places, with
the "usual" permissions and communicate on the "usual" TCP/IP ports.

The PCP QA infrastructure does **not** execute PCP applications like
**pmcd**(1), **pmlogger**(1), **pminfo**(1), **pmie**(1), **pmrep**(1),
etc built in the git tree, rather they need to have been already built,
packaged and installed on the local system prior to starting any QA.

Refer to the **Makepkgs** script in the top directory of the source
tree for a recipe that may be used to build packages for a variety of
platforms.

We assume you're a developer or tester, running, building or fixing PCP QA
tests, so you are operating in a git tree (not the
_/var/lib/pcp/testsuite_ directory that is packaged and installed),
you're using a non-root login and
let's assume that from the base of the git tree you've already done:

```bash
$ cd qa
```

Since the "qa" directory is where all the QA action happens, scripts
and files in this cookbook that are not absolute paths are all
relative to the "qa" directory, so for example *src/app1* is the path
*qa/src/app1* relative to the base of the git tree.

If you're setting up the PCP QA infrastructure for a new machine or VM or container,
then refer to the [Initial setup](#appendix-initial-setup) appendix
and the [Package lists](#package-lists) section in this document.

The PCP QA infrastructure exercises and tests aspects of the PCP
packaging, use of certain local accounts, interaction with system
daemons and services, and a number of PCP-related system administrative
functions, e.g. to stop and start PCP services.
Some of these require "root" privileges, refer to the
[**sudo** setup](#sudo-setup) section below.

But this also means the QA tests may alter existing system
configuration files, and this introduces some risk, so PCP QA should
not be run on production systems. Historically we have used developer
systems and dedicated QA systems for running QA - VMs are particularly
well-suited to this task.

In addition to the base PCP package installation, the **sample** and
**simple** [PMDAs](#idx+pmda) need to be installed (however the QA infrastructure
will take care of this).

The phrase "test script" or simply "test" refers to one of the
thousands of test scripts numbered 000 to 999 and then 1000 \[1]... For
shell usage the "glob" pattern **[0-9\]\*[0-9\]** does a reasonable job
of matching all test scripts.

\[1] The unfortunate mix of 3-digit and 4-digit test numbers is a
historical accident; when we started, no one could have imagined that
we'd ever have more than a thousand test scripts!

Where these notes need to refer to a specific test, we'll use **$seq**
to mean "the test you're currently interested in", so we're
assuming you've already done something like:

```bash
$ seq=1234
```

For the components of the PCP QA infrastructure, commands (and their
options), scripts, environment variables, shell variables and shell
procedures all appear in **bold** case. File names appear in *italic*
case. Snippets of code, shell scripts, configuration file contents and
examples appear in `fixed width` font.

Annotations of the form **command**(1) are oblique pointers to the "man"
pages that are **not** part of the PCP QA infrastructure, although they
may well be PCP "man" pages, e.g. **pmcd**(1).

This document is very long.  However when displayed in a "markdown"
viewer like **okular**(1) or **retext**(1) or a web browser using
the github formatter
[https://github.com/performancecopilot/pcp/blob/main/qa/COOKBOOK.md](https://github.com/performancecopilot/pcp/blob/main/qa/COOKBOOK.md),
then the [Table of contents](#table-of-contents) at the beginning and the [Index](#index)
of commands, scripts, shell functions and shell variables at
the end provide links that enable quick access to the section(s)
you may be interested in.

<a id="the-basic-model"></a>
# 2 The basic model

Minimally each test consists of a shell script **$seq** and an expected
output file **$seq**_.out_.

When run under the control of [**check**](#check-script), **$seq** is
executed and the output is captured and compared to **$seq**_.out_. If
the two outputs are identical the test is deemed to have passed, else
the (unexpected) output is saved to **$seq**_.out.bad_.

Central to this model is the fact that **$seq** must produce
deterministic output, independent of hostname, filesystem pathname
differences, date, timezone, locale, platform differences, variations in output
from non-PCP applications, etc. This is achieved by "filtering" command
output and log files to remove lines or fields that are not
deterministic, and replace variable text with constants.

The test scripts are expected to exit with status **0**, but may exit with
a non-zero status in cases of catastrophic failure, e.g. some service to
be exercised did not start correctly, so there is nothing to test.

As tests are developed and evolve, the [**remake**](#idx+cmds+remake)
script is used to
generate updated versions of **$seq**_.out_.

To assist with test failure triage, all tests also generate a
**$seq**_.full_ file that contains additional diagnostics and
unfiltered output.

<a id="creating-a-new-test"></a>
# 3 Creating a new test

"Good" QA tests are ones that typically:

- Are focused on one area of functionality or previous regression (complex tests are more likely to pass in multiple subtests but failing a single subtest makes the whole test fail, and complex tests are harder to debug).
- Run quickly -- the entire QA suite already takes a long time to run.
- Are resilient to platform and distribution changes.
- Don't check something that's already covered in another test.
- When exercising complex parts of core PCP functionality we'd like to see both a non-valgrind and a valgrind variant of the test (see [**new**](#the-new-script) and [**new-grind**](#idx+cmds+new-grind) below).
- Be sensitive to system state, so if a test needs a particular service or configuration setting it should take control no matter what the system state is at the start of the test and return the system to the original state before exiting (even on error paths).  This is particularly important for PMDA installation and changes to the configuration files for key PCP services like **pmcd**(1), **pmproxy**(1) and **pmlogger**(1). 

And "learning by example" is the well-trusted model that pre-dates AI ... there are thousands of
existing tests, so lots of worked examples for you to choose from.

Always use **new** to create a skeletal test. In addition to creating
the skeletal test, this will **git** **add** the new test and the
_.out_ file, and update the *group* file, so when you've finished the
script development you need to (at least):

```bash
$ ./remake $seq
$ git add $seq $seq.out
$ git commit
```

Additional **git** commands and possibly *GNUmakefile* changes will be
needed if your test needs any additional new files, e.g. a new source
program or script below in the [*qa/src*](#src) directory or a new
archive in the [*qa/archives*](#archives) directory.

<a id="the-new-script"></a>
## 3.1 The <a id="idx+cmds+new"></a>**new** script

Usage: **new** \[*options*] \[*seqno* \[*group* ...]]

The **new** script creates a new skeletal test.

The new test has a test number assigned from "gaps" in the
[*group*](#the-group-file) file; the starting test number is randomly
generated (based on a range hard-coded in **new**, your login name
magically converted to a number, the seconds component of the current
time and some hashing ...  all designed to assign different numbers
to different users at about the same time and so a weak collision
avoidance scheme).  The default pseudo-random allocation is the same as
specifying a *seqno* of **-**.

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

<a id="control-files"></a>
# 4 Control files

There are several files that augment the test scripts and control
how QA tests are executed.

<a id="the-group-file"></a>
## 4.1 The <a id="idx+files+group"></a>_group_ file

Each test belongs to one or more "groups" and the
_group_ file is used to record
the set of known groups and the 1:many mapping between each test
and an associated set of groups.

Groups are defined for applications, [PMDAs](#idx+pmda), services, general
features or functional areas (e.g. **archive**, **pmns**, **getopt**, ...)
and testing type (e.g. **remote**, **local**, **not\_in\_ci**, ...).

The format of the _group_ file is:

- comment lines begin with **#**
- blank lines are ignored
- lines beginning with a non-numeric name a group
- lines beginning with a number associate groups with a test
- an optional tag may be appended to a test number (without whitespace separation) to indicate the entry requires special treatment; the tag may be **:reserved** (the test number is allocated but the test development is not yet completed) or **:retired** (no longer active and not expected to be run)

Comments within the file provide further information as to format.

<a id="the-localconfig-file"></a>
## 4.2 The <a id="idx+files+localconfig"></a>_localconfig_ file

The _localconfig_ file is generated by the [**mk.localconfig**](#idx+cmds+mk.localconfig)
script.  It defines the shell variables
<a id="idx+vars+pcpplatform"></a>**$PCP\_PLATFORM** and
<a id="idx+vars+pcpver"></a>**$PCP\_VER**
The common values for **$PCP\_PLATFORM** are **linux**, **darwin**, **freebsd**, **netbsd**, **openbsd**, **solaris**. 
**$PCP\_VER** is an all numeric PCP version number, e.g. 70100 for PCP 7.1.0, suitable for making relative numeric comparisons), so can be used as follows:

```bash
$ [ "$PCP_VER" -lt 61007 ] && _notrun "PCP version $PCP_VER too old"
```

_localconfig_ is sourced
from **common.check** so every test script has access to these shell
variables.

<a id="check-script"></a>
# 5 <a id="idx+cmds+check"></a>**check** script

The **check** script is responsible for running one or more tests and
determining their outcome as follows:
<br>

|**Outcome**|**Description**|
|---|---|
|**pass**|test ran to completion, exit status is **0** and output is identical to the corresponding *.out* file|
|**notrun**|test called [**\_notrun**](#idx+funcs+notrun)|
|**callback**|same as **pass** but [**check.callback**](#check.callback-script) was run and detected a problem|
|**fail**|test exit status was not **0**|
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
## 5.1 Command line options for **check**

The parsing of command line arguments is a little Neanderthal, so best
practice is to separate options with whitespace.
<br>

|**Option**|**Description**|
|---|---|
|**-c**|Before and after each test check for selected configuration files to ensure they have not been modified.|
|**-C**|Enable color mode to highlight outcomes (assumes interactive execution).|
|**-CI**|When QA tests run in the github infrastructure for the CI or QA actions, there are some tests that will never pass. The **-CI** option is shorthand for "**-x x11 -x remote -x not\_in\_ci**" and also sets <a id="idx+vars+pcpqainci"></a>**$PCPQA\_IN\_CI** to **yes** so individual tests can make local decisions if they are running in this environment.|
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

<a id="the-triaged-file"></a>
## 5.2 The <a id="idx+files+triaged"></a>_triaged_ file

Some tests may fail in ways that after careful analysis are deemed to be
a "test" failure, rather than a PCP failure or regression. Causes might be
timing issues that are impossible to control or failures on slow VMs or
caused by non-PCP code that's failing.

The _triaged_ file provides a mechanism to describe failures for
specific tests on particular hosts or operating system versions or
CPU architectures, or ... that have been analyzed and should not be
considered a hard PCP QA failure.  Comments at the head of the file
describe the required format for entries.

**check** consults _triaged_ after a test failure, and if a match is
found the test outcome is considered to be **triaged** not **fail** and
the text "**\[triaged]**" is appended to the _.out.bad_ file.

<a id="check-setup"></a>
## 5.3 **check** setup

Unless the **-q** option is given, **check** will perform
the following tasks before any test is run:<br>

- run [**mk.localconfig**](#idx+cmds+mk.localconfig)
- ensure **pmcd**(1) is running with the **-T 3** option to enable PDU tracing and the **-C 512** option to enable 512 [PMAPI](#idx+pmapi) contexts
- ensure the **sample**, **sampledso** and **simple** [PMDAs](#idx+pmda) are installed and working
- ensure **pmcd**(1), **pmproxy**(1) and **pmlogger**(1) are all configured to allow remote connections
- ensure the primary **pmlogger**(1) instance is running
- if [Distributed QA](#distributed-qa) has been enabled, check that the PCP setup on the remote systems is OK
- run `make setup` which will run `make setup` in multiple subdirectories, but most importantly [_src_](#src) (so the QA apps are made), [_tmparch_](#tmparch) (so the transient archives are present) and [_pmdas_](#pmdas) (so the QA [PMDAs](#idx+pmda) are up to date) 

<a id="check.callback-script"></a>
## 5.4 **check.callback** script

If **check.callback** exists and is executable, then it will be run from
**check** both before and after each test case is completed. The
optional first argument is **--precheck** for the before call, and the
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
- Is **pmcd** healthy? This is delegated to **./941** with **--check**.
- Is **pmlogger** healthy? This is delegated to **./870** with **--check**.
- Are all of the configured [PMDAs](#idx+pmda) still alive?
- Has the [PMNS](#idx+pmns) been trashed? This is delegated to **./1190** with **--check**.
- Are there any PCP configuration files that contain text to indicate they have been modified by a QA test, as opposed to the version installed from packages.

<a id="check.log-file"></a>
## 5.5 <a id="idx+files+check.log"></a>_check.log_ file

An historical record of each execution of the [**check**](#check-script) script,
reporting what tests were run, notrun, passed, triaged, failed, etc.

<a id="check.time-file"></a>
## 5.6 <a id="idx+files+check.time"></a>_check.time_ file

Elapsed time for last successful execution of each test run by
**check**.

<a id="qahosts.primary-and-qahosts-files"></a>
## 5.7 <a id="idx+files+qahosts.primary"></a>_qa\_hosts.primary_ and <a id="idx+files+qahosts"></a>_qa\_hosts_ files

Refer to the [**mk.qa\_hosts**](#mk.qahosts-script) section.

<a id="show-me-script"></a>
# 6 <a id="idx+cmds+show-me"></a>**show-me** script

Usage: **show-me** \[**-g** *group*] \[**-l**] \[**-n**] \[**-x** *group*] \[seqno ...]

The **show-me** script is responsible for displaying the differences
between the actual output (**$seq**_.out.bad_) and the expected output
(**$seq**_.out_) for selected tests.

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
# 7 Common shell variables

The common preamble in every test script sources some *common\** scripts
and the following shell variables that may be used in your test script.<br>

|**Variable**|**Description**|
|---|---|
|<a id="idx+vars+pcpstar"></a>**$PCP\_\***|Everything from **$PCP\_DIR**_/etc/pcp.conf_ is placed in the environment by calling **$PCP\_DIR**_/etc/pcp.env_ from *common.rc*, so for example **$PCP\_LOG\_DIR** is always defined and **$PCP\_AWK\_PROG** should be used instead of **awk**.|
|**$PCP\_PLATFORM**|See the [_localconfig_ file](#the-localconfig-file) section.|
|**$PCP\_VER**|See the [_localconfig_ file](#the-localconfig-file) section.|
|<a id="idx+vars+here"></a>**$here**|Current directory tests are run from. Most useful after a test script **cd**'s someplace else and you need to **cd** back, or reference a file back in the starting directory.|
|<a id="idx+vars+seq"></a>**$seq**|The sequence number of the current test.|
|<a id="idx+vars+seqfull"></a>**$seq\_full**|Full pathname to the test's *.full* file. Always use this in preference to **$seq**.*full* because **$seq\_full** works no matter where the test script might **cd** to.|
|<a id="idx+vars+status"></a>**$status**| Exit status for the test script.|
|<a id="idx+vars+sudo"></a>**$sudo**|Proper invocation of **sudo**(1) that includes any per-platform additional command line options.|
|<a id="idx+vars+tmp"></a>**$tmp**|Unique prefix for temporary files or directory. Use **$tmp**_.foo_ or `$ mkdir $tmp` and then use **$tmp**_/foo_ or both. The standard **trap** cleanup function in each test will remove all these files and directories automatically when the test finishes, so save anything useful to **$seq\_full**.|

There are some further shell variables that may be required or used by specific
parts of the QA infrastructure as described below.<br>

|**Variable**|**Description** or **Reference**|
|---|---|
|**$DSOSUFFIX**|Shared library suffix. Set by [**\_set\_dsosuffix**](#idx+funcs+setdsosuffix) called.|
|**$grind\_extra**|Extra arguments for **valgrind**(1).  Must be set before calling [**\_run\_valgrind**](#idx+funcs+runvalgrind) or using [**$valgrind\_clean\_assert**](#idx+vars+valgrindcleanassert).|
|**$PCPQA\_CLOSE\_X\_SERVER**|See the [_common.config_ files](#common.config-file) section.|
|**$PCPQA\_DESKTOP\_HACK**|See the [_common.config_ files](#common.config-file) section.|
|**$PCPQA\_FAR\_PMCD**|See the [_common.config_ files](#common.config-file) section.|
|**$PCPQA\_HYPHEN\_HOST**|See the [_common.config_ files](#common.config-file) section.|
|**$PCPQA\_IN\_CI**|See the [Command line options for **check**](#command-line-options-for-check) section.|
|**$PCPQA\_PREFER\_VALGRIND**|Used in [**\_prefer\_valgrind**](#idx+funcs+prefervalgrind).|
|**$PCPQA\_SYSTEMD**|Set in [_common.check_](#idx+vars+pcpqasystemd).|
|**$PCPQA\_VALGRIND**|Used in all tests generated by [**new-grind**](#idx+cmds+new-grind). See the [**Using valgrind**](#using-valgrind) section for details.|
|**$pid**|Set on return from [**\_start\_up\_pmlogger**](#idx+funcs+startuppmlogger).|
|**$pmcd\_args**|Set before calling [**\_private\_pmcd**](#privatepmcd).|
|**$pmcd\_pid**|Set on return from [**\_private\_pmcd**](#privatepmcd).|
|**$pmcd\_port**|Set on return from [**\_private\_pmcd**](#privatepmcd).|
|**$PMCD\_PORT**|Set on return from [**\_private\_pmcd**](#privatepmcd).|
|**$valgrind\_clean\_assert**|See the [Using valgrind](#using-valgrind) section.|

<a id="coding-style-suggestions-for-tests"></a>
# 8 Coding style suggestions for tests

<a id="portability-considerations"></a>
## 8.1 Portability considerations

PCP QA runs on lots of systems, including older (but not yet end-of-life) versions
of the various Linux distributions and non-Linux systems (particularly macOS and the
BSD-based distributions), so portability is important.

Particular attention should be paid to the following repeat offenders:

- Posix **sh**(1) is the standard, not this week's version of **bash**(1). The list of things to avoid is long, but here are some examples: `[[`, arithmetic expansion `$((expression))`, `local` variables, variable indirection `${!var}`.<br>Some might advise the assistance of **shellcheck**(1) here, but in my experience this is (a) broken and (b) bug reports go unanswered. 
- Don't use explicit pathnames, QA test scripts already have access to all the [**$PCP\_\*** variables](#idx+vars+pcpstar) from **$PCP\_DIR**_/etc/pcp.conf_ 
- Use [**$PCP\_PLATFORM**](#idx+vars+pcpplatform) for platform-specific conditional things, including when to call [**_notrun**](#idx+funcs+notrun).
- Don't assume your test will be run in the same timezone that you created it and any PCP archives in.  Use **-z** with PCP tools fetching information from archives, and either filter local times away or set **TZ=UTC** in the environment.

<a id="take-control-of-stdout-and-stderr"></a>
## 8.2 Take control of stdout and stderr

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
## 8.3 <a id="idx+files+seqfull"></a>**$seq\_full** file suggestions

Assume your test is going to fail at some point, so be defensive up
front. In particular ensure that your test appends the following
sort of information to **$seq\_full**:

- values of critical shell variables that are not hard-coded, e.g. remote host, port being used for testing
- unfiltered output (before it goes to the *.out* file) especially if the filtering is complex or aggressive to meet determinism requirements
- on error paths, output from commands that might help explain the error cause
- context that helps match up test cases in the script with the output in both the *.out* and *.full* files, e.g. this is a common pattern:<br>
`$ echo "--- subtest foobar ---" | tee -a $seq_full`
- log files that are unsuitable for inclusion in *.out*

The common preamble for all tests will ensure **$seq\_full** is removed at the start of each test, so you can safely use constructs like:

```bash
$ echo ... >>$seq_full
$ cmd ... | tee -a $seq_full | ...
```

Remember that **$seq\_full** translates to file **$seq**_.full_ (dot, not underscore) in the directory the **$seq** test is run from.

<a id="dealing-with-the-known-unknowns"></a>
## 8.4 Dealing with the known unknowns

If tests are dealing with time intervals in terms of "today" or
"yesterday" or "4 hours ago", then running the test in the region of
midnight can be problematic. Similarly New Year's Eve is a time where
"this year" can change quite quickly.

More subtle are the points where daylight saving might start or stop,
leaving the system clock running but wallclock time suddenly misses an
hour or runs the same hour twice.

When this makes a test non-deterministic, the defensive
mechanisms are to either use an appropriate guard with
[**\_notrun**](#idx+funcs+notrun) or add the test to the
[*triaged*](#the-triaged-file) file.

<a id="shell-functions-from-common.check"></a>
# 9 Shell functions from _common.check_

A large number of shell functions that are useful across
multiple test scripts are provided by _common.check_ to assist with
test development and these are always available for tests created with
a standard preamble provided by the [**new**](#the-new-script) script.

In addition to defining the shell procedures
described in the table below, _common.check_ also
handles:

- if necessary running [**mk.localconfig**](#idx+cmds+mk.localconfig)
- sourcing [_localconfig_](#idx+files+localconfig)
- setting <a id="idx+vars+pcpqasystemd"></a>**$PCPQA\_SYSTEMD** to **yes** or **no** depending if services are controlled by **systemctl**(1) or not

In the descriptions below "output" means output to stdout.
<br>

|**Function**|**Description**|
|---|---|
|<a id="idx+funcs+allhostnames"></a>**\_all\_hostnames**|Usage: **\_all\_hostnames** _hostname_<br>Generate a comma separated list of all hostnames or IP addresses associated with active network interfaces for the host _hostname_.<br>Requires **ssh**(1) access for the user **pcpqa** to _hostname_.|
|<a id="idx+funcs+allipaddrs"></a>**\_all\_ipaddrs**|Usage: **\_all\_ipaddrs** _hostname_<br>Generate a comma separated list of all IP addresses associated with active network interfaces for the host _hostname_.<br>Requires **ssh**(1) access for the user **pcpqa** to _hostname_.|
|<a id="idx+funcs+archstart"></a>**\_arch\_start**|Usage: **\_arch\_start** _archive_ \[_offset_]<br>Output the time of the first real metric record in _archive_ in the format **@HH:MM:SS.SSS** (so suitable for use with a **-O** or **-S** command line option to PCP reporting tools). The time is in the timezone of the archive, so it is expected that it will be used with a **-z** command line option.<br>If present _offset_ is interpreted as a number of seconds to be added to the time before it is printed.<br>Use this function to safely skip over the _preamble_ **pmResult** at the start of a PCP archive.|
|<a id="idx+funcs+availmetric"></a>**\_avail\_metric**|Usage: **\_avail\_metric** _metric_<br>Check if _metric_ is available from the local **pmcd**(1). If available return **0** else return **1**. Use this check for a _metric_ that is optionally required by a QA test but _metric_ is not universally available, e.g. kernel metrics that are not present on all platforms.<br>See also the [**\_check\_metric**](#idx+funcs+checkmetric) and [**\_need\_metric**](#idx+funcs+needmetric) functions below.|
|<a id="idx+funcs+changeconfig"></a>**\_change\_config**|Usage: **\_change\_config** _service_ _onoff_<br>The PCP services like **pmcd**, **pmproxy**, **pmlogger** and **pmie** are typically enabled or disabled by some mechanism outside the PCP ecosystem, e.g. **init**(1) or **systemd**(1) and this influences whether each of the services is stopped or started during system shutdown and system boot. The **\_change\_config** function provides a platform-independent way of changing the setting for _service_. _onoff_ must be **on** or **off**.<br>As a special case, _service_ may be **verbose** to enable or disable verbosity for the underlying mechanism if that notion is supported.<br>See also [**\_get\_config**](#idx+funcs+getconfig).|
|<a id="idx+funcs+check64bitplatform"></a>**\_check\_64bit\_platform**|Usage: **\_check\_64bit\_platform**<br>If the test is **not** being run on a 64-bit platform, call [**\_notrun**](#idx+funcs+notrun) with an appropriate message.|
|<a id="idx+funcs+checkagent"></a>**\_check\_agent**|Usage: **\_check\_agent** _pmda_ \[_verbose_]<br>Checks that the _pmda_ [PMDA](#idx+pmda) is installed and responding to metric requests. Returns **0** if all is well, else returns **1** and outputs diagnostics to explain why. If _verbose_ is **true** emit diagnostics independent of return value.|
|<a id="idx+funcs+checkbigfile"></a>**\_check\_big\_file**|Usage: **\_check\_big\_file** *file*<br>Big files may not be packaged so check if *file* exists and if it does return.<br>Otherwise call [**\_notrun**](#idx+funcs+notrun) if QA is being run from the package install directory (**$PCP_VAR_DIR**_/testsuite_) else this is a problem and call [**\_fail**](#idx+funcs+fail).|
|<a id="idx+funcs+checkcore"></a>**\_check\_core**|Usage: **\_check\_core** \[_dir_]<br>List any "core" files in _dir_ (defaults to _._), so no output if there are none.|
|<a id="idx+funcs+checkdisplay"></a>**\_check\_display**|Usage: **\_check\_display**<br>For applications that need an X11 display (like **pmchart**(1)), call [**\_notrun**](#idx+funcs+notrun) with an appropriate message if [**$PCPQA\_CLOSE\_X\_SERVER**](#idx+vars+pcpqaclosexserver) is not set or **xdpyinfo**(1) cannot be run successfully.<br>If **\_check\_display** believes the X11 display is accessible and [**$PCPQA\_DESKTOP\_HACK**](#idx+vars+pcpqadesktophack) is set to **true** then the directory **$tmp**_/xdg-runtime_ is also created and **$XDG\_RUNTIME\_DIR** is set to the path to this directory.  This may be needed to suppress warning babble from some Qt applications. See also [**\_clean\_display**](#idx+funcs+cleandisplay).|
|<a id="idx+funcs+checkfreespace"></a>**\_check\_freespace**|Usage: **\_check\_freespace** _need_<br>Returns **0** if there is more than _need_ Mbytes of free space in the filesystem for the current working directory, else returns **1**.|
|<a id="idx+funcs+checkjobscheduler"></a>**\_check\_job\_scheduler**|Usage: **\_check\_job\_scheduler**<br>Many of the PCP services require periodic actions to check health, rotate logs, rotate **pmlogger**(1) archives, daily report generation, etc. and these depend on **systemd**(1) timers or **cron**(1). This function tests if one of the required underlying mechanisms is available, else call [**\_notrun**](#idx+funcs+notrun) with the (somewhat cryptic) message "No crontab binary found". Tests that rely on these periodic actions are (a) rare, but (b) likely to fail when QA is running in a container, hence the need for this function.<br>When the timers need to be disabled for a QA test, the usual sequence is to call **\_check\_job\_scheduler** to ensure the underlying mechanism is available, then call [**\_remove\_job\_scheduler**](#idx+funcs+removejobscheduler) to disable the timer(s) and then call [**\_restore\_job\_scheduler**](#idx+funcs+restorejobscheduler) at the end of the test to re-enable the timer(s).|
|<a id="idx+funcs+checkkeyserver"></a>**\_check\_key\_server**|Usage: **\_check\_key\_server** \[_port_]<br>Check if a key server is installed and running locally and the version is not too old. Optional _port_ parameter defaults to 6379. Uses [**\_check\_key\_server\_version**](#idx+funcs+checkkeyserverversion).|
|<a id="idx+funcs+checkkeyserverping"></a>**\_check\_key\_server\_ping**|Usage: **\_check\_key\_server\_ping** _port_<br>If there is no key server CLI application installed, call **_notrun**. Otherwise send "ping" requests to the local key server on port _port_ until we see the expected response, in which case the function silently returns. Will timeout after trying for 1.25 secs and the last output from key server CLI application will be output.|
|<a id="idx+funcs+checkkeyserverversion"></a>**\_check\_key\_server\_version**|Usage: **\_check\_key\_server\_version** _port_<br> Check key server version on port _port_. Assumes [**\_check\_series**](#idx+funcs+checkseries) already called and a key server is running locally. If a problem is encountered, [**\_notrun**](#idx+funcs+notrun) is called with an appropriate message.|
|<a id="idx+funcs+checkkeyserverversionoffline"></a>**\_check\_key\_server\_version\_offline**|Usage: **\_check\_key\_server\_version\_offline**<br>Check key server version without contacting key server, uses **--version** argument to key server executable. If a problem is encountered, [**\_notrun**](#idx+funcs+notrun) is called with an appropriate message.|
|<a id="idx+funcs+checklocalprimaryarchive"></a>**\_check\_local\_primary\_archive**|Usage: **\_check\_local\_primary\_archive**<br>Check if the primary **pmlogger**(1) is writing to a local file (as opposed to "logpush" via http to a remote **pmproxy**(1).  Returns **0** if true, else returns **1**.|
|<a id="idx+funcs+checkmetric"></a>**\_check\_metric**|Usage: **\_check\_metric** _metric_ \[_hostname_]<br>Check if _metric_ is available from the host _hostname_ (defaults to **local:**). If available return **0** else emit an error message and return **1**. Use this check for a _metric_ that should have been made available by a recent PMDA installation. See also the [**\_avail\_metric**](#idx+funcs+availmetric) function above and the [**\_need\_metric**](#idx+funcs+needmetric) function below.|
|<a id="idx+funcs+checksearch"></a>**\_check\_search**|Usage: **\_check\_search**<br>Check if the key server search module is installed. If OK, silently returns, else [**\_notrun**](#idx+funcs+notrun) is called with an appropriate message.<br>Uses [**\_find\_key\_server\_search**](#idx+funcs+findkeyserversearch) and [**\_find\_key\_server\_modules**](#idx+funcs+findkeyservermodules).|
|<a id="idx+funcs+checkseries"></a>**\_check\_series**|Usage: **\_check\_series**<br>Check we have **pmseries**(1) and key server **-cli** and **-server** executables installed. Call [**\_notrun**](#idx+funcs+notrun) with an appropriate message otherwise.<br>See also [**\_check\_key\_server**](#idx+funcs+checkkeyserver).|
|<a id="idx+funcs+cleandisplay"></a>**\_clean\_display**|Usage: **\_clean\_display**<br>Remove the directory **$tmp**_/xdg-runtime_, probably created by a previous call to [**\_check\_display**](#idx+funcs+checkdisplay).|
|**\_cleanup\_pmda**|Refer to the [PMDA Install and Remove](#pmda-install-and-remove) section below.|
|<a id="idx+funcs+disableloggers"></a>**\_disable\_loggers**|Usage: **\_disable\_loggers**<br>Replaces the control files for all **pmlogger**(1) instances with a simple one that starts a primary logger with _/dev/null_ as the configuration file so it makes **no** requests to **pmcd**(1).  In effect, this "disables" all loggers.<br>Restore the control files by calling [**\_restore\_loggers**](#idx+funcs+restoreloggers) before exiting the test.|
|<a id="idx+funcs+domainname"></a>**\_domain\_name**|Usage: **\_domain\_name**<br>Output the local host's domain name, else **localdomain** if the real domain name cannot be found.|
|<a id="idx+funcs+exit"></a>**\_exit**|Usage: **\_exit** _status_<br>Set [$status](#idx+vars+status) to _status_ and force test exit.|
|<a id="idx+funcs+fail"></a>**\_fail**|Usage: **\_fail** _message_<br>Emit _message_ on stderr and force failure exit of test.|
|<a id="idx+funcs+filesize"></a>**\_filesize**|Usage: **\_filesize** _file_<br>Output the size of _file_ in bytes.|
|<a id="idx+funcs+filterinitdistro"></a>**\_filter\_init\_distro**|Usage: **\_filter\_init\_distro**<br>Distro-specific filtering for **init**(1), "rc" scripts, etc.|
|<a id="idx+funcs+findfreeport"></a>**\_find\_free\_port**|Usage: **\_find\_free\_port** \[_baseport_]<br>Find an unused local TCP port starting at _baseport_ (defaults to 54321). Search proceeds by incrementing the port number by 1 after each failed probe. On success, output the port number, else after 100 attempts give up, emit an error message on stderr and call [**\_exit**](#idx+funcs+exit).|
|<a id="idx+funcs+findkeyservermodules"></a>**\_find\_key\_server\_modules**|Usage: **\_find\_key\_server\_modules**<br>Outputs the path to the "modules" directory for the local key server, if any.|
|<a id="idx+funcs+findkeyservername"></a>**\_find\_key\_server\_name**|Usage: **\_find\_key\_server\_name**<br>Output the name of the installed key server, either **valkey** or **redis** or "".|
|<a id="idx+funcs+findkeyserversearch"></a>**\_find\_key\_server\_search**|Usage: **\_find\_key\_server\_search**<br>Output the name of the installed key server search application, either **valkeysearch** or **redisearch** or "".|
|<a id="idx+funcs+getconfig"></a>**\_get\_config**|Usage: **\_get\_config**<br>PCP services like **pmcd**, **pmproxy**, **pmlogger** and **pmie** are typically enabled or disabled by some mechanism outside the PCP ecosystem, e.g. **init**(1) or **systemd**(1) and this influences whether each of the services is stopped or started during system shutdown and system boot. The **\_get\_config** function provides a platform-independent way of interrogating the state for _service_. If the state can be determined, **\_get\_config** outputs **on** or **off** and returns **0**.  Otherwise the return value is **1** and an explanatory message is output. As a special case, _service_ may be **verbose** to interrogate the verbosity for the underlying mechanism if that notion is supported.<br>See also [**\_change\_config**](#idx+funcs+changeconfig).|
|<a id="idx+funcs+getendian"></a>**\_get\_endian**|Usage: **\_get\_endian**<br>Output **be** (big endian) or **le** (little endian) for the host where the QA test is running.|
|<a id="idx+funcs+getfqdn"></a>**\_get\_fqdn**|Usage: **\_get\_fqdn**<br>Output the FQDN (fully qualified domain name) for the host running the QA test by calling [**\_host\_to\_fqdn**](#idx+funcs+hosttofqdn).|
|<a id="idx+funcs+getlibpcpconfig"></a>**\_get\_libpcp\_config**|Usage: **\_get\_libpcp\_config**<br>Calls **pmconfig**(1) (with the **-L** and **-s** options) and sets _all_ of the PCP library configuration variables as shell variables in the environment.|
|<a id="idx+funcs+getport"></a>**\_get\_port**|Usage: **\_get\_port** _proto_ _lowport_ _highport_<br>For IP protocol _proto_ (**tcp** or **udp**) output the first unused port in the range _lowport_ to _highport_, else no output if an unused port cannot be found.|
|<a id="idx+funcs+getprimaryloggerpid"></a>**\_get\_primary\_logger\_pid**|Usage: **\_get\_primary\_logger\_pid**<br>Output the PID of the running primary **pmlogger**(1), if any.|
|<a id="idx+funcs+getwordsize"></a>**\_get\_word\_size**|Usage: **\_get\_word\_size**<br>If known, output the word size (in bits) for the host running the QA test and return **0**. Otherwise output **0** and return **1**.|
|<a id="idx+funcs+hosttofqdn"></a>**\_host\_to\_fqdn**|Usage: **\_host\_to\_fqdn** _hostname_<br>Output the FQDN (fully qualified domain name) for _hostname_.|
|<a id="idx+funcs+hosttoipaddr"></a>**\_host\_to\_ipaddr**|Usage: **\_host\_to\_ipaddr** _hostname_<br>Output the IPv4 address (excluding loopback) of _hostname_.|
|<a id="idx+funcs+hosttoipv6addrs"></a>**\_host\_to\_ipv6addrs**|Usage: **\_host\_to\_ipv6addrs** _hostname_<br>Output all the IPv6 connections strings (excluding loopback) for the host _hostname_ which must be running a contactable **pmcd**(1).|
|<a id="idx+funcs+ipaddrtohost"></a>**\_ipaddr\_to\_host**|Usage: **\_ipaddr\_to\_host** _ipaddr_<br>Output the hostname associated with the IPv4 address _ipaddr_.  No output if the lookup fails or an attempt to **ping**(1) the IP address fails.|
|<a id="idx+funcs+ipv6localhost"></a>**\_ipv6\_localhost**|Usage: **\_ipv6\_localhost**<br>Output the IPv6 connection string for localhost. Emit an error message on stderr if this cannot be found.|
|<a id="idx+funcs+libvirtisok"></a>**\_libvirt\_is\_ok**|Usage: **\_libvirt\_is\_ok**<br>Check if _libvirt_ and in particular the Python wrapper for _libvirt_ seems to be OK ... historically some versions were prone to core dumps. Returns **1** for known to be bad, else **0**.|
|<a id="idx+funcs+machineid"></a>**\_machine\_id**|Usage: **\_machine\_id**<br>Output the machine id signature for the host running QA.  For Linux systems this is the SHA from _/etc/machine-id_ else the dummy value **localmachine**.|
|<a id="idx+funcs+makehelptext"></a>**\_make\_helptext**|Usage: **\_make\_helptext** _pmda_<br>Each PMDA provides "help" text for the metrics it exports.  For some PMDAs this is in text files which need to be converted into the format required by the library routines that underpin each PMDA. This function optionally runs **newhelp**(1), checks that the necessary files are in place for the _pmda_ PMDA, and returns **0** if all is well. If there is a problem, error messages are output and the return value is **1**.|
|<a id="idx+funcs+makeprocstat"></a>**\_make\_proc\_stat**|Usage: **\_make\_proc\_stat** _path_ _ncpu_<br>Generate dummy lines for a Linux _/proc/stat_ file and write them to the file _path_. The **cpu** and **cpu**_N_ lines are generated for a machine with _ncpu_ CPUs.|
|<a id="idx+funcs+needmetric"></a>**\_need\_metric**|Usage: **\_need\_metric** _metric_<br>Check if _metric_ is available from the local **pmcd**(1). If not available, call [**\_notrun**](#idx+funcs+notrun) with an appropriate message.<br>Use this check for a _metric_ that is required by a QA test but _metric_ is not universally available, e.g. kernel metrics that are not present on all platforms.<br>See also the [**\_avail\_metric**](#idx+funcs+availmetric) and [**\_check\_metric**](#idx+funcs+checkmetric) functions above.|
|<a id="idx+funcs+notrun"></a>**\_notrun**|Usage: **\_notrun** _message_<br>Not all tests are expected to be able to run on all platforms. Reasons might include: won't work at all on a certain operating system, application required by the test is not installed, metric required by the test is not available from **pmcd**(1), etc.<br>In these cases, the test should include a guard that captures the required precondition and call **\_notrun** with a helpful _message_ if the guard fails. For example.<br>&nbsp;&nbsp;&nbsp;`which pmrep >/dev/null 2>&1 || _notrun "pmrep not installed"`|
|<a id="idx+funcs+pathreadable"></a>**\_path\_readable**|Usage: **\_path\_readable** _user_ _path_<br>Determine if the file _path_ is readable by the login _user_. Return **0** if OK, else returns **1** after emitting reason(s) on stderr.|
|<a id="idx+funcs+pidincontainer"></a>**\_pid\_in\_container**|Usage: **\_pid\_in\_container** _pid_<br>Return **0** if process _pid_ is definitely running in a container (assumes Linux and some heuristic pattern matching against _/proc/pid/cgroup_), otherwise return **1**.|
|**\_prepare\_pmda**|Refer to the [PMDA Install and Remove](#pmda-install-and-remove) section below.|
|**\_prepare\_pmda\_install**|Refer to the [PMDA Install and Remove](#pmda-install-and-remove) section below.|
|<a id="idx+funcs+preparepmdammv"></a>**\_prepare\_pmda\_mmv**|Usage: **\_prepare\_pmda\_mmv**<br>Save the directory of mmap'd files used by the **mmv** PMDA, pending installation of a new configuration for this PMDA.<br>The test should call [**\_restore\_pmda\_mmv**](#idx+funcs+restorepmdammv) before exiting.|
|<a id="idx+funcs+privatepmcd"></a>**\_private\_pmcd**|See the [**\_private\_pmcd**](#privatepmcd) section below.|
|<a id="idx+funcs+pstcpport"></a>**\_ps\_tcp\_port**|Usage: **\_ps\_tcp\_port** _port_<br>Output details of processes listening on TCP port _port_.|
|<a id="idx+funcs+pstreeall"></a>**\_pstree\_all**|Usage: **\_pstree\_all** _pid_<br>Show all the ancestors and descendent of the process with PID _pid_. Hides the platform-specific differences in how **pstree**(1) needs to be called.|
|<a id="idx+funcs+pstreeoneline"></a>**\_pstree\_oneline**|Usage: **\_pstree\_oneline**<br>One line summary version of **\_pstree\_all**.|
|<a id="idx+funcs+restoreautorestart"></a>**\_restore\_auto\_restart**|Usage: **\_restore\_auto\_restart** _service_<br>See [**\_stop\_auto\_restart**](#idx+funcs+stopautorestart).|
|<a id="idx+funcs+removejobscheduler"></a>**\_remove\_job\_scheduler**|Usage: **\_remove\_job\_scheduler** _cron_ _systemd_ _sudo_<br>Disable all PCP service timers (assumes [**\_check\_job\_scheduler**](#idx+funcs+checkjobscheduler) was called earlier). Prior **cron**(1) state (if any) is saved in the file _cron_. Prior **systemd**(1) state (if any) is saved in the file _systemd_. Uses _sudo_ as the **sudo**(1) command.<br>Refer to [**\_check\_job\_scheduler**](#idx+funcs+checkjobscheduler) for details.|
|<a id="idx+funcs+restoreconfig"></a>**\_restore\_config**|Usage: **\_restore\_config** _target_<br>Reinstates a configuration file or directory _target_ previously saved with [**\_save\_config**](#idx+funcs+saveconfig).|
|<a id="idx+funcs+restorejobscheduler"></a>**\_restore\_job\_scheduler**|Usage: **\_restore\_job\_scheduler** _cron_ _systemd_ _sudo_<br>Re-enable PCP service timers (assumes [**\_remove\_job\_scheduler**](#idx+funcs+removejobscheduler) was called earlier). Desired **cron**(1) state (if any) was previously saved in the file _cron_. Desired **systemd**(1) state (if any) was previously saved in the file _systemd_. Uses _sudo_ as the **sudo**(1) command.<br>Refer to [**\_check\_job\_scheduler**](#idx+funcs+checkjobscheduler) for details.|
|<a id="idx+funcs+restoreloggers"></a>**\_restore\_loggers**|Usage: **\_restore\_loggers**<br>Reverses the changes from [**\_disable\_loggers**](#idx+funcs+disableloggers), see above.|
|**\_restore\_pmda\_install**|Refer to the [PMDA Install and Remove](#pmda-install-and-remove) section below.|
|<a id="idx+funcs+restorepmdammv"></a>**\_restore\_pmda\_mmv**|Usage: **\_restore\_pmda\_mmv**<br>Restore the directory of mmap'd files used by the **mmv** PMDA saved in an earlier call to [**\_prepare\_pmda\_mmv**](#idx+funcs+preparepmdammv).|
|<a id="idx+funcs+restorepmloggercontrol"></a>**\_restore\_pmlogger\_control**|Usage: **\_restore\_pmlogger\_control**<br>Edit the various **pmlogger**(1) control files below the **$PCP\_SYSCONF\_DIR**_/pmlogger_ directory to ensure that only the primary **pmlogger** is enabled.<br>Only the control files are changed, the caller needs to follow up with<br>`$ _service pmlogger restart`<br>for any change to take effect.|
|<a id="idx+funcs+restoreprimarylogger"></a>**\_restore\_primary\_logger**|Usage: **\_restore\_primary\_logger**<br>Ensure the configuration file for the primary **pmlogger**(1) does _not_ allow **pmlc**(1) to make state changes to dynamically add or delete metrics to be logged (this is the default after PCP installation).<br>This function effectively reverses the changes made by [**\_writable\_primary\_logger**](#idx+funcs+writableprimarylogger).|
|<a id="idx+funcs+saveconfig"></a>**\_save\_config**|Usage: **\_save\_config** _target_<br>Save a configuration file or directory _target_ with a name that uses [$seq](#idx+vars+seq) so that if a test aborts we know who was dinking with the configuration.<br>Operates in concert with [**\_restore\_config**](#idx+funcs+restoreconfig).|
|<a id="idx+funcs+service"></a>**\_service**|Usage: **\_service** \[**-v**] _service_ _action_<br>Controlling services like **pmcd**(1) or **pmlogger**(1) or ... may involve **init**(1) or **systemctl**(1) or something else. This complexity is hidden behind the **\_service** function which should be used whenever a test wants to control a PCP service.<br>Supported values for _service_ are: **pmcd**, **pmlogger**, **pmproxy** **pmie**.<br>_action_ is one of **stop**, **start** (may be no-op if already started) or **restart** (force stop if necessary, then start).<br>Use **-v** for more verbosity.|
|<a id="idx+funcs+setdsosuffix"></a>**\_set\_dsosuffix**|Usage: **\_set\_dsosuffix**<br>Set the shell variable <a id="idx+vars+dsosuffix"></a>**$DSOSUFFIX** to the suffix used for shared libraries. This is platform-specific, but for Linux it is **so**.|
|<a id="idx+funcs+sighuppmcd"></a>**\_sighup\_pmcd**|Usage: **\_sighup\_pmcd**<br>Send **pmcd**(1) a SIGHUP signal and reliably check that it received (at least) one.<br>Returns **0** on success, else the return value is **1** and an explanatory message is output.|
|<a id="idx+funcs+startuppmlogger"></a>**\_start\_up\_pmlogger**|Usage: **\_start\_up\_pmlogger** _arg_ ...<br>Start a new **pmlogger**(1) instance in the background with appropriate privileges so that it can create the portmap files in **$PCP\_TMP\_DIR** and thus be managed by **pmlogctl**(1) or **pmlc**(1).  All of the _arg_ arguments are passed to the new **pmlogger** and the process ID of the new **pmlogger** is returned in <a id="idx+vars+pid"></a>**$pid** (which will be empty if **pmlogger** was not started).<br>The process will be run as the user **pcp** (or **root** if **pcp** is not available), so the current directory needs to be writable by that user and the test's **_cleanup** function needs to use **$sudo** to remove the files created by **pmlogger**.|
|<a id="idx+funcs+stopautorestart"></a>**\_stop\_auto\_restart**|Usage: **\_stop\_auto\_restart** _service_<br>When testing error handling or timeout conditions for services it may be important to ensure the system does not try to restart a failed service (potentially leading to an hard loop of retry-fail-retry). **\_stop\_auto\_restart** will change system configuration to prevent restarting for _service_ if the platform supports this function.<br>Use [**\_restore\_auto\_restart**](#idx+funcs+restoreautorestart) with the same _service_ to reinstate the system configuration.|
|<a id="idx+funcs+systemctlstatus"></a>**\_systemctl\_status**|Usage: **\_systemctl\_status** _service_<br>If the service _service_ is being managed by **systemd**(1) then call **systemctl**(1) and **journalctl**(1) to provide a verbose report on the status of the service.  Mostly used by other functions in this group in the event of failure to start or stop _service_.|
|<a id="idx+funcs+triagepmcd"></a>**\_triage\_pmcd**|Usage: **\_triage\_pmcd**<br>Produce a triage report for a failing **pmcd**(1).|
|<a id="idx+funcs+triagewaitpoint"></a>**\_triage\_wait\_point**|See the [**\_triage\_wait\_point**](#triagewaitpoint) section below.|
|<a id="idx+funcs+trypmlc"></a>**\_try\_pmlc**|Usage: **\_try\_pmlc** \[_expect_]<br>The **pmlc**(1) application interrogates and controls **pmlogger**(1) instances, but it may not succeed if some other **pmlc** is interacting with the target **pmlogger** (this is expected when **pmlogger** timer services are running concurrently with QA).<br>On entry to this function, the desired **pmlc** commands (including the **connect** command to identify the **pmlogger** of interest) must be in the file **$tmp**.pmlc. This function will then try up to 10 times (with 0.1 sec sleeps between tries) to run **pmlc**. A record of the successes or failures is appended to **$seq\_full**. On failure after 10 attempts, if the optional argument _expect_ is **expect-failure** the function quietly returns, else an error message and the last output (stdout and stderr) from **pmlc** is reported.|
|<a id="idx+funcs+waitforpmcd"></a>**\_wait\_for\_pmcd**|Usage: **\_wait\_for\_pmcd** \[_maxdelay_ \[_host_ \[_port_]]]<br>Wait for **pmcd**(1) to start. The arguments are optional, but the parsing is a bit Neanderthal so if you need to specify an argument you need to specify all the preceding arguments in the order above. The defaults are _maxdelay_ **20** (seconds), _host_ **localhost** and _port_ "" (so the default **$PMCD\_PORT**).<br>**pmcd** is considered "started" when it returns a value for the `pmcd.numclient` metric and **\_wait\_for\_pmcd** returns **0**. If, after _maxdelay_ iterations (each with a 1 sec sleep), **pmcd** does not respond, then a detailed triage report is produced and **\_wait\_for\_pmcd** returns **1**.|
|<a id="idx+funcs+waitforpmcdstop"></a>**\_wait\_for\_pmcd\_stop**|Usage: **\_wait\_for\_pmcd\_stop** \[_maxdelay_]<br>Wait for **pmcd**(1) to stop. _maxdelay_ defaults to **20** (seconds).<br>**pmcd** is considered "stopped" when it does not return a value for the `pmcd.numclient` metric and **\_wait\_for\_pmcd\_stop** returns **0**. If, after _maxdelay_ iterations (each with a 1 sec sleep), **pmcd** is still responding, then a detailed triage report is produced and **\_wait\_for\_pmcd\_stop** returns **1**.<br>See also [**\_wait\_pmcd\_end**](#idx+funcs+waitpmcdend).|
|<a id="idx+funcs+waitforpmie"></a>**\_wait\_for\_pmie**|Usage: **\_wait\_for\_pmie**<br>Wait for the primary **pmie**(1) process to get started as indicated by the presence of the **$PCP\_RUN\_DIR**_/pmie.pid_ file. Return **0** on success, else returns **1** with a status report on failure after 10 secs.|
|<a id="idx+funcs+waitforpmlogger"></a>**\_wait\_for\_pmlogger**|Usage: **\_wait\_for\_pmlogger** \[_pid_ \[_logfile_ \[_maxdelay_]]]<br>Wait for a **pmlogger**(1) instance to start. The arguments are optional, but the parsing is a bit Neanderthal so if you need to specify an argument you need to specify all the preceding arguments in the order above. The defaults are _pid_ **-P** (the primary **pmlogger** instance), _logfile_ **$PCP\_ARCHIVE\_DIR**_/$(hostname)/pmlogger.log_ and _maxdelay_ **20** (seconds).<br>The designated **pmlogger** is considered "started" when it can be contacted via **pmlc**(1). On success **\_wait\_for\_pmlogger** returns **0**. If, after _maxdelay_ iterations (each with a 1 sec sleep), **pmlogger** does not respond, then a detailed triage report is produced including the contents of _logfile_ and **\_wait\_for\_pmlogger** returns **1**.|
|<a id="idx+funcs+waitforpmproxy"></a>**\_wait\_for\_pmproxy**|Usage: **\_wait\_for\_pmproxy** \[_port_] \[_logfile_]]<br>Wait for **pmproxy**(1) to start. The arguments are optional, but the parsing is a bit Neanderthal so if you need to specify an argument you need to specify all the preceding arguments in the order above. The defaults are _port_ **44322** and _logfile_ **$PCP\_LOG\_DIR**_/pmproxy/pmproxy.log_.<br>**pmproxy** is considered "started" when it accepts TCP connections on port _port_ and **\_wait\_for\_pmproxy** returns **0**. If, after 20 iterations (each with a 1 sec sleep), **pmproxy** does not respond then a status report is produced and **\_wait\_for\_pmproxy** returns **1**.<br>See also [**\_wait\_for\_pmproxy\_logfile**](#idx+funcs+waitforpmproxylogfile) and [**\_wait\_for\_pmproxy\_metrics**](#idx+funcs+waitforpmproxymetrics).|
|<a id="idx+funcs+waitforpmproxylogfile"></a>**\_wait\_for\_pmproxy\_logfile**|Usage: **\_wait\_for\_pmproxy\_logfile** _logfile_<br>Called after **\_wait\_for\_pmproxy**, this function waits up to 5 secs for the file _logfile_ to appear. Returns **0** on success, else returns **1**.|
|<a id="idx+funcs+waitforpmproxymetrics"></a>**\_wait\_for\_pmproxy\_metrics**|Usage: **\_wait\_for\_pmproxy\_metrics**<br>Called after **\_wait\_for\_pmproxy**, this function waits up to 5 secs for values of the metrics `pmproxy.pid`, `pmproxy.cpu` and `pmproxy.map.instance.size` to be available. Returns **0** on success, else returns **1**.|
|<a id="idx+funcs+waitforport"></a>**\_wait\_for\_port**|Usage: **\_wait\_for\_port** _port_<br>Wait up to 5 secs for a process running on the local system to be accepting connections on TCP port _port_. Returns **0** for success, else returns **1**.|
|<a id="idx+funcs+waitpmcdend"></a>**\_wait\_pmcd\_end**|Usage: **\_wait\_pmcd\_end**<br>Wait up to 10 secs until **pmcd**(1) is no longer running, usually called after `_service pmcd stop`. If the process is running, calls [**\_wait\_process\_end**](#idx+funcs+waitprocessend).<br>Returns **0** on success, else returns **1**.<br>See also [**\_wait\_for\_pmcd\_stop**](#idx+funcs+waitforpmcdstop).|
|<a id="idx+funcs+waitpmieend"></a>**\_wait\_pmie\_end**|Usage: **\_wait\_pmie\_end**<br>Wait up to 10 secs until the primary **pmie**(1) is no longer running, usually called after `_service pmie stop`. If the process is running, calls [**\_wait\_process\_end**](#idx+funcs+waitprocessend).<br>Returns **0** on success, else returns **1**.|
|<a id="idx+funcs+waitpmlogctl"></a>**\_wait\_pmlogctl**|Usage: **\_wait\_pmlogctl**<br>Wait up to 60 secs until **pmlogctl**(1)'s lock file (**$PCP\_ETC\_DIR**_/pcp/pmlogger/lock_) has been removed.|
|<a id="idx+funcs+waitpmloggerend"></a>**\_wait\_pmlogger\_end**|Usage: **\_wait\_pmlogger\_end** \[_pid_]<br>Wait up to 10 secs until a **pmlogger**(1) instance is no longer running, usually called after `_service pmlogger stop`. The pmlogger instance is identified by _pid_ else the primary pmlogger if _pid_ is not specified. If the process is running, calls [**\_wait\_process\_end**](#idx+funcs+waitprocessend).<br>Returns **0** on success, else returns **1**.|
|<a id="idx+funcs+waitpmproxyend"></a>**\_wait\_pmproxy\_end**|Usage: **\_wait\_pmproxy\_end**<br>Wait up to 10 secs until **pmproxy**(1) is no longer running, usually called after `_service pmproxy stop`. If the process is running, calls [**\_wait\_process\_end**](#idx+funcs+waitprocessend).<br>Returns **0** on success, else returns **1**.|
|<a id="idx+funcs+waitprocessend"></a>**\_wait\_process\_end**|Usage: **\_wait\_process\_end** \[_tag_] _pid_<br>Wait up to 10 seconds for process _pid_ to vanish. _tag_ is used as a prefix for any output and defaults to **\_wait\_process\_end**. If process _pid_ vanishes, return **0**, else return **1** and output a message. If process _pid_ has exited, but has not yet gone away because it has not been harvested by its parent process, then return **0** and further details are appended to [**$seq\_full**](#idx+vars+seqfull).|
|<a id="idx+funcs+webapiheaderfilter"></a>**\_webapi\_header\_filter**|Usage: **\_webapi\_header\_filter**<br>This filter makes HTTP header responses from **pmproxy**(1) deterministic by replacing variable components (sizes, dates, context numbers, version numbers, ...) by constant strings and deleting some optional header lines.<br>The unfiltered input to **\_webapi\_header\_filter** is appended to [**$seq\_full**](#idx+vars+seqfull).|
|<a id="idx+funcs+webapiresponsefilter"></a>**\_webapi\_response\_filter**|Usage: **\_webapi\_response\_filter**<br>This filter makes HTTP responses from **pmproxy**(1) deterministic by replacing variable components (hostnames, ip addresses, sizes, dates, version numbers, ...) by constant strings and deleting some optional header lines and diagnostics.<br>The unfiltered input to **\_webapi\_response\_filter** is appended to [**$seq\_full**](#idx+vars+seqfull).|
|<a id="idx+funcs+withintolerance"></a>**\_within\_tolerance**|Usage: **\_within\_tolerance** _name_ _observed_ _expected_ _mintol_ \[_maxtol_] \[**-v**]<br>When tests report the values of performance metrics, especially from live systems, the acceptable values may fall within a range and in this case outputting the metric's value makes the test non-deterministic. This function determines if the _observed_ value is within an acceptable range as defined by _expected_ minus _mintol_ to _expected_ plus _maxtol_.<br>_maxtol_ defaults to _mintol_ and these arguments are percentages (with an optional **%** suffix to aid readability).<br>If the _observed_ value is within the specified range the function returns **0** else the return value is **1**.<br>The **-v** option outputs an explanation as well.|
|<a id="idx+funcs+writableprimarylogger"></a>**\_writable\_primary\_logger**|Usage: **\_writable\_primary\_logger**<br>Ensure the configuration file for the primary **pmlogger**(1) allows **pmlc**(1) to make state changes to dynamically add or delete metrics to be logged.<br>Assumes the configuration file is the default one generated by **pmlogconf**(1) and calls [**\_save\_config**](#idx+funcs+saveconfig) to save the old configuration file, so the test **must** call either [**\_restore\_primary\_logger**](#idx+funcs+restoreprimarylogger) or [**\_restore\_config**](#idx+funcs+restoreconfig) with the argument **$PCP\_VAR\_DIR**_/config/pmlogger/config.default_ before exiting.<br>Does not restart the primary **pmlogger**(1) instance, so the caller must do that before the changes take effect.|

<a id="pmda-install-and-remove"></a>
## 9.1 PMDA Install and Remove

Many QA tests are designed to exercise an individual [PMDA](#idx+pmda)
and each PMDA has its own **Install** and **Remove** script to
handle installation and removal (optional building, creation of help
text files, updating **pmcd**(1)'s configuration file and signalling
**pmcd**).  But most PMDAs are **not** installed by default, so special
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

<a id="idx+funcs+preparepmda"></a>**\_prepare\_pmda**<br>
Usage: **\_prepare\_pmda** _pmda_ \[_name_]<br>
Called before any PMDA changes are made to ensure the state of the
_pmda_ PMDA will be restored at the end of the test when the companion
function [**\_cleanup\_pmda**](#idx+funcs+cleanuppmda) is called in the
test's **\_cleanup** function. _name_ is the name of a metric in the
[PMNS](#idx+pmns) that belongs to the _pmda_ PMDA, so it can be used to
probe the PMDA; if _name_ is not provided, it defaults to _pmda_.

This will:

- note if the _pmda_ PMDA is already installed
- call [**\_save\_config**](#idx+funcs+saveconfig) to save **pmcd**'s configuration file (**$PCP\_PMCDCONF\_PATH**)

And before the test exits, call

<a id="idx+funcs+cleanuppmda"></a>**\_cleanup\_pmda**<br>
Usage: **\_cleanup\_pmda** _pmda_ \[_install-config_]<br>
Called at the end of a test to restore the state of the _pmda_
PMDA to the state it was in when the companion
function [**\_prepare\_pmda**](#idx+funcs+preparepmda) was
called. _install-config_ is an optional input file for the PMDA's
**Install** script to be used if the PMDA needs to be re-installed
(default is _/dev/null_).

This will:

- check and call `_exit 1` if **\_cleanup\_pmda** is called without a prior call to **\_prepare\_pmda**
- check and call `_exit 1` if **\_cleanup\_pmda** is called twice
- call [**\_restore\_config**](#idx+funcs+restoreconfig) to reinstate **pmcd**'s configuration file (**$PCP\_PMCDCONF\_PATH**)
- call [**\_restore\_auto\_restart**](#idx+funcs+restoreautorestart), [**\_service**](#idx+funcs+service) and [**\_wait\_for\_pmcd**](#idx+funcs+waitforpmcd) to restart **pmcd**
- call [**\_service**](#idx+funcs+service) and [**\_wait\_for\_pmlogger**](#idx+funcs+waitforpmlogger) to restart (the primary) **pmlogger**
- reinstall the PMDA if needed

<a id="plan-b"></a>
### Plan B

Initially, call

<a id="idx+funcs+preparepmdainstall"></a>**\_prepare\_pmda\_install**<br>
Usage: **\_prepare\_pmda\_install** _pmda_<br>
Called before any PMDA changes are made to ensure the state of the
_pmda_ PMDA will be restored at the end of the test when the companion
function [**\_restore\_pmda\_install**](#idx+funcs+restorepmdainstall)
is called in the test's **\_cleanup** function.

This will:

- call [**\_save\_config**](#idx+funcs+saveconfig) to save **pmcd**'s configuration file (**$PCP\_PMCDCONF\_PATH**)
- **cd** into the PMDA's directory (**$PCP\_PMDAS\_DIR/**_pmda_)
- if there is a _Makefile_, run **make**(1) and return **1** if this fails
- run _./Remove_ to remove the PMDA if it is already installed
- save **$PCP\_VAR\_DIR/config/**_pmda_**/**_pmda_**.conf** if it exists
- return **0**

Note this leaves the calling shell in a different directory to the one
it was in when **\_prepare\_pmda\_install** was called.

And before the test exits, call

<a id="idx+funcs+restorepmdainstall"></a>**\_restore\_pmda\_install**<br>
Usage: **\_restore\_pmda\_install** _pmda_ \[_reinstall_]<br>
Called at the end of a test to (sort of) restore the state of the
_pmda_ PMDA to the state it was in when the companion function
[**\_prepare\_pmda\_install**](#idx+funcs+preparepmdainstall) was
called. If _reinstall_ is not specified the PMDA's **Install** script
will be used if the PMDA needs to be re-installed with input coming
from _/dev/null_, else do not re-install the PMDA.

<a id="privatepmcd"></a>
## 9.2 **\_private\_pmcd**

Usage: **\_private\_pmcd** \[_config_]

This function starts a new **pmcd**(1) running privately as a non-daemon background process
listening on a non-standard port.
The **pmcd** configuration used is either the file _config_ if specified
(and this must include the **pmcd** PMDA), else the
**sampledso** and **pmcd** PMDA lines taken from **$PCP\_PMCDCONF\_PATH**.
This **pmcd** is completely isolated, so other services
can continue to use the system's **pmcd** concurrent with testing of the private **pmcd**.
The new **pmcd** runs with the user id of the caller (not **pcp** or **root**),
although of course

```bash
$sudo _private_pmcd
```

is always an option to start **pmcd** as **root** if required for the PMDA being tested.

On entry <a id="idx+vars+pmcdargs"></a>**$pmcd\_args** is optionally set to any additional arguments for **pmcd**.

On return

- <a id="idx+vars+pmcdpid"></a>**$pmcd\_pid** is the PID of the new **pmcd**.
- <a id="idx+vars+pmcdport"></a>**$pmcd\_port** is the port **pmcd** is listening on and this is also set in the environment as **$PMCD\_PORT**.
- **$tmp**_/pmcd.socket_ Unix domain socket for **pmcd** requests
- **$tmp**_/pmcd.conf_ **pmcd**'s configuration file
- **$tmp**_/pmcd.out_ stdout for **pmcd**
- **$tmp**_/pmcd.err_ stderr for **pmcd**
- **$tmp**_/pmcd.log_ **pmcd**'s log file

To cleanup and terminate the private **pmcd** the test should do the following before exiting:
```bash
$ kill -TERM $pmcd_pid
```

<a id="triagewaitpoint"></a>
## 9.3 **\_triage\_wait\_point**

Usage: **\_triage\_wait\_point** \[_file_] _message_

If you have a QA test that needs triaging after it has done some setup
(e.g. start a QA version of a daemon, install a PMDA, or unpack a
"fake" set of kernel stats files, create an archive), then add a call
to **\_triage\_wait\_point** in the test once the setup has been done.

The _message_ must be specified and it will be echoed; this allows the test
to disclose useful information from the setup, e.g. a process ID or the
path to where the magic files have been unpacked.

If _file_ is missing, the default file name is **$seq**_.wait_.
Specifying _file_ allows for multiple wait points with different
files as the guard so a test can be paused at more than one point of
interest.

Now, to triage the test:

```bash
$ touch $seq.wait
$ ./$seq
```

and when **\_triage\_wait\_point** is called it will output _message_
and go into a sleep-check loop waiting
for **$seq**_.wait_ to disappear.
So at this point the test is paused and you can go poke a process, look
at a log file, check an archive, ...

When the triage is done,

```bash
$ rm $seq.wait
```
and the test will resume.

<a id="shell-functions-from-common.filter"></a>
# 10 Shell functions from _common.filter_

Because filtering output to produce deterministic results is such a
key part of the PCP QA methodology, a number of common filtering
functions are provided by _common.filter_ as described below.

Except where noted, these functions as classical Unix "filters" reading
from standard input and writing to standard output.  Most do not have
arguments, except where noted with a _Usage_ message.
<br>

|**Function**|**Description**|
|---|---|
|<a id="idx+funcs+cullduplines"></a>**\_cull\_dup\_lines**|Cull repeated lines (unlike **uniq**(1), input does not have to be sorted).|
|<a id="idx+funcs+filterallpcpstart"></a>**\_filterall\_pcp\_start**|Pipes input into [**\_filter\_pcp\_start**](#idx+funcs+filterpcpstart) and then removes some additional optional chatter associated with first restart of **pmcd**(1) after a package install.|
|<a id="idx+funcs+filtercompilerbabble"></a>**\_filter\_compiler\_babble**|Some versions of the C compiler are just plain wrong and emit bogus warnings when re-compiling the sample PMDA.  Filter the babble away.|
|<a id="idx+funcs+filterconsole"></a>**\_filter\_console**|Input is the "console" output from Qt applications and **pmchart**(1) in particular. Replace some text to make the output deterministic.|
|<a id="idx+funcs+filtercronscripts"></a>**\_filter\_cron\_scripts**|Rewrite platform-specific paths in **cron**(1) scripts.|
|<a id="idx+funcs+filterdbg"></a>**\_filter\_dbg**|Replace timestamps in some **-D** output (refer to **pmdbg**(1)) with the constant **TIMESTAMP**.|
|<a id="idx+funcs+filterdumpresult"></a>**\_filter\_dumpresult**|Filter output from **pmDumpResult**(3), specifically replacing text by generic and deterministic strings for timestamps, values and some instance names.<br>Useful when the test needs to know that **pmFetch**(3) returned "something" reasonable, but the details are not relevant to the functionality being tested.|
|<a id="idx+funcs+filterls"></a>**\_filter\_ls**|Usage: **\_filter\_ls** \[**-u**] \[**-g**] \[**-s**]<br>The command line arguments to **ls**(1) and even the output format varies from platform to platform. Hide the format differences with this filter that expects the input to be from `ls -l`. The output is the mode (stripped of any trailing ".", so 10 characters), the user name (omit with **-u**), the group name (omit with **-g**) and the size (omit with **-s**).|
|<a id="idx+funcs+filteroptionallabels"></a>**\_filter\_optional\_labels**|Input is a report of metric and instances with labels, e.g. from **pminfo**(1) **-l**. Some labels are optional. If a test does not expect them to be present, but if they are in the files below the directory **$PCP\_ETC\_DIR**_/labels/optional_ then build a filter to remove them.  Details of the filtering are appended to **$seq\_full**.|
|<a id="idx+funcs+filteroptionalpmdainstances"></a>**\_filter\_optional\_pmda\_instances**|Input from **pminfo**(1) **-f** (or similar) for the `pmcd.agent` metrics. The configuration of PMDAs that have been installed for **pmcd**(1) varies from platform to platform and time to time. This filter removes the instance lines for optional PMDAs that may, or may not, be active.|
|<a id="idx+funcs+filteroptionalpmdas"></a>**\_filter\_optional\_pmdas**|Input could be a **pmcd**(1) log file or output from **ps**(1). The configuration of PMDAs that have been installed for **pmcd**(1) varies from platform to platform and time to time. This filter removes lines that contain the name of the PMDA's executable for all optional PMDAs.|
|<a id="idx+funcs+filterpcprestart"></a>**\_filter\_pcp\_restart**|Pipes input into [**\_filter\_pcp\_stop**](#idx+funcs+filterpcpstop) and then into [**\_filter\_pcp\_start**](#idx+funcs+filterpcpstart).|
|<a id="idx+funcs+filterpcpstartdistro"></a>**\_filter\_pcp\_start\_distro**|Hook to filter away any babble produced on a specific platform when PCP daemons are started.  Currently a no-op.|
|<a id="idx+funcs+filterpcpstart"></a>**\_filter\_pcp\_start**|Input is from [**_service**](#idx+funcs+service) starting or restarting **pmcd** and/or **pmlogger**. Platform-specific paths are rewritten, optional chatter from the "rc" scripts or the infrastructure that runs them (e.g. **init**(1) or **systemctl**(1)) is removed.<br>Also calls [**\_filter\_pcp\_start\_distro**](#idx+funcs+filterpcpstartdistro) and [**\_filter\_init\_distro**](#idx+funcs+filterinitdistro).|
|<a id="idx+funcs+filterpcpstop"></a>**\_filter\_pcp\_stop**|Input is from [**_service**](#idx+funcs+service) stopping **pmcd** and/or **pmlogger**. Platform-specific paths are rewritten, optional chatter from the "rc" scripts or the infrastructure that runs them (e.g. **init**(1) or **systemctl**(1)) is removed.<br>Also calls [**\_filter\_init\_distro**](#idx+funcs+filterinitdistro).|
|<a id="idx+funcs+filterpmcdlog"></a>**\_filter\_pmcd\_log**|Input is a _pmcd.log_ file from **pmcd**(1).|
|<a id="idx+funcs+filterpmdainstall"></a>**\_filter\_pmda\_install**|Input is from a PMDA's **Install** script. Remove the non-deterministic chatter from installing a PMDA and notifying **pmcd**(1).<br>Calls [**\_filter\_pcp\_start**](#idx+funcs+filterpcpstart).|
|<a id="idx+funcs+filterpmdaremove"></a>**\_filter\_pmda\_remove**|Input is from a PMDA's **Remove** script. Remove the non-deterministic chatter from uninstalling a PMDA and notifying **pmcd**(1).<br>Calls [**\_filter\_pmda\_install**](#idx+funcs+filterpmdainstall).|
|<a id="idx+funcs+filterpmdumplog"></a>**\_filter\_pmdumplog**|Usage: **\_filter\_pmdumplog** \[_opt_]<br>Input is from **pmlogdump**(1), also known as **pmdumplog**(1). Replace dates with **DATE**, timestamps with **TIMESTAMP**, host name with **HOST** and archive name with **ARCHIVE**. If _opt_ is **--any-version** also handle differences in format between V2 and V3 archives.|
|<a id="idx+funcs+filterpmdumptext"></a>**\_filter\_pmdumptext**|Input is from **pmdumptext**(1). Replace date and time with **DATE**.|
|<a id="idx+funcs+filterpmielog"></a>**\_filter\_pmie\_log**|Input is a _pmie.log_ file from **pmie**(1).|
|<a id="idx+funcs+filterpmiestart"></a>**\_filter\_pmie\_start**|Input is from [**_service**](#idx+funcs+service) starting or restarting **pmie**. Platform-specific paths are rewritten, optional chatter from the "rc" scripts or the infrastructure that runs them (e.g. **init**(1) or **systemctl**(1)) is removed.<br>Also calls [**\_filter\_init\_distro**](#idx+funcs+filterinitdistro).|
|<a id="idx+funcs+filterpmiestop"></a>**\_filter\_pmie\_stop**|Input is from [**_service**](#idx+funcs+service) stop for **pmie**. Platform-specific paths are rewritten, optional chatter from the "rc" scripts or the infrastructure that runs them (e.g. **init**(1) or **systemctl**(1)) is removed.<br>Also calls [**\_filter\_init\_distro**](#idx+funcs+filterinitdistro).|
|<a id="idx+funcs+filterpmproxylog"></a>**\_filter\_pmproxy\_log**|Input is a _pmproxy.log_ file from **pmproxy**(1).|
|<a id="idx+funcs+filterpmproxystart"></a>**\_filter\_pmproxy\_start**|Input is from [**_service**](#idx+funcs+service) starting or restarting **pmproxy**. Platform-specific paths are rewritten, optional chatter from the "rc" scripts or the infrastructure that runs them (e.g. **init**(1) or **systemctl**(1)) is removed.<br>Also calls [**\_filter\_init\_distro**](#idx+funcs+filterinitdistro).|
|<a id="idx+funcs+filterpmproxystop"></a>**\_filter\_pmproxy\_stop**|Input is from [**_service**](#idx+funcs+service) stop for **pmproxy**. Platform-specific paths are rewritten, optional chatter from the "rc" scripts or the infrastructure that runs them (e.g. **init**(1) or **systemctl**(1)) is removed.<br>Also calls [**\_filter\_init\_distro**](#idx+funcs+filterinitdistro).|
|<a id="idx+funcs+filterpost"></a>**\_filter\_post**|Input is stderr from **pmchart**(1).  Rewrite local hostname.|
|<a id="idx+funcs+filterslowpmie"></a>**\_filter\_slow\_pmie**|Input is stderr from **pmie**(1).  This filter removes diagnostics associated with delays in rule execution schedule that sometimes occur on busy QA machines.|
|<a id="idx+funcs+filtertoppmns"></a>**\_filter\_top\_pmns**|The [**PMNS**](#idx+pmns) contains top-level metric names that depend on the configuration of PMDAs that have been installed for **pmcd**(1). This filter removes the lines for top-level PMNS names that are associated with optional PMDAs that may, or may not, be in the reported PMNS.|
|<a id="idx+funcs+filtertortureapi"></a>**\_filter\_torture\_api**|Input is from the _src/torture\_api_ application.  Deal with variability in the PMNS and platform-specific path rewriting.<br>Calls [**\_filter\_top\_pmns**](#idx+funcs+filtertoppmns) and [**\_filter\_dumpresult**](#idx+funcs+filterdumpresult).|
|<a id="idx+funcs+filterviews"></a>**\_filter\_views**|Filter away chatter from Qt and **pmchart**(1).|
|<a id="idx+funcs+instancesfilterany"></a>**\_instances\_filter\_any**|Input from **pminfo**(1) **-f** (or similar). If any metric instance has a value, output "OK" else output nothing.|
|<a id="idx+funcs+instancesfilterexact"></a>**\_instances\_filter\_exact**|Usage: **\_instances\_filter\_exact** _match_<br>Input from **pminfo**(1) **-f** (or similar). If any metric instance has a value equal to _match_, output "OK" else output nothing.|
|<a id="idx+funcs+instancesfilternonzero"></a>**\_instances\_filter\_nonzero**|Input from **pminfo**(1) **-f** (or similar). If any metric instance has a value and the value is greater than zero, output "OK" else output nothing.|
|<a id="idx+funcs+instvaluefilter"></a>**\_inst\_value\_filter**|Input from **pminfo**(1) **-f** (or similar). Concatenate lines so that each metric instance has one value line.  Likely use is for `proc` metrics where the values are strings and may contain embedded newlines.|
|<a id="idx+funcs+quotefilter"></a>**\_quote\_filter**|Input is arbitrary.  Replace each newline embedded in quotes (") with a literal backslash-n (**\\n**).|
|<a id="idx+funcs+showpmieerrors"></a>**\_show\_pmie\_errors**|Input is stdout from **pmie**(1).  Strip header and footer lines (contain host name, date and time), blank lines and deal with some differences in message formatting.|
|<a id="idx+funcs+showpmieexit"></a>**\_show\_pmie\_exit**|Input is stdout from **pmie**(1).  Report just the "evaluator exiting" line in a canonical form.|
|<a id="idx+funcs+sortpmdumplogd"></a>**\_sort\_pmdumplog\_d**|Input is from **pmlogdump**(1) **-d**. This filter sorts the metadata into ascending [PMID](#idx+pmid) sequence and calls _src/hex2nbo_ to convert any hexadecimal numbers into network byte order.|
|<a id="idx+funcs+valuefilterany"></a>**\_value\_filter\_any**|Input from **pminfo**(1) **-f** (or similar). For all lines, if a metric instance has a value, replace the value with **OK**.|
|<a id="idx+funcs+valuefilternonzero"></a>**\_value\_filter\_nonzero**|Input from **pminfo**(1) **-f** (or similar). For all lines, if a metric instance has a value and the value is greater than zero, replace the value with **OK**.|

<a id="helper-scripts"></a>
# 11 Helper scripts

There are a large number of shell scripts in the QA directory that are
intended for common QA development and triage tasks beyond simply
running tests with [**check**](#check-script).<br>

|**Command**|**Description**|
|---|---|
|<a id="idx+cmds+all-by-group"></a>**all-by-group**|Report all tests (excluding those tagged **:retired** or **:reserved**) in _group_ sorted by group.|
|<a id="idx+cmds+appchange"></a>**appchange**|Usage: **appchange** \[**-c**] _app1_ \[_app2_ ...]<br>Recheck all QA tests that appear to use the test application _src/app1_ or _src/app2_ or ...<br>**${TMPDIR:-/tmp}**_/appcache_ is a cache of mappings between test numbers and application uses, for all applications in _src/_ and **appchange** will build the cache if it is not already there, use **-c** to clear and rebuild the cache.|
|<a id="idx+cmds+bad-by-group"></a>**bad-by-group**|Use the _\*.out.bad_ files to report failures by group.|
|<a id="idx+cmds+check.app.ok"></a>**check.app.ok**|Usage: **check.app.ok** _app_<br>When the test application _src/app.c_ (or similar) has been changed, this script<br>(a) remakes the application and checks **make**(1) status, and<br>(b) finds all the tests that appear to run the _src/app_ application and runs [**check**](#check-script) for these tests.|
|<a id="idx+cmds+check-auto"></a>**check-auto**|Usage: **check-auto** \[_seqno_ ...]<br>Check that if a QA script uses **\_stop\_auto\_restart** for a (**systemd**) service, it also uses **\_restore\_auto\_restart** (preferably in \_cleanup()). If no _seqno_ options are given then check all tests.|
|<a id="idx+cmds+check-flakey"></a>**check-flakey**|Usage: **check-flakey** \[_seqno_ ...\]<br>Recheck failed tests and try to classify them as "flakey" if they pass now, or determine if the failure is "hard" (same _seqno.out.bad_) or some other sort of non-deterministic failure. If no _seqno_ options are given then check all tests with a _\*.out.bad_ file.|
|<a id="idx+cmds+check-group"></a>**check-group**|Usage: **check-group** _query_<br>Check the _group_ file and test scripts for a specific _query_ that is assumed to be **both** the name of a command that appears in the test scripts (or part of a command, e.g. **valgrind** in **\_run\_valgrind**) and the name of a group in the _group_ file. Report differences, e.g. _command_ appears in the _group_ file for a specific test but is not apparently used in that test, or _command_ is used in a specific test but is not included in the _group_ file entry for that test.<br>There are some special cases to handle the pcp-foo commands, aliases and [PMDAs](#idx+pmda) ... refer to **check-group** for details.<br>Special control lines like:<br>`# check-group-include: group ...`<br>`# check-group-exclude: group ...`<br>may be embedded in test scripts to over-ride the heuristics used by **check-group**.|
|<a id="idx+cmds+check-pdu-coverage"></a>**check-pdu-coverage**|Check that PDU-related QA apps in _src_ provide full coverage of all current PDU types.|
|<a id="idx+cmds+check-setup"></a>**check-setup**|Check the QA environment is as expected. Documented in the [Distributed QA](#distributed-qa) section below but not used otherwise.|
|<a id="idx+cmds+check-vars"></a>**check-vars**|Check shell variables across the _common\*_ "include" files and the scripts used to run and manage QA. For the most part, the _common\*_ files should use a "\_\_" prefix for shell variables\[2] to insulate them from the use of arbitrarily named shell variables in the QA tests themselves (all of which "source" multiple of the _common\*_ files). **check-vars** also includes some exceptions which are a useful cross-reference.<br>\[2] If all shells supported the **local** keyword for variables we could use that, but that's not the case across all the platforms PCP runs on, so the "\_\_" prefix model is a weak substitute for proper variable scoping.|
|<a id="idx+cmds+cull-pmlogger-config"></a>**cull-pmlogger-config**|Cull the default **pmlogger**(1) configuration (**$PCP\_VAR\_DIR**_/config/pmlogger/config.default)_ to remove any **zeroconf** proc metric logging that threatens to fill the filesystem on small QA machines.|
|<a id="idx+cmds+daily-cleanup"></a>**daily-cleanup**|Run from [**check**](#check-script), this script will try to make sure the **pmlogger\_daily**(1) work has really been done; this is important for QA VMs that are only booted for QA and tend to miss the nightly **pmlogger\_daily**(1) run and this may lead to QA test failure.|
|<a id="idx+cmds+find-app"></a>**find-app**|Usage: **find-app** \[**-f**] _app_ ...<br>Find and report tests that use any of the QA applications _src/app_ ...<br>The **-f** argument changes the interpretation of _app_ from _src/app_ to "all the _src/\*_ programs" that call the function _app_.|
|<a id="idx+cmds+find-archive"></a>**find-archive**|Usage: **find-archive** _archive_ ...<br>Find and report tests that use any of the QA archives _archive_ ...<br>For common uses each _archive_ would be the PCP archive basename for one of the QA archives in the [_archives_](#archives), [_badarchives_](#badarchives), or [_tmparch_](#tmparch) directories|
|<a id="idx+cmds+find-bound"></a>**find-bound**|Usage: **find-bound** _archive_ _timestamp_ _metric_ \[_instance_]<br>Scan _archive_ for values of _metric_ (optionally constrained to the one _instance_) within the interval _timestamp_ (in the format HH:MM:SS, as per **pmlogdump**(1) and assuming a timezone as per **-z**).|
|<a id="idx+cmds+find-metric"></a>**find-metric**|Usage: **find-metric** \[**-a**\|**-h**] _pattern_ ...<br>Search for metrics with name or metadata that matches _pattern_. With **-h** interrogate the local **pmcd**(1), else with **-a** (the default) search all the QA archives in the directories _archive_ and _tmparch_.<br>Multiple pattern arguments are treated as a disjunction in the search which uses **grep**(1) style regular expressions. Metadata matches are against the **pminfo**(1) **-d** output for the type, instance domain, semantics, and units.|
|<a id="idx+cmds+flakey-summary"></a>**flakey-summary**|Assuming the output from **check-flakey** has been kept for multiple QA runs across multiple hosts and saved in a file called _flakey_, this script will summarize the test failure classifications.|
|<a id="idx+cmds+getpmcdhosts"></a>**getpmcdhosts**|Usage: **getpmcdhosts** _lots-of-options_<br>Find a remote host matching a selection criteria based on hardware, operating system, installed [PMDA](#idx+pmda), primary logger running, etc. Use<br>`$ ./getpmcdhosts -?`<br>to see all options.|
|<a id="idx+cmds+grind"></a>**grind**|Usage: **grind** _seqno_ \[...]<br>Run select test(s) in a loop until one of them fails and produces a _.out.bad_ file. Stop with Ctrl-C or for a more orderly end after the current iteration<br>`$ touch grind.stop`|
|<a id="idx+cmds+grind-pmda"></a>**grind-pmda**|Usage: **grind-pmda** _pmda_ _seqno_ \[...]<br>Exercise the _pmda_ [PMDA](#idx+pmda) by running the PMDA's **Install** script, then using [**check**](#check-script) to run all the selected tests, checking that the PMDA is still installed, running the PMDA's **Remove** script, then running the selected tests again and checking that the PMDA is still **not** installed.|
|<a id="idx+cmds+group-stats"></a>**group-stats**|Report test frequency by group, and report any group name anomalies.|
|<a id="idx+cmds+mk.localconfig"></a>**mk.localconfig**|Recreate the _localconfig_ file that provides the platform and PCP version information and the _src/localconfig.h_ file that can be used by C programs in the _src_ directory.|
|**mk.logfarm**|See the [**mk.logfarm**](#mk.logfarm-script) section.|
|**mk.qa\_hosts**|See the [**mk.qa\_hosts**](#mk.qahosts-script) section below.|
|<a id="idx+cmds+mk.variant"></a>**mk.variant**|Usage: **mk.variant** _seqno_<br>Sometimes a test has no choice other than to produce different output on different platforms. This script may be used to convert an existing test to accommodate multiple _seqno.out_ files.|
|**new**|See the [**new**](#the-new-script) section above.|
|<a id="idx+cmds+new-dup"></a>**new-dup**|Usage: **new-dup** \[**-n**] _seqno_<br>Make a copy of the test _seqno_ using a new test number as assigned by [**new**](#the-new-script), including rewriting the old _seqno_ in the new test and its new _.out_ file. **-n** is "show me" mode and no changes are made.|
|<a id="idx+cmds+new-grind"></a>**new-grind**|Usage: **new-grind** \[**-n**] \[**-v**] _seqno_<br>Make a functional copy of the test _seqno_ using a new test number as assigned by [**new**](#the-new-script) and arrange matters so the new test runs the old test but selects the **valgrind**(1) sections of that test. **-n** is "show me" mode and no changes are made, use **-v** for more verbosity.<br>The valgrind variant of the test is a wrapper that runs the non-valgrind variant and the original _seqno_ test must include conditional fragments to run the application being tested either directly (the default) or under the control of **valgrind**(1). See tests _1501_ and _1502_ for an example.|
|<a id="idx+cmds+new-seqs"></a>**new-seqs**|Report the unallocated blocks of test sequence numbers from the _group_ file.|
|<a id="idx+cmds+really-retire"></a>**really-retire**|Usage: **really-retire** _seqno_ \[...]<br>Mark the selected tests as **:retired** in the _group_ file and then replace the test and its _.out_ file with boilerplate text that explains what has happened and unconditionally calls [**\_notrun**](#idx+funcs+notrun) (in case the test is ever really run).|
|<a id="idx+cmds+recheck"></a>**recheck**|Usage: **recheck** \[**-t**] \[_options_] \[_seqno_ ...\]<br>Run [**check**](#check-script) again for failed tests. If no _seqno_ options are given then check all tests with a _\*.out.bad_ file. By default tests that failed last time and were classified as **triaged** will not be rerun, but **-t** overrides this. Other _options_ are any command line options that [**check**](#check-script) understands.|
|<a id="idx+cmds+remake"></a>**remake**|Usage: **remake** \[_options_] _seqno_ \[...]<br>Remake the _.out_ file for the specified test(s). Command line parsing is the same as [**check**](#check-script) so _seqno_ can be a single test sequence number, or a range, or a **-g** _group_ specification. Similarly **-l** selects **diff**(1) rather than a graphical diff tool to show the differences.<br>Since the _seqno.out_ files are precious and reflect the state of the qualified and expected output, they should typically not be changed unless some change has been made to the _seqno_ test or the applications the test runs produce different output or the filters in the test have changed.|
|<a id="idx+cmds+sameas"></a>**sameas**|Usage: **sameas** _seqno_ \[...]<br>See if _seqno.out_ and _seqno.out.bad_ are identical except for line ordering. Useful to detect cases where non-determinism is caused by the order in which subtests were run, e.g. sensitive to directory entry order in the filesystem or metric name order in the [PMNS](#idx+pmns).|
|<a id="idx+cmds+var-use"></a>**var-use**|Usage: **var-use** _var_ \[_seqno_ ...]<br>Find assignment and uses of the shell variable _$var_ in tests. If _seqno_ not specified, search all tests.|

<a id="mk.logfarm-script"></a>
## 11.1 <a id="idx+cmds+mk.logfarm"></a>**mk.logfarm** script

Usage: **mk.logfarm** \[**-c** _config_] _rootdir_

The **mk.logfarm** script creates a forest of archives suitable for
use with **pmlogger\_daily**(1) in tests.

The forest is rooted at the directory _rootdir_ which must exist. Most
often this would be **$tmp**.

A default configuration is hard-wired, but an alternative is read from
_config_ if **-c** is specified.

Each line in configuration contains 3 fields:

- hostname
- source archive basename, typically one of the archives in the _archives_ directory
- destination archive basename, usually in one of the formats _YYYYMMDD_, _YYDDMM.HH.MM_ or _YYDDMM.HH.MM-NN_.

Destination archives are copied with **pmlogcp**(1), the hostname is
changed with **pmlogrewrite**(1) and if the destination has one of
the datestamp formats, then **src/timeshift** is used to rewrite all
the timestamps in the archive relative to the date and time in the
archive's basename.

A part of the default configuration is as follows:

```bash
thishost        archives/foo+   20011005
thishost        archives/foo+   20011006.00.10
thishost        archives/foo+   20011007
otherhost       archives/ok-foo 20011002.00.10
otherhost       archives/ok-foo 20011002.00.10-00
```

<a id="mk.qahosts-script"></a>
## 11.2 <a id="idx+cmds+mk.qahosts"></a>**mk.qa\_hosts** script

The **mk.qa\_hosts** script includes heuristics for selecting
and sorting the list of potential remote PCP QA hosts in the
_qa\_hosts.primary_ file to produce the _qa\_hosts_ file used
by the [getpmcdhosts](#idx+cmds+getpmcdhosts) command.

Refer to the comments in _qa\_hosts.primary_,
and make appropriate changes.

The heuristics use the domain name for
the current host to choose a set of hosts that can be considered
when running distributed tests, e.g. **pmlogger**(1) locally and
**pmcd**(1) on a remote host. Anyone wishing to do this sort of
testing (it does not happen in the github CI and QA actions) will
need to figure out how to append control lines in the
_qa\_hosts.primary_ file.

**mk.qa\_hosts** is run from _GNUmakefile_ so once created, _qa\_hosts_
will tend to hang around.

<a id="qa-subdirectories"></a>
# 12 qa subdirectories

Below "qa" there are a number of important subdirectories.

<a id="src"></a>
## 12.1 _src_

The source for most of the QA applications live here along with
the executables that are run from the tests.

Adding a new QA application written in C involves these steps:

1. copy _template.c_ as the framework for the new QA application
2. edit the copy at will
3. update GNUlocaldefs with stanzas to match all the places **template** appears in this file
4. `$ make`
5. add the new executable to _src/.gitignore_

<a id="archives"></a>
## 12.2 _archives_

This directory contains stable PCP archives (in the git repository) that can
be used to provide deterministic PCP archives for tests to operate on.

<a id="badarchives"></a>
## 12.3 _badarchives_

Like _archives_ this directory provides
deterministic PCP archives for tests to operate on, but these ones are
all "damaged" in some way or other to allow error paths to be exercised.

<a id="tmparch"></a>
## 12.4 _tmparch_

This directory contains PCP archives that are created as required and
are used by tests checking the operation of **pmlogger**(1) and the
associated configurations and installed [PMDAs](#idx+pmda) on the local
host.

Once created, the archives are not automatically re-created; to force creation of
a new set of archives use:

```bash
$ ( cd tmparch; make clean setup )
```

<a id="pmdas"></a>
## 12.5 _pmdas_

This directory contains a number of subdirectories each implementing
one or more [PMDAs](#idx+pmda) that behave badly or exercise corner
cases.<br>

|**PMDA**|**Description**|
|---|---|
|bigun|Exports a metric with a big (1048576 byte) aggregate value.|
|broken|A family of PMDAs that are broken and exercise error paths and recovery for **libpcp\_pmda** and **pmcd**(1). See comments at the start of _pmdas/broken/broken.c_ for details.|
|dynamic|Exerciser for dynamic instance domains via metrics that can be updated using **pmStore**(1).|
|github-56|Reproducer for github issue #56 (memory leak in **libpcp\_pmda** with dynamic metrics).|
|memory\_python|Exerciser for memory corruption in the Python wrapper for **libpcp\_pmda**.|
|schizo|Two PMDAs with the same metrics but different metadata (simulates PMDA changes between versions).|
|slow|PMDA in Perl with configurable delays at start up and for metric fetching, to test **pmcd**(1) timeouts.|
|slow\_python|Python version of "slow".|
|test\_perl|Small sanity tester for a PMDA in Perl.|
|test\_python|Python version of "test\_perl".|
|whacko|Leverages the **trivial** PMDA to exercise integrity checking in the common shell functions used for PMDA **Install** and **Remove** scripts.|

<a id="admin"></a>
## 12.6 _admin_

A collection of scripts, mostly for use in the QA Farm in Melbourne,
so of limited general interest.

Exceptions are:

- **check-vm** - basic sanity checker to see if the local machine (or virtual machine) setup looks OK for running PCP QA
- **list-packages** - see the [Package lists](#package-lists) section below.
- **myconfigure** - run the top-level **configure** script just like it would be run from **Makepkgs** or a distribution package build for PCP
- <a id="idx+cmds+whatami"></a>**whatami** - report platform details (host name, PCP version, machine architecture, operating system and release, kernel version, Python version, SELinux state)

<a id="other-directories"></a>
# 13 Other directories

Other subdirectories below "qa" generally provide test scripts or test data used
to exercise specific PCP applications, PMDAs or libraries.  Some examples are listed below, but there
are many more like this.<br>

|**Directory**|**Description**|
|---|---|
|bpftrace|Python scripts to exercise the bpftrace PMDA.|
|ceph|JSON test files for exercising the JSON string manipulation routines in **libpcp\_web**.|
|cifs|**tar**(1) archives of _/proc/fs/cifs/_ files for various Linux kernel versions for testing the cifs PMDA.|
|cisco|Output from the "show interface" command to Cisco routers for lots of different interface types, used for testing the cisco PMDA.|
|collectl|Output from **collectl**(1) on various systems, used as input for **collectl2pcp**(1) testing.|
|denki|**tar**(1) archives of _/sys/_ files for various Linux kernel versions for testing the denki PMDA.|
|farm|Test data files to exercise the farm PMDA.|
|interact|Checklists for interactive QA of GUI applications (probably have not been used since the SGI days).|
|pdudata|Data files describing malformed PDUs to be parsed by the _src/pdu-gadget_ QA application sent to **pmcd**(1) to exercise error handling and robustness to DOS attack.|
|qt|C++ test programs to exercise **libpcp\_qmc** services.|
|views|Test views for **pmchart**(1) QA.|
|vllmbench|JSON input files for testing **vllmbench2pcp**(1).|

<a id="using-valgrind"></a>
# 14 Using valgrind

The **valgrind**(1) tool is very useful and we try to maximize the
**valgrind** code coverage for the components of PCP that are written in C
(all of the libraries and the key daemons and services **pmcd**(1),
**pmlogger**(1) and **pmproxy**(1)).

But **valgrind** is not 100% foolproof, so we need "suppressions"
once an issue has been triaged and classified as a problem with
**valgrind** or some other non-PCP code and not PCP _per se_.
This is done in one of several ways:

- The file _valgrind-suppress_ contains suppressions that have been observed across multiple versions of **valgrind** and 
these are used unconditionally when **valgrind** is run using the functions described below.
- The files _valgrind-suppress-&lt;version>_ are only used if the installed version of **valgrind** matches _&lt;version>_.
- Individual tests may include their own suppressions for issues local to that test.  See [**$grind\_extra**](#idx+vars+grindextra) below and test _1136_ for an example of how to use this.

A common practice is to develop a new test and then use [**new-grind**](#idx+cmds+new-grind)
to create a variant of this test to be run using **valgrind**.
In this case, the valgrind variant is a wrapper that runs
the non-valgrind variant, therefore the former provides a superset of the QA
coverage from the non-valgrind variant.
The default behaviour
is to run only the valgrind variant when **valgrind** is available,
else run the non-valgrind variant (this is achieved by some
additional logic inserted into the non-valgrind variant of the
test by **new-grind**).
If the shell variable <a id="idx+vars+pcpqavalgrind"></a>**$PCPQA_VALGRIND** is set
to **both** in the environment then the check is by-passed and
both variants of the test will be run when **valgrind** is available.
This is most useful when confirming that both 
_.out_ files are correct after a change to the the test.

**common.check** and **common.filter** include the following shell functions to assist when
using **valgrind**(1) in a QA test.<br>

|**Function**|**Description**|
|---|---|
|<a id="idx+funcs+checkvalgrind"></a>**\_check\_valgrind**|Usage: **\_check\_valgrind**<br>Check if **valgrind**(1) is installed and call [**\_notrun**](#idx+funcs+notrun) if not. Must be called after [**$seq**](#idx+vars+seq) is assigned and before calling [**\_check\_helgrind**](#idx+funcs+checkhelgrind), or [**\_run\_valgrind**](#idx+funcs+runvalgrind), or [**\_run\_helgrind**](#idx+funcs+runhelgrind), or using [**$valgrind\_clean\_assert**](#idx+vars+valgrindcleanassert).|
|<a id="idx+funcs+filtervalgrind"></a>**\_filter\_valgrind**|Usage: **\_filter\_valgrind**<br>Remove the fluff from **valgrind**(1)'s reporting and leave behind the summary lines.|
|<a id="idx+funcs+prefervalgrind"></a>**\_prefer\_valgrind**|Usage: **\_prefer\_valgrind**<br>If **valgrind**(1) is not installed, or we're running on a platform where **valgrind** is known to be broken, or the shell variable <a id="idx+vars+pcpqaprefervalgrind"></a>**$PCPQA\_PREFER\_VALGRIND** has the value **no** then return **1** to indicate we'd prefer to run a non-valgrind variant of a test if possible. Otherwise return **0** to indicate we'd prefer to run a valgrind variant of a test.<br>This has been replaced to some extent by the valgrind support that is in the skeletal test generated by the [**new**](#the-new-script) script and the "dual" variant non-valgrind and valgrind pairing of tests supported by the [**new-grind**](#idx+cmds+new-grind) script.|
|<a id="idx+funcs+runvalgrind"></a>**\_run\_valgrind**|Usage: **\_run\_valgrind** \[**--sudo**] \[**--save-output**] _app_ \[_arg_ ...]<br>Run the application _app_ with the optional arguments _arg_ under the control of **valgrind**.<br>Use **--sudo** to execute **valgrind** with [**$sudo**](#idx+vars+sudo).<br>Normally **\_run\_valgrind** will take care of separating and reporting stdout and stderr, but with the **--save-output** option these are saved in **$tmp**_.out_ and **$tmp**_.err_ respectively.<br>If set, <a id="idx+vars+grindextra"></a>**$grind\_extra** may be used to pass additional arguments to **valgrind**.<br>The full report from **valgrind** itself is appended to **$seq\_full** and this same information is output after filtering with [**\_filter\_valgrind**](#idx+funcs+filtervalgrind).|
|<a id="idx+funcs+filtervalgrindpossibly"></a>**\_filter\_valgrind\_possibly**|Usage: **\_filter\_valgrind\_possibly**<br>Strip "blocks are possibly lost in loss record" information from **valgrind** report.|
|<a id="idx+vars+valgrindcleanassert"></a>**$valgrind\_clean\_assert**|A shell variable, not a function, but provides similar functionality to [**\_run\_valgrind**](#idx+funcs+runvalgrind).<br>Typical usage would be:<br>`_check_valgrind`<br>`...`<br>`$valgrind_clean_assert app args ...`|

<a id="using-helgrind"></a>
# 15 Using helgrind

There is also some use of **helgrind** (a "tool" that is part of
**valgrind**(1)) especially for tests that exercise thread safety in
**libpcp**.

Suppressions are handled in a similar manner to
**valgrind**, namely the file _helgrind-suppress_, the files
_helgrind-suppress-&lt;version>_ (although there are none of these
in the git tree as of the time of writing this document) and the
(overloaded) use of [**$grind\_extra**](#idx+vars+grindextra) in
individual tests.<br>

**common.check** includes the following shell functions to assist when
using **helgrind**(1) in a QA test.<br>

|**Function**|**Description**|
|---|---|
|<a id="idx+funcs+checkhelgrind"></a>**\_check\_helgrind**|Usage: **\_check\_helgrind**<br>Check if **helgrind** is known to be flakey on this platform and call [**\_notrun**](#idx+funcs+notrun) if so. Must be called after [**\_check\_valgrind**](#idx+funcs+checkvalgrind) has been called.|
|<a id="idx+funcs+filterhelgrind"></a>**\_filter\_helgrind**|Usage: **\_filter\_helgrind**<br>Remove the fluff from **helgrind**'s reporting and leave behind the summary lines.|
|<a id="idx+funcs+runhelgrind"></a>**\_run\_helgrind**|Usage: **\_run\_helgrind** \[**--sudo**] _app_ \[_arg_ ...]<br>Run the application _app_ with the optional arguments _arg_ under the control of **helgrind**.<br>Use **--sudo** to execute **helgrind** with [**$sudo**](#idx+vars+sudo).<br>If set, [**$grind\_extra**](#idx+vars+grindextra) may be used to pass additional arguments to **helgrind**.<br>The full report from **helgrind** itself is appended to **$seq\_full** and this same information is output after filtering with [**\_filter\_helgrind**](#idx+funcs+filterhelgrind).|

<a id="common-and-common.-files"></a>
# 16 <a id="idx+files+common"></a>_common_ and <a id="idx+files+common.star"></a>_common.\*_ files

Some of these files are automatically included by the skeletal test
that the [**new**](#the-new-script) script generates, specifically
_common.check_, _common.filter_, _common.product_, _common.rc_ and
_common.setup_.

Others contain useful shell functions for exercising specific areas
of functionality.  Their focus tends to be narrow and outside the
scope of this document.  Refer to the comments in these files for more
information.

<a id="selinux-considerations"></a>
# 17 SELinux considerations

SELinux has a long and gory history with PCP QA.  If you're using SELinux
then these are the suggested steps to resolve problems:

- if SELinux is **Enforcing**, change it to **Permissive**<br>`$ sudo setenforce Permissive`<br>and rerun the failing test.  If it passes, then it is unlikely to be a PCP QA issue.
- enable the default [**check.callback**](#check.callback-script) script and rerun the failing test.  Look to **$seq**_.out.bad for AVCs triggered by the failing test.

If the problem is a missing SELinux policy then you'll need to decide
if it is a generic PCP issue in which case the "fix" will need to be
applied in _../src/selinux_ (see the _README_ file there for more
information), or the issue relates just to the QA test in which
case the "fix" will need to be applied to _pcp-testsuite.te_ (the
_../src/selinux/README_ file will also provide guidance here).

<a id="package-lists"></a>
# 18 Package lists

Building _all_ of the components of PCP requires a **lot** of packages
to be installed.  Running _all_ of PCP QA requires **even more**
packages to be installed.

The required packages depend on the platform, the operating system
(type and version) and the phase of the moon.

The <a id="idx+cmds+list-packages"></a>**admin/list-packages** script helps here.

Usage: **admin/list-packages** \[**-A** _arch_] \[**-D** _distro_] \[**-c**] \[**-i** _tag_] \[**-m**] \[**-n**] \[**-v**] \[**-V** _version_] \[**-x** _tag_]

The current platform is identified by an architecture, a distro and
a (distro) version.  The defaults come from a slice-n-dice of the
output from the [admin/whatami](#idx+cmds+whatami) command, but may be
over-ridden using the **-A**, **-D** or **-V** options.

The current platform identifies a file in the _admin/package-lists_
directory using some "fuzzy" matching where **any** matches any _arch_
and _version_ matches any distro version for which it is a prefix.
The **-n** option simply prints the matching packing-list file, which
is most useful in:

```bash
$ vi $(admin/list-packages -n)
```

The _admin/package-lists_ files are named _distro_**+**_version_**+**_arch_ and have a simple format:

- lines beginning with a hash (#) are comments
- one line per package with the package name optionally followed by one or more "tags" (e.g. **cpan**, **pip3**, **not4ci**, ...)
- the package name may include shell commands escaped by backticks, e.g. `linux-headers-\`uname -r\``

The **-m** (missing) option lists packages in the _packing-list_ file that are **not** currently installed.

The **-i** (include) or **-x** (exclude) options may be used to refine the set of packages checked, based on the _tag_.

The **-c** (check) checks the _packing-list_ file for consistency with the available packages.

If there is no matching _packing-list_ file this is treated as an error.
But a new one can be created using this recipe:

```bash
$ cd admin/package-lists
$ ./new
```

The "check" and "create a new" operations use magic from the Dark Ages that has been salted away in
control files in the _admin/other-packages_ directory.  Explaining that would take too long, but for
anyone with an interest in archeology, check out _admin/other-packages/manifest_.

<a id="the-last-word"></a>
# 19 The last word

If you find something that does not work or seems "not quite right"&trade;, then either
send email to  <a href="mailto:pcp@groups.io">pcp@groups.io</a>
or join the Performance Co-Pilot chat at
[www.performancecopilot.slack.com](https://www.performancecopilot.slack.com/)
and post to the **#pcpqa** channel.

Better still, if you can fix the problem or create
additional QA tests please commit these to **git** and open
a Pull Request at
[https://github.com/performancecopilot/pcp](https://github.com/performancecopilot/pcp)

Happy testing and cheers,<br>
Ken McDonell<br>

<a id="appendix-initial-setup"></a>
# Appendix: Initial setup

<a id="sudo-setup"></a>
## **sudo** setup

The PCP tests are designed to be run by a non-root user. Where "root"
privileges are needed, e.g. to stop or start **pmcd**(1), or run a PMDA's **Install**
or **Remove** scripts, etc. the **sudo**(1) application is used. When
using **sudo** for QA, your current user login needs to be able to
execute commands as root without being prompted for a password. This
can be achieved by adding the following line to the _/etc/sudoers_
file:

```
<your login>   ALL=(ALL) NOPASSWD: ALL
```
and checked with the command and expected result below

```bash
$ sudo id
uid=0(root) gid=0(root) groups=0(root)
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

Refer to the [**mk.qa_hosts** script](#mk.qahosts-script) section
to see how the remote QA hosts are configured.

For each of the potential remote PCP QA hosts, the following must be
set up:

1. PCP installed from packages,
2. **pmcd**(1) enabled and running,
3. a login for the user **pcpqa** needs to be created, and then set up in such a way that **ssh**(1) and **scp**(1) will work without the need for any password, i.e. these sorts of commands<br>`$ ssh pcpqa@pcp-qa-host some-command`<br>`$ scp some-file pcpqa@pcp-qa-host:some-dir`<br>must work correctly when run from the local host with no interactive input and no Password: prompt<br><br>On SELinux systems it may be necessary to execute the following command to make this work:<br>`$ sudo chcon -R unconfined_u:object_r:ssh_home_t:s0 ~pcpqa/.ssh`<br>so that the ssh\_home\_t attribute is set on ~pcpqa/.ssh and all the files below there.<br><br>The **pcpqa** user's environment must also be initialized so that their shell's path includes all of the PCP binary directories (identify these with `$ grep BIN /etc/pcp.conf`), so that all PCP commands are executable without full pathnames.  Of most concern would be auxiliary directory (usually _/usr/lib/pcp/bin_, _/usr/share/pcp/bin_ or _/usr/libexec/pcp/bin_) where commands like **pmlogger\_daily**(1), **pmnsadd**(1), **newhelp**(1), **mkaf**(1) etc.) are installed.<br><br>And finally, the **pcpqa** user needs to be included in the group **pcp** in _/etc/group_.

Once you've setup the remote PCP QA hosts and modified _common.config_
and _qa\_hosts.primary_ locally, then validate the setup using
[check-setup](#check-setup):

```bash
$ ./check-setup
```

<a id="firewall-setup"></a>
## Firewall setup

Network firewalls can get in the way, especially if you're attempting
any [distributed QA](#distributed-qa).

In addition to the standard **pmcd**(1) and **pmproxy**(1) port(s) (TCP ports 44321,
44322 and 44323) one needs to open ports to allow incoming connections
and outgoing connections on a range of ports for **pmdatrace**(1),
**pmlogger**(1) connections via **pmlc**(1), and some QA tests.
Opening the TCP range 4320 to 4350 (inclusive) should suffice for these
additional ones.

<a id="common.config-file"></a>
## <a id="idx+files+common.config"></a>_common.config_ file

This script uses heuristics to set a number of
interesting variables, specifically:<br>

|**Shell Variable**|**Description**|
|---|---|
|<a id="idx+vars+pcpqaclosexserver"></a>**$PCPQA\_CLOSE\_X\_SERVER**|The **$DISPLAY** setting for an X server that is willing to accept connections from X clients running on the local machine. This is optional, and if not set any QA tests dependent on this will be skipped. See also the [**\_check\_display**](#idx+funcs+checkdisplay) shell function.|
|<a id="idx+vars+pcpqadesktophack"></a>**$PCPQA\_DESKTOP\_HACK**|Set to **true** to enable a workaround for babble from Qt applications on some platforms, see the [**\_check\_display**](#idx+funcs+checkdisplay) and [**\_clean\_display**](#idx+funcs+cleandisplay) shell functions.|
|<a id="idx+vars+pcpqafarpmcd"></a>**$PCPQA\_FAR\_PMCD**|The hostname for a host running **pmcd**(1), but the host is preferably a long way away (in terms of TCP/IP latency) for timing tests. This is optional, and if not set any QA tests dependent on this will be skipped.|
|<a id="idx+vars+pcpqahyphenhost"></a>**$PCPQA\_HYPHEN\_HOST**|The hostname for a host running **pmcd**(1), with a hyphen (-) in the hostname. This is optional, and if not set any QA tests dependent on this will be skipped.|

If relevant, edit this file to provide suitable settings for the local environment.

<a id="some-special-test-cases"></a>
## Some special test cases

For test 051 we need five local hostnames that are valid, although PCP
does not need to be installed there, nor **pmcd**(1) running.  The five
hosts listed in 051.hosts (the comments at the start of this file
explain what is required) should suffice for most installations.

Some tests are graphical, and wish to make use of your display.
For authentication to succeed, you may need to perform some access list
updates for such tests to pass (e.g. test 325), e.g.

```bash
$ xhost +local:
```

<a id="take-it-for-a-test-drive"></a>
## Take it for a test drive

You can now verify your QA setup, by running:

```bash
$ ./check 000
```

The first time you run [**check**](#check-script)  it will descend into
the [_src_](#src) directory and make all of the QA test programs and
then descend into the [_tmparch_](#tmparch) directory and recreate the
transient PCP archives, so some patience may be required.

If test 000 fails, it may be that you have locally developed PMDAs
or optional PMDAs installed.  Edit _common.filter_ and modify the
[**\_filter\_top\_pmns**](#idx+funcs+filtertoppmns) function to strip
the top-level name components for any new metric names (there are
lots of examples already there) ... if these are distributed (shipped)
PMDAs, please update the list in _common.filter_ and commit the changes
to **git**.

<a id="appendix-pcp-acronyms"></a>
# Appendix: PCP acronyms

|**Acronym**|**Description**|
|---|---|
|<a id="idx+pcp"></a>**PCP**|**P**erformance **C**o-**P**ilot|
|<a id="idx+pmapi"></a>**PMAPI**|**P**erformance **M**etrics **A**pplication **I**nterface: the public interfaces supported by **libpcp**|
|<a id="idx+pmcd"></a>**PMCD**|**P**erformance **M**etrics **C**ollection **D**aemon: aka **pmcd**(1), the source of all performance metric metadata and data on the local host, although the real work is delegated to the PMDAs|
|<a id="idx+pmda"></a>**PMDA**|**P**erformance **M**etrics **D**omain **A**gent: a "plugin" for **pmcd**(1) that is responsible for an independent subset of the available performance metrics|
|<a id="idx+pmid"></a>**PMID**|**P**erformance **M**etric **ID**entifier: unique internal identifier for each performance metric, usually represented as a triple comprising a _domain_ number, a _cluster_ number and an _ordinal_ number|
|<a id="idx+pmns"></a>**PMNS**|**P**erformance **M**etrics **N**ame **S**pace: all of the metric names in a PCP archive or known to **pmcd**(1)|

<!--

.\" control lines for scripts/man-spell -- need to fake troff comment here
.\" -- shell functions
.\" +ok+ _all_hostnames _all_ipaddrs _arch_start _avail_metric
.\" +ok+ _change_config _check_64bit_platform _check_agent _check_core
.\" +ok+ _check_display _check_freespace _check_helgrind _check_job_scheduler
.\" +ok+ _check_key_server _check_key_server_ping _check_key_server_version
.\" +ok+ _check_key_server_version_offline _check_local_primary_archive
.\" +ok+ _check_metric _check_search _check_series _check_valgrind
.\" +ok+ _clean_display _cleanup_pmda _cull_dup_lines _disable_loggers
.\" +ok+ _domain_name _exit _fail _filesize _filterall_pcp_start
.\" +ok+ _filter_compiler_babble _filter_console _filter_cron_scripts
.\" +ok+ _filter_dbg _filter_dumpresult _filter_helgrind _filter_init_distro
.\" +ok+ _filter_ls _filter_optional_labels _filter_optional_pmda_instances
.\" +ok+ _filter_optional_pmdas _filter_pcp_restart _filter_pcp_start
.\" +ok+ _filter_pcp_start_distro _filter_pcp_stop _filter_pmcd_log
.\" +ok+ _filter_pmda_install _filter_pmda_remove _filter_pmdumplog
.\" +ok+ _filter_pmdumptext _filter_pmie_log _filter_pmie_start
.\" +ok+ _filter_pmie_stop _filter_pmproxy_log _filter_pmproxy_start
.\" +ok+ _filter_pmproxy_stop _filter_post _filter_slow_pmie _filter_top_pmns
.\" +ok+ _filter_torture_api _filter_valgrind _filter_valgrind_possibly
.\" +ok+ _filter_views _find_free_port _find_key_server_modules
.\" +ok+ _find_key_server_name _find_key_server_search _get_config _get_endian
.\" +ok+ _get_fqdn _get_libpcp_config _get_port _get_primary_logger_pid
.\" +ok+ _get_word_size _host_to_fqdn _host_to_ipaddr
.\" +ok+ _host_to_ipv {_host_to_ipv6addrs}
.\" +ok+ _instances_filter_any _instances_filter_exact _instances_filter_nonzero
.\" +ok+ _inst_value_filter _ipaddr_to_host
.\" +ok+ _ipv _localhost {_ipv6_localhost} _libvirt_is_ok
.\" +ok+ _machine_id _make_helptext _make_proc_stat _need_metric _notrun
.\" +ok+ _path_readable _pid_in_container _prefer_valgrind _prepare_pmda
.\" +ok+ _prepare_pmda_install _prepare_pmda_mmv _private_pmcd
.\" +ok+ _ps_tcp_port _pstree_all _pstree_oneline _quote_filter
.\" +ok+ _remove_job_scheduler _restore_config _restore_job_scheduler
.\" +ok+ _restore_loggers _restore_pmda_install _restore_pmda_mmv
.\" +ok+ _restore_pmlogger_control _restore_primary_logger _run_helgrind
.\" +ok+ _run_valgrind _save_config _service _set_dsosuffix _show_pmie_errors
.\" +ok+ _show_pmie_exit _sighup_pmcd _sort_pmdumplog_d _start_up_pmlogger
.\" +ok+ _stop_auto_restart _systemctl_status _triage_pmcd _triage_wait_point
.\" +ok+ _try_pmlc _value_filter_any _value_filter_nonzero _wait_for_pmcd
.\" +ok+ _wait_for_pmcd_stop _wait_for_pmie _wait_for_pmlogger _wait_for_pmproxy
.\" +ok+ _wait_for_pmproxy_logfile _wait_for_pmproxy_metrics _wait_for_port
.\" +ok+ _wait_pmcd_end _wait_pmie_end _wait_pmlogctl _wait_pmlogger_end
.\" +ok+ _wait_pmproxy_end _wait_process_end _webapi_header_filter
.\" +ok+ _webapi_response_filter _within_tolerance _writable_primary_logger
.\" +ok+ _restore_auto_restart _restore_pmda _restore_primary_logger
.\" +ok+ _stop_auto_start _check_big_file
.\"
.\" -- scripts
.\" +ok+ all-by-group appchange bad-by-group check.app.ok check-auto
.\" +ok+ flakey {check-flakey} {check-group}
.\" +ok+ pdu {check-pdu-coverage} setup {check-setup} vars {check-vars}
.\" +ok+ config {cull-pmlogger-config} {daily-cleanup}
.\" +ok+ app {find-app} {find-bound} {find-metric}
.\" +ok+ flakey {flakey-summary} getpmcdhosts {grind} {grind-pmda} {group-stats}
.\" +ok+ mk localconfig {mk.localconfig} logfarm {mk.logfarm}
.\" +ok+ qa_hosts qahosts {mk.qa_hosts} {mk.variant}
.\" +ok+ new dup {new-dup} {new-grind} seqs {new-seqs}
.\" +ok+ {really-retire} recheck remake sameas var {var-use} whatami
.\" +ok+ Makepkgs
.\"
.\" -- shell variables
.\" +ok+ PCPQA_CLOSE_X_SERVER PCPQA_DESKTOP_HACK PCPQA_FAR_PMCD PCPQA_HYPHEN_HOST
.\" +ok+ PCPQA_IN_CI PCPQA_PREFER_VALGRIND PCPQA_VALGRIND PCPQA_SYSTEMD
.\" +ok+ PCP_AWK_PROG PCP_VER
.\" +ok+ DSOSUFFIX grind_extra XDG_RUNTIME_DIR
.\" +ok+ seq seq_full tmp TMPDIR valgrind_clean_assert
.\" +ok+ pmcd_args pmcd_pid pmcd_port
.\"
.\" -- options and arguments
.\" +ok+ arg args baseport dir distro lowport highport ipaddr logfile
.\" +ok+ maxdelay maxtol mintol nbo ncpu onoff precheck {--precheck}
.\" +ok+ proto rootdir selinux {../src/selinux} seqno TT
.\"
.\" -- files and directories
.\" +ok+ appcache badarchives bigun ceph cifs cisco collectl denki ganglia
.\" +ok+ gfs {gfs2} gluster gpfs guidellm hacluster haproxy hdb infiniband
.\" +ok+ lio lustre mic mmv named nfsclient openmetrics opentelemetry
.\" +ok+ paulsmith pconf pdudata perfevent postfix qt rocestat sadist
.\" +ok+ slurm statsd tls.conf tmparch unbound vllmbench 
.\" +ok+ gitignore triaged src GNUlocaldefs
.\" +ok+ memory_python schizo slow_python test_perl test_python whacko
.\" +ok+ timeshift {src/timeshift} torture_api {src/torture_api}
.\" +ok+ xdg {$tmp/xdg-runtime} sys {/sys} fs {/proc/fs/cifs/}
.\"
.\" -- system commands and libraries
.\" +ok+ bpftrace cd chcon helgrind helgrind's journalctl perl pmdumplog
.\" +ok+ mkdir mmap'd pstree redisearch valgrind valkey valkeysearch uniq
.\" +ok+ okular retext
.\" +ok+ libc libexec libvirt linux xdpyinfo xhost scp sudoers libpcpqmc
.\" +ok+ libpcpqmc {libpcp_qmc} libpcpweb {libpcp_web} libpcp_qmc
.\"
.\" -- others
.\" +ok+ \$seq\*.full {**$seq**_.full_} McDonell SGI md {COOKBOOK.md}
.\" +ok+ addrs {ipv6addrs} aka CLI {CLI application}
.\" +ok+ _check_ bit_platform {from _check_64bit_platform}
.\" +ok+ br {from <br>} cd's cgroup {from /proc/pid/cgroup}
.\" +ok+ _cleanup {_cleanup function} cli {-cli suffix} cmd {example}
.\" +ok+ cpu cpuN {example} Ctrl {Ctrl-C} datestamp dinking Distro {Distro-specific}
.\" +ok+ endian favoured github localdomain checkvalgrind {_check_valgrind example}
.\" +ok+ IDentifier {P..M..**ID**entifier} IPv {IPv4} NOPASSWD {sudo setup}
.\" +ok+ pmDumpResult SELinux zeroconf rc {"rc" script} SHA
.\" +ok+ _filter {example | _filter} FQDN HH {hour} SSS {secs} le be {endian}
.\" +ok+ href mailto nbsp lt {html} idxctl {index control} ip
.\" +ok+ localmachine {default machine id} logpush sampledso
.\" +ok+ not_in_ci {packing list tag} notrun {check outcome}
.\" +ok+ numclient {pmcd.numclient} pre {pre-dates}
.\" +ok+ objectr {AVC object_r} ssh_home_t sshhomet {AVC}
.\" +ok+ unconfinedu {AVC} te testsuite {pcp-testsuite.te}
.\" +ok+ thishost otherhost {example} pcpqa {login} portmap {pmlogger}
.\" +ok+ tcp udp Sssh {-s mode} subtests VM VMs wallclock __ {prefix}
.\" +ok+ freebsd netbsd openbsd darwin macOS solaris PCP_PLATFORM
.\" +ok+ privatepmcd {`$sudo _private_pmcd`}

-->

<!--idxctl
General Index|Commands and Scripts|Shell Functions|Shell Variables|Files
!|cmds|funcs|vars|files
-->
<a id="index"></a>
# Index

|**General Index**|**Shell Functions ...**|**Shell Functions ...**|**Shell Functions ...**|
|---|---|---|---|
|[PCP](#idx+pcp)|[\_check\_job\_scheduler](#idx+funcs+checkjobscheduler)|[\_get\_fqdn](#idx+funcs+getfqdn)|[\_wait\_for\_pmcd\_stop](#idx+funcs+waitforpmcdstop)|
|[PMAPI](#idx+pmapi)|[\_check\_key\_server](#idx+funcs+checkkeyserver)|[\_get\_libpcp\_config](#idx+funcs+getlibpcpconfig)|[\_wait\_for\_pmie](#idx+funcs+waitforpmie)|
|[PMCD](#idx+pmcd)|[\_check\_key\_server\_ping](#idx+funcs+checkkeyserverping)|[\_get\_port](#idx+funcs+getport)|[\_wait\_for\_pmlogger](#idx+funcs+waitforpmlogger)|
|[PMDA](#idx+pmda)|[\_check\_key\_server\_version](#idx+funcs+checkkeyserverversion)|[\_get\_primary\_logger\_pid](#idx+funcs+getprimaryloggerpid)|[\_wait\_for\_pmproxy](#idx+funcs+waitforpmproxy)|
|[PMID](#idx+pmid)|[\_check\_key\_server\_version\_offline](#idx+funcs+checkkeyserverversionoffline)|[\_get\_word\_size](#idx+funcs+getwordsize)|[\_wait\_for\_pmproxy\_logfile](#idx+funcs+waitforpmproxylogfile)|
|[PMNS](#idx+pmns)|[\_check\_local\_primary\_archive](#idx+funcs+checklocalprimaryarchive)|[\_host\_to\_fqdn](#idx+funcs+hosttofqdn)|[\_wait\_for\_pmproxy\_metrics](#idx+funcs+waitforpmproxymetrics)|
|**Commands and Scripts**|[\_check\_metric](#idx+funcs+checkmetric)|[\_host\_to\_ipaddr](#idx+funcs+hosttoipaddr)|[\_wait\_for\_port](#idx+funcs+waitforport)|
|[admin/list-packages](#idx+cmds+list-packages)|[\_check\_search](#idx+funcs+checksearch)|[\_host\_to\_ipv6addrs](#idx+funcs+hosttoipv6addrs)|[\_wait\_pmcd\_end](#idx+funcs+waitpmcdend)|
|[all-by-group](#idx+cmds+all-by-group)|[\_check\_series](#idx+funcs+checkseries)|[\_instances\_filter\_any](#idx+funcs+instancesfilterany)|[\_wait\_pmie\_end](#idx+funcs+waitpmieend)|
|[appchange](#idx+cmds+appchange)|[\_check\_valgrind](#idx+funcs+checkvalgrind)|[\_instances\_filter\_exact](#idx+funcs+instancesfilterexact)|[\_wait\_pmlogctl](#idx+funcs+waitpmlogctl)|
|[bad-by-group](#idx+cmds+bad-by-group)|[\_clean\_display](#idx+funcs+cleandisplay)|[\_instances\_filter\_nonzero](#idx+funcs+instancesfilternonzero)|[\_wait\_pmlogger\_end](#idx+funcs+waitpmloggerend)|
|[check](#idx+cmds+check)|[\_cleanup\_pmda](#idx+funcs+cleanuppmda)|[\_inst\_value\_filter](#idx+funcs+instvaluefilter)|[\_wait\_pmproxy\_end](#idx+funcs+waitpmproxyend)|
|[check.app.ok](#idx+cmds+check.app.ok)|[\_cull\_dup\_lines](#idx+funcs+cullduplines)|[\_ipaddr\_to\_host](#idx+funcs+ipaddrtohost)|[\_wait\_process\_end](#idx+funcs+waitprocessend)|
|[check-auto](#idx+cmds+check-auto)|[\_disable\_loggers](#idx+funcs+disableloggers)|[\_ipv6\_localhost](#idx+funcs+ipv6localhost)|[\_webapi\_header\_filter](#idx+funcs+webapiheaderfilter)|
|[check-flakey](#idx+cmds+check-flakey)|[\_domain\_name](#idx+funcs+domainname)|[\_libvirt\_is\_ok](#idx+funcs+libvirtisok)|[\_webapi\_response\_filter](#idx+funcs+webapiresponsefilter)|
|[check-group](#idx+cmds+check-group)|[\_exit](#idx+funcs+exit)|[\_machine\_id](#idx+funcs+machineid)|[\_within\_tolerance](#idx+funcs+withintolerance)|
|[check-pdu-coverage](#idx+cmds+check-pdu-coverage)|[\_fail](#idx+funcs+fail)|[\_make\_helptext](#idx+funcs+makehelptext)|[\_writable\_primary\_logger](#idx+funcs+writableprimarylogger)|
|[check-setup](#idx+cmds+check-setup)|[\_filesize](#idx+funcs+filesize)|[\_make\_proc\_stat](#idx+funcs+makeprocstat)|**Shell Variables**|
|[check-vars](#idx+cmds+check-vars)|[\_filterall\_pcp\_start](#idx+funcs+filterallpcpstart)|[\_need\_metric](#idx+funcs+needmetric)|[$DSOSUFFIX](#idx+vars+dsosuffix)|
|[cull-pmlogger-config](#idx+cmds+cull-pmlogger-config)|[\_filter\_compiler\_babble](#idx+funcs+filtercompilerbabble)|[\_notrun](#idx+funcs+notrun)|[$grind\_extra](#idx+vars+grindextra)|
|[daily-cleanup](#idx+cmds+daily-cleanup)|[\_filter\_console](#idx+funcs+filterconsole)|[\_path\_readable](#idx+funcs+pathreadable)|[$here](#idx+vars+here)|
|[find-app](#idx+cmds+find-app)|[\_filter\_cron\_scripts](#idx+funcs+filtercronscripts)|[\_pid\_in\_container](#idx+funcs+pidincontainer)|[$PCP\_\*](#idx+vars+pcpstar)|
|[find-archive](#idx+cmds+find-archive)|[\_filter\_dbg](#idx+funcs+filterdbg)|[\_prefer\_valgrind](#idx+funcs+prefervalgrind)|[$PCP\_PLATFORM](#idx+vars+pcpplatform)|
|[find-bound](#idx+cmds+find-bound)|[\_filter\_dumpresult](#idx+funcs+filterdumpresult)|[\_prepare\_pmda](#idx+funcs+preparepmda)|[$PCPQA\_CLOSE\_X\_SERVER](#idx+vars+pcpqaclosexserver)|
|[find-metric](#idx+cmds+find-metric)|[\_filter\_helgrind](#idx+funcs+filterhelgrind)|[\_prepare\_pmda\_install](#idx+funcs+preparepmdainstall)|[$PCPQA\_DESKTOP\_HACK](#idx+vars+pcpqadesktophack)|
|[flakey-summary](#idx+cmds+flakey-summary)|[\_filter\_init\_distro](#idx+funcs+filterinitdistro)|[\_prepare\_pmda\_mmv](#idx+funcs+preparepmdammv)|[$PCPQA\_FAR\_PMCD](#idx+vars+pcpqafarpmcd)|
|[getpmcdhosts](#idx+cmds+getpmcdhosts)|[\_filter\_ls](#idx+funcs+filterls)|[\_private\_pmcd](#idx+funcs+privatepmcd)|[$PCPQA\_HYPHEN\_HOST](#idx+vars+pcpqahyphenhost)|
|[grind](#idx+cmds+grind)|[\_filter\_optional\_labels](#idx+funcs+filteroptionallabels)|[\_ps\_tcp\_port](#idx+funcs+pstcpport)|[$PCPQA\_IN\_CI](#idx+vars+pcpqainci)|
|[grind-pmda](#idx+cmds+grind-pmda)|[\_filter\_optional\_pmda\_instances](#idx+funcs+filteroptionalpmdainstances)|[\_pstree\_all](#idx+funcs+pstreeall)|[$PCPQA\_PREFER\_VALGRIND](#idx+vars+pcpqaprefervalgrind)|
|[group-stats](#idx+cmds+group-stats)|[\_filter\_optional\_pmdas](#idx+funcs+filteroptionalpmdas)|[\_pstree\_oneline](#idx+funcs+pstreeoneline)|[$PCPQA\_SYSTEMD](#idx+vars+pcpqasystemd)|
|[mk.localconfig](#idx+cmds+mk.localconfig)|[\_filter\_pcp\_restart](#idx+funcs+filterpcprestart)|[\_quote\_filter](#idx+funcs+quotefilter)|[$PCPQA_VALGRIND](#idx+vars+pcpqavalgrind)|
|[mk.logfarm](#idx+cmds+mk.logfarm)|[\_filter\_pcp\_start](#idx+funcs+filterpcpstart)|[\_remove\_job\_scheduler](#idx+funcs+removejobscheduler)|[$PCP\_VER](#idx+vars+pcpver)|
|[mk.qa\_hosts](#idx+cmds+mk.qahosts)|[\_filter\_pcp\_start\_distro](#idx+funcs+filterpcpstartdistro)|[\_restore\_auto\_restart](#idx+funcs+restoreautorestart)|[$pid](#idx+vars+pid)|
|[mk.variant](#idx+cmds+mk.variant)|[\_filter\_pcp\_stop](#idx+funcs+filterpcpstop)|[\_restore\_config](#idx+funcs+restoreconfig)|[$pmcd\_args](#idx+vars+pmcdargs)|
|[new](#idx+cmds+new)|[\_filter\_pmcd\_log](#idx+funcs+filterpmcdlog)|[\_restore\_job\_scheduler](#idx+funcs+restorejobscheduler)|[$pmcd\_pid](#idx+vars+pmcdpid)|
|[new-dup](#idx+cmds+new-dup)|[\_filter\_pmda\_install](#idx+funcs+filterpmdainstall)|[\_restore\_loggers](#idx+funcs+restoreloggers)|[$pmcd\_port](#idx+vars+pmcdport)|
|[new-grind](#idx+cmds+new-grind)|[\_filter\_pmda\_remove](#idx+funcs+filterpmdaremove)|[\_restore\_pmda\_install](#idx+funcs+restorepmdainstall)|[$seq](#idx+vars+seq)|
|[new-seqs](#idx+cmds+new-seqs)|[\_filter\_pmdumplog](#idx+funcs+filterpmdumplog)|[\_restore\_pmda\_mmv](#idx+funcs+restorepmdammv)|[$seq\_full](#idx+vars+seqfull)|
|[really-retire](#idx+cmds+really-retire)|[\_filter\_pmdumptext](#idx+funcs+filterpmdumptext)|[\_restore\_pmlogger\_control](#idx+funcs+restorepmloggercontrol)|[$status](#idx+vars+status)|
|[recheck](#idx+cmds+recheck)|[\_filter\_pmie\_log](#idx+funcs+filterpmielog)|[\_restore\_primary\_logger](#idx+funcs+restoreprimarylogger)|[$sudo](#idx+vars+sudo)|
|[remake](#idx+cmds+remake)|[\_filter\_pmie\_start](#idx+funcs+filterpmiestart)|[\_run\_helgrind](#idx+funcs+runhelgrind)|[$tmp](#idx+vars+tmp)|
|[sameas](#idx+cmds+sameas)|[\_filter\_pmie\_stop](#idx+funcs+filterpmiestop)|[\_run\_valgrind](#idx+funcs+runvalgrind)|[$valgrind\_clean\_assert](#idx+vars+valgrindcleanassert)|
|[show-me](#idx+cmds+show-me)|[\_filter\_pmproxy\_log](#idx+funcs+filterpmproxylog)|[\_save\_config](#idx+funcs+saveconfig)|**Files**|
|[var-use](#idx+cmds+var-use)|[\_filter\_pmproxy\_start](#idx+funcs+filterpmproxystart)|[\_service](#idx+funcs+service)|[$seq\_full](#idx+files+seqfull)|
|[whatami](#idx+cmds+whatami)|[\_filter\_pmproxy\_stop](#idx+funcs+filterpmproxystop)|[\_set\_dsosuffix](#idx+funcs+setdsosuffix)|[_check.log_](#idx+files+check.log)|
|**Shell Functions**|[\_filter\_post](#idx+funcs+filterpost)|[\_show\_pmie\_errors](#idx+funcs+showpmieerrors)|[_check.time_](#idx+files+check.time)|
|[\_all\_hostnames](#idx+funcs+allhostnames)|[\_filter\_slow\_pmie](#idx+funcs+filterslowpmie)|[\_show\_pmie\_exit](#idx+funcs+showpmieexit)|[_common.\*_](#idx+files+common.star)|
|[\_all\_ipaddrs](#idx+funcs+allipaddrs)|[\_filter\_top\_pmns](#idx+funcs+filtertoppmns)|[\_sighup\_pmcd](#idx+funcs+sighuppmcd)|[_common_](#idx+files+common)|
|[\_arch\_start](#idx+funcs+archstart)|[\_filter\_torture\_api](#idx+funcs+filtertortureapi)|[\_sort\_pmdumplog\_d](#idx+funcs+sortpmdumplogd)|[_common.config_](#idx+files+common.config)|
|[\_avail\_metric](#idx+funcs+availmetric)|[\_filter\_valgrind](#idx+funcs+filtervalgrind)|[\_start\_up\_pmlogger](#idx+funcs+startuppmlogger)|[_group_](#idx+files+group)|
|[\_change\_config](#idx+funcs+changeconfig)|[\_filter\_valgrind\_possibly](#idx+funcs+filtervalgrindpossibly)|[\_stop\_auto\_restart](#idx+funcs+stopautorestart)|[_localconfig_](#idx+files+localconfig)|
|[\_check\_64bit\_platform](#idx+funcs+check64bitplatform)|[\_filter\_views](#idx+funcs+filterviews)|[\_systemctl\_status](#idx+funcs+systemctlstatus)|[_qa\_hosts_](#idx+files+qahosts)|
|[\_check\_agent](#idx+funcs+checkagent)|[\_find\_free\_port](#idx+funcs+findfreeport)|[\_triage\_pmcd](#idx+funcs+triagepmcd)|[_qa\_hosts.primary_](#idx+files+qahosts.primary)|
|[\_check\_big\_file](#idx+funcs+checkbigfile)|[\_find\_key\_server\_modules](#idx+funcs+findkeyservermodules)|[\_triage\_wait\_point](#idx+funcs+triagewaitpoint)|[_triaged_](#idx+files+triaged)|
|[\_check\_core](#idx+funcs+checkcore)|[\_find\_key\_server\_name](#idx+funcs+findkeyservername)|[\_try\_pmlc](#idx+funcs+trypmlc)||
|[\_check\_display](#idx+funcs+checkdisplay)|[\_find\_key\_server\_search](#idx+funcs+findkeyserversearch)|[\_value\_filter\_any](#idx+funcs+valuefilterany)||
|[\_check\_freespace](#idx+funcs+checkfreespace)|[\_get\_config](#idx+funcs+getconfig)|[\_value\_filter\_nonzero](#idx+funcs+valuefilternonzero)||
|[\_check\_helgrind](#idx+funcs+checkhelgrind)|[\_get\_endian](#idx+funcs+getendian)|[\_wait\_for\_pmcd](#idx+funcs+waitforpmcd)||
