# INSTALL

- Packages
  1. Linux Installation (rpm, deb)
  2. Mac OS X Installation (brew)
  3. AIX Installation
  4. Solaris Installation
  5. Windows Installation
- Building from source
- Post-install steps
- Non-default build, install and run

This document describes how to configure and build the open source
PCP package from source, and how to install and finally run it.

### 1. Linux Installation

If you are using Debian, or a Debian-based distribution like Ubuntu,
PCP is included in the distribution (as of late 2008).  Run:
```
# apt-get install pcp
```

If you are using a RPM based distribution and have the binary rpm:
```
# rpm -Uvh pcp-*.rpm
```
... and skip to the final section (below) - "Post-install steps".

*Special note for Ubuntu 8.04, 8.10, 9.04, 9.10 and 10.04*

I've had to make the changes below to /usr/bin/dpkg-buildpackage.
Without these two changes, my pcp builds produce bad binaries with a
bizarre array of failure modes!
```
my $default_flags = defined $build_opts->{noopt} ? "-g -O0" : "-g
-O2";
my $default_flags = defined $build_opts->{noopt} ? "-g -O0" : "-g -O0";
my %flags = ( CPPFLAGS => '',
              CFLAGS   => $default_flags,
              CXXFLAGS => $default_flags,
              FFLAGS   => $default_flags,
              #kenj# LDFLAGS  => '-Wl,-Bsymbolic-functions',
              LDFLAGS  => '',
    );
```
Without these changes, we see QA failures for 039, 061, 072, 091, 135,
147 and 151 ... and the QA 166 goes into a loop until it fills up the
root filesystem.

*-- Ken*

### 2. Mac OS X Installation

Installing PCP on MacOSX is done via https://brew.sh/ commands.
From a Terminal run:
```
$ brew install qt
$ brew link qt --force
$ brew install pcp
$ brew link pcp
$ pcp --version
```

The output for the last command will be something like
```
pcp version 4.1.1
```

Use the version number for creating symlinks (for version 4.1.1)
```
$ export version="4.1.1"
$ sudo ln -s /usr/local/Cellar/pcp/$version/etc/pcp.conf /etc/pcp.conf
$ sudo ln -s /usr/local/Cellar/pcp/$version/etc/pcp.env /etc/pcp.env
```

### 3. AIX Installation

At this stage, noone is making available pre-built AIX packages.
A port to AIX has been done, and merged, however - building from
the source is currently the only option.  The packaging work is also
begun on this platform (see the build/aix/ directory in the sources).

### 4. Solaris Installation

Prebuild Solaris packages are available from the PCP download site.

At this stage the package are distributed as SVR4 package datastream
and are built on Open Solaris.

You can install the package using 'pkgadd' command, e.g.:
```
	# pkgadd -d pcp-X.Y.Z
```

During the installation the following three services are registered
with the Solaris' service management facility:
```
	# svccfg list \*/pcp/\*
	application/pcp/pmcd
	application/pcp/pmlogger
	application/pcp/pmie
	application/pcp/pmproxy
```

On the new installation all services are disabled, during the upgrade
from the previous version of PCP the state of the services is
preserved.

Use of 'svcadm' command to enable or disable is preferred over explicit
invocation of the pmcd start script.

Use 'svcs' command to check the state of the services, e.g.:
```
	# svcs -l application/pcp/pcp
	fmri         svc:/application/pcp/pcp:default
	name         Performance Co-Pilot Collector Daemon
	enabled      false
	state        disabled
	next_state   none
	state_time   20 March 2012 11:33:27 AM EST
	restarter    svc:/system/svc/restarter:default
	dependency   require_all/none svc:/system/filesystem/local:default (online) svc:/milestone/network:default (online)
```

### 5. Windows Installation

There are 3 ways to get PCP working on Windows:

1) Download the native Windows version of PCP from bintray.com/pcp/windows

2) Set up PCP build environment manually. For that you can:

