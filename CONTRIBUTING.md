We welcome your contributions to the Performance Co-Pilot project!

# Contact

Our preferred method of exchanging code, documentation, tests and new
ideas is via git.  To participate, create a cloned git tree using a
public git hosting service (e.g. github) and either send mail to the
list - <pcp@groups.io> - with location and description of the code or
open a pull request: https://github.com/performancecopilot/pcp/pulls

Patches are fine too - send 'em through to the list at any time.  Even
just ideas, pseudo-code, etc - we've found it is often a good idea to
seek early feedback, particularly when modelling the metrics you'll be
exporting from any new PMDAs you might be writing.

## Diversity

We welcome and encourage participation by everyone.  Our community is
based on mutual respect, tolerance and encouragement, and we wish to
help each other live up to these principles.  We want our community
to be more diverse: whoever you are, and whatever your background, we
welcome you.

## Vulnerabilities

If you have discovered a security vulnerability in the PCP code, please
do not hesitate to contact the PCP maintainers immediately.

You can choose to do this either privately or publicly - however, if
you report to the maintainers privately at first we will endeavour to
respond within 3 business days with an initial assessment of the issue,
a timeframe for resolution, and target our next scheduled release with
the fix(es).  We release regularly, typically once every 7-8 weeks.

The maintainers can be contacted privately via <pcp-maintainers@groups.io>

# Philosophy

PCP development has a long tradition of focussing on automated testing.

New tests should be added to the testsuite with new code.  This is not
a hard requirement when sending out patches or git commits, however,
particularly as a new contributor.  Someone else will work with you to
introduce an appropriate level of testing to exercise your fix or new
code, and that will end up being committed along with your new changes.

For the more practiced, regular PCP committers: testing is assumed to
have been done for all commits - both regression testing, using all of
the existing tests, and addition of new tests to exercise the commits.
A guide to using the automated quality assurance test suite exists in
the qa/README file.

For the very practiced, life members of the team: please help out in
terms of getting new contributors moving with their patches - writing
tests with them, giving feedback, and merging their code quickly.  We
aim to have new PMDA or monitoring tool contributions included in the
release immediately following the first arrival of code.  Also ensure
attribution of other contributors code is handled correctly using the
git-commit --author option.

*Above all, have fun!*


# Coding Conventions

Add permanent diagnostics to any code of middling-to-high complexity.
The convention is to test the options in the pmDebugOptions struct
(from libpcp) which is supported by every tool via the -D/--debug
option - see pmdbg(1) for details, and see also the many examples of
its use in existing code.

Use the same coding style used in the code surrounding your changes.

All the existing code is GPL'd, so feel free to borrow and adapt code
that is similar to the function you want to implement.  This maximizes
your chance of absorbing the "style" and "conventions" and producing
code that others will be able to easily read, understand and refine.


# QA (Quality Assurance) Practicalities

Refer to the qa/README file for details on using the testsuite locally.
When writing new tests, bear in mind that other people will be running
those tests *alot*.  An ideal test is one that:

...is small - several small tests trump one big test
   (allowing someone else debugging a test failure to quickly get their
    head around what the test does)

...runs in as short an amount of time as possible
   (allowing someone debugging a failure to iterate quickly through any
    ideas they have while working on the fix)

...uses the same style as all the other tests in the "qa" directory
   (other people will be debugging tests you write and you may need to
    debug other peoples tests as well - sticking to the same script
    style and conventions is universally helpful)

...uses filtering intelligently ... because tests pass or fail based
   on textual comparison of observed and expected output, it is critical
   to filter out the fluff (remove it or make it deterministic) and
   retain the essence of what the test is exercising.  See common.filter
   for some useful filtering functions, and watch out for things like:
   o dates and times (unless using archives)
   o use -z when using archives so the results are not dependent on
     the timezone where the test is run
   o the pid of the running test, especially when part of $tmp that is
     used extensively
   o collating sequences and locale ... take explicit control if in doubt
     

...is portable, testing as many platforms as possible, for example:
   o  use pmdasample rather than a pmda<kernel> metric;
   o  use archives for exercising exact behaviour of monitoring tools;
   o  watch out for differences in output between system utilities;
   o  before running, test for situations where it cannot run - such
      as unsupported platforms/configurations - use _notrun() here.


# Vendoring

To keep track of the vendored code origin and changes, it is strongly advised
to use git subtrees when vendoring 3rd party code. The advantage of using git
subtrees instead of git submodules is that with git subtree the vendored code
is stored inside the PCP repository, instead of merely linking to a specific
commit of the remote repository.

Example usage:
```
# Vendor a new library
git subtree add --prefix vendor/github.com/redis/hiredis \
  https://github.com/redis/hiredis.git v1.0.0 --squash

# Pull changes from the remote repository
git subtree pull --prefix vendor/github.com/redis/hiredis \
  https://github.com/redis/hiredis.git v1.0.1 --squash

# Push modifications of vendored sources to the remote repository
# Note: the commit(s) are split automatically, i.e. if one commit modifies
# both PCP and vendored sources, the commit pushed to the remote repository
# only contains updated files from the vendored directory
git subtree push --prefix vendor/github.com/redis/hiredis \
  https://github.com/andreasgerstmayr/hiredis.git some-updates
```

Example vendoring a subdirectory of a remote repository:
```
# Initial vendoring
git remote add bcc https://github.com/iovisor/bcc.git
git fetch bcc
git checkout bcc/master
git subtree split --prefix libbpf-tools --branch bcc-libbpf-tools-split
git checkout main
git subtree add --prefix vendor/github.com/iovisor/bcc/libbpf-tools bcc-libbpf-tools-split --squash

# Pull changes from the remote repository
git fetch bcc
git checkout bcc/master
git subtree split --prefix libbpf-tools --branch bcc-libbpf-tools-split
git checkout main
git subtree merge --prefix vendor/github.com/iovisor/bcc/libbpf-tools bcc-libbpf-tools-split --squash
```

**Note:** All modifications of vendored code should be pushed upstream. The
goal is to be a good open source citizen and contribute changes back, and to
keep the differences minimal in order to ease future updates of vendored code.

**All bugs and CVEs of a vendored library in turn become responsibilities of the
PCP maintainers as well** (and fixes must be pushed upstream).