- download Git for Windows SDK (https://github.com/git-for-windows/build-extra/releases)
- download PCP package from bintray (https://bintray.com/pcp/windows)
- install PCP package via pacman. (pacman -S mingw-w64-x86_64-pcp-X.Y.Z-any.pkg.tar)
- set PCP_DIR to C:\git-sdk-64\mingw64
- add to the system PATH:
..1. "C:\git-sdk-64\mingw64\bin"
..2. "C:\git-sdk-64\mingw64\lib"
..3. "C:\git-sdk-64\mingw64\libexec\pcp\bin"
- start pmcd
```
$PCP_DIR\libexec\pcp\bin\pmcd.exe
```

3) Same as 2 except building the PCP pacman package from source

- follow https://github.com/git-for-windows/git/wiki/Package-management
  and get PKGBUILD from https://github.com/Andrii-hotfix/MINGW-packages
- cd MINGW-packages/mingw-w64-pcp
- makepkg-mingw -s
- install pcp package via pacman as above.


## Building from source

### 0. Preliminaries

The PCP code base is targeted for many different operating
systems and many different combinations of related packages,
so a little planning is needed before launching into a build
from source.

Package dependencies come in several flavours:

- hard build dependencies - without these PCP cannot be
  build from source, and the build will fail in various
  ways at the compilation or packaging stages, e.g. gmake,
  autoconf, flex, bison, ...;

- optional build dependences - if these components are not
  installed the build will work, but the resultant packages
  may be missing some features or entire applications, e.g.
  extended authentication, secure connections, service
  discovery, pmwebd, ...;

- QA dependencies - you can ignore these unless you want to
  run the (extensive) PCP QA suite.

It is strongly recommended that you run the script:
```
$ qa/admin/check-vm
```
and review the output before commencing a build.  It is is generally
safe to ignore packages marked as "N/A" (not available), "build
optional" or "QA optional".  Alternatively use:
```
$ qa/admin/check-vm -bfp
```
(-b for basic packages, -f to **not** try to guess Python, Perl, ...
version and -p to output just package names) to produce a minimal
list of packages that should be installed.

### 1. Configure, build and install the package

The pcp package uses autoconf/configure and expects a GNU build
environment (your platform must at least have gmake).

If you just want to build a .rpm, .deb, .dmg, .msi[*] and/or
tar file, use the "Makepkgs" script in the top level directory.
This will configure and build the package for your platform and leave
binary and src packages in either the build/<pkg-type> directory
or the pcp-<version>/build/<pkg-type> directory.  It will also
leave a source tar file in either the build/tar directory or the
pcp-<version>/build/tar directory.
```
$ ./Makepkgs --verbose
$ ./Makepkgs --verbose --target mingw64
```
Once "Makepkgs" completes you will have package binaries that will
need to be installed.  The recipe depends on the packaging flavour,
but the following should provide guidance:

**dkg install** (Debian and derivative distributions)
```
$ cd build/deb
$ dpkg -i *.deb
```
**rpm install** (RedHat, SuSE and their derivative distributions)
```
$ cd pcp-<version>/build/rpm
$ sudo rpm -U `echo *.rpm | sed -e '/\.src\.rpm$/d'`
```
**tarball install** (where we don't have native packaging working yet)
```
$ cd pcp-<version>/build/tar
$ here=`pwd`
$ tarball=$here/pcp-[0-9]*[0-9].tar.gz
$ sudo ./preinstall
$ cd /
$ sudo tar -zxpf $tarball
$ cd $here
$ sudo ./postinstall
```
[*] Windows builds require https://fedoraproject.org/wiki/MinGW
cross-compilation.  Currently packaging is no longer performed,
although previously MSI builds were possible.  Work on tackling
this short-coming would be most welcome.

Base package list needed for Fedora (26+) cross-compilation:
    mingw64-gcc
    mingw64-binutils
    mingw64-qt5-qttools-tools
    mingw64-qt5-qtbase-devel
    mingw64-pkg-config
    mingw64-readline
    mingw64-xz-libs
    mingw64-qt5-qtsvg
    mingw64-pdcurses
    mingw64-libgnurx

Since Fedora 28, there are also Python packages available:

    mingw64-python2

### 2. Account creation

If you want to build the package and install it manually you will
first need to ensure the "user" pcp is created so that key parts
of the PCP installation can run as a user other than root.
For Debian this means the following (equivalent commands are
available on all distributions):
```
$ su root
# groupadd -r pcp
# useradd -c "Performance Co-Pilot" -g pcp -d /var/lib/pcp -M -r -s /usr/sbin/nologin pcp
```

Then use the following steps (use configure options to suit your
preferences, refer to the qa/admin/myconfigure script for some
guidance and see also section D below for additional details):
```
$ ./configure --prefix=/usr --libexecdir=/usr/lib --sysconfdir=/etc \
	     --localstatedir=/var --with-rcdir=/etc/init.d
$ make
$ su root
# make install
```

   Note 0: PCP services run as non-root by default.  Create unprivileged
   users "pcp" with home directory /var/lib/pcp, and "pcpqa" with home
   directory such as /var/lib/pcp/testsuite, or as appropriate, or
   designate other userids in the pcp.conf file.

   Note 1: that there are so many "install" variants out there that we
   wrote our own script (see "install-sh" in the top level directory),
   which works on every platform supported by PCP.

   Note 2: the Windows build is particularly involved to setup, this
   is primarily due to build tools not being available by default on
   that platform.  See the PCP Glider scripts and notes in the pcpweb
   tree to configure your environment before attempting to build from
   source under Win32.

## Post-install steps

You will need to start the PCP Collection Daemon (PMCD), as root:

Linux:
```
# systemctl start pmcd  (or...)
# service pmcd start  (or...)
# /etc/init.d/pmcd start  (or...)
# /etc/rc.d/init.d/pmcd start
```
Mac OS X:
```
# /Library/StartupItems/pcp/pmcd start
```
Windows:
```
$PCP_DIR/etc/pmcd start
```
Solaris:
```
# svcadm enable application/pcp/pmcd
```
Once you have started the PMCD daemon, you can list all performance
metrics using the pminfo(1) command, E.g.
```
# pminfo -fmdt   (you don't have to be root for this, but you may need to
		  type rehash so your shell finds the pminfo command).
```
If you are writing scripts, you may find the output from pmprobe(1)
easier to parse than that for pminfo(1). There are numerous other
PCP client tools included.

PCP can be configured to automatically log certain performance metrics
for one or more hosts. The scripts to do this are documented in
pmlogger_check(1). By default this facility is not enabled. If you want
to use it, you need to

- determine which metrics to log and how often you need them
- edit $PCP_SYSCONF_DIR/pmlogger/control
- edit $PCP_SYSCONF_DIR/pmlogger/config.default
- (and any others in same dir)
- as root, "crontab -e" and add something like:
```
# -- typical PCP log management crontab entries
# daily processing of pmlogger archives and pmie logs
10      0       *       *       *       $PCP_BINADM_DIR/pmlogger_daily
15      0       *       *       *       $PCP_BINADM_DIR/pmie_daily
#
# every 30 minutes, check pmlogger and pmie instances are running
25,40   *       *       *       *       $PCP_BINADM_DIR/pmlogger_check
5,55    *       *       *       *       $PCP_BINADM_DIR/pmie_check
```
The pmie (Performance Metrics Inference Engine) daemon is _not_
configured to start by default. To enable it, you may want to (on
Linux platforms with chkconfig).
```
# su root
# chkconfig pmie on
# edit the pmie control file (usually below $PCP_SYSCONF_DIR/pmie)
# edit the config file (usually $PCP_SYSCONF_DIR/pmie/config.default)
# set up cron scripts similar to those for pmlogger (see above)
```

#### Configure some optional Performance Metrics Domain Agents (PMDAs)

The default installation gives you the metrics for cpu, per-process,
file system, swap, network, disk, memory, interrupts, nfs/rpc and
others. These metrics are handled using the platform PMDA - namely
pmda_linux.so (Linux), pmda_darwin.dylib (Mac), or pmda_windows.dll
(Windows). It also gives you the PMCD PMDA, which contains metrics
that monitor PCP itself.

There are many other optional PMDAs that you can configure, depending
on which performance metrics you need to monitor, as follows:
Note: $PCP_PMDAS_DIR is normally /var/pcp/pmdas, see pcp.conf(5).

Web Server metrics
```
# su root
# cd $PCP_PMDAS_DIR/apache  (i.e. cd /var/pcp/pmdas/apache)
# ./Install
# Check everything is working OK
# pminfo -fmdt apache
```

Other PMDAs in the pcp package include:

- apache - monitor apache web server stats
- cisco - monitor Cisco router stats
- dbping - query any database, extract response times
- elasticsearch - monitor an elasticsearch cluster
- kvm - monitor kernel-based virtual machine stats
- mailq - monitor the mail queue
- memcache - monitor memcache server stats
- mmv - export memory-mapped value stats from an application
- mounts - keep track of mounted file systems
- mysql - monitor MySQL relational databases
- oracle - monitor Oracle relational databases
- postgres - monitor PostGreSQL relational databases
- process - keep an eye on critical processes/daemons
- roomtemp - monitor room temp (needs suitable probe)
- rsyslog - monitor the reliable system log daemon
- sendmail - monitor sendmail statistics
- shping - ping critical system services, extract response times
- trace - for instrumenting arbitrary applications, see pmtrace(1)
- txmon - transaction and QOS monitoring

- sample - for testing
- simple - example src code if you want to write a new PMDA
- trivial - even easier src code for a new PMDA.

The procedure for configuring all of these is to change to the
directory for the PMDA (usually below /var/lib/pcp/pmdas), and then
run the ./Install script found therein. None of these PMDAs are
configured by default - you choose the PMDAs you need and run the
Install script.  Installation can be automated (defaults chosen) by
touching .NeedInstall in the appropriate pmdas directory and then
restarting the pmcd service via its startup script.

## Non-default build, install and run

To run build and run a version of PCP that is installed in a private
location (and does not require root privileges), first create the
pcp "user" as described in section B.2 above), then
```
$ ./configure --prefix=/some/path
```
This will populate /some/path with a full PCP installation.  To use this
ensure the following are set in the environment:
```
$ export PCP_DIR=/some/path
```
Amend your shell's $PATH to include the PCP directories, found as follows:
```
$ cd /some/path
$ xtra=`grep '^PCP_BIN' etc/pcp.conf | sed -e 's/.*=//' | paste -s -d :`
$ PATH=$xtra:$PATH
```
Ensure the new libraries can be found:
```
$ export LD_LIBRARY_PATH=`grep '^PCP_LIB' etc/pcp.conf \
     | sed -e 's/.*=//' | uniq | paste -s -d :`
```
Tell Perl where to find loadable modules:
```
$ export PERL5LIB=$PCP_DIR/usr/lib/perl5:$PCP_DIR/usr/share/perl5
```
Allow man(1) to find the PCP manual pages:
```
$ export MANPATH=`manpath`:$PCP_DIR/usr/share/man
```
If your version is co-exiting with a running PCP in a default
install, then alternative port numbers in your environment for pmcd
($PMCD_PORT), pmlogger ($PMLOGGER_PORT) and pmproxy ($PMPROXY_PORT)
