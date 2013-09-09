Summary: System-level performance monitoring and performance management
Name: pcp
Version: 3.8.3
%define buildversion 1

Release: %{buildversion}%{?dist}
License: GPLv2+ and LGPLv2.1+
URL: http://oss.sgi.com/projects/pcp
Group: Applications/System
Source0: pcp-%{version}.src.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: procps autoconf bison flex
BuildRequires: nss-devel
BuildRequires: python-devel
BuildRequires: ncurses-devel
BuildRequires: readline-devel
BuildRequires: cyrus-sasl-devel
BuildRequires: libmicrohttpd-devel
BuildRequires: systemtap-sdt-devel
BuildRequires: perl(ExtUtils::MakeMaker)
BuildRequires: initscripts man /bin/hostname
%if 0%{?fedora} >= 18 || 0%{?rhel} >= 7
BuildRequires: systemd-devel
%endif
 
Requires: bash gawk sed grep fileutils findutils initscripts perl
Requires: python
%if 0%{?rhel} <= 5
Requires: python-ctypes
%endif

Requires: pcp-libs = %{version}-%{release}
Requires: python-pcp = %{version}-%{release}
Requires: perl-PCP-PMDA = %{version}-%{release}

%global tapsetdir      %{_datadir}/systemtap/tapset

%define _confdir  %{_sysconfdir}/pcp
%define _initddir %{_sysconfdir}/rc.d/init.d
%define _logsdir  %{_localstatedir}/log/pcp
%define _pmnsdir  %{_localstatedir}/lib/pcp/pmns
%define _tempsdir %{_localstatedir}/lib/pcp/tmp
%define _pmdasdir %{_localstatedir}/lib/pcp/pmdas
%define _testsdir %{_localstatedir}/lib/pcp/testsuite
%if 0%{?fedora} >= 20
%define _with_doc --with-docdir=%{_docdir}/%{name}
%else
%define _with_doc %{nil}
%endif

%description
Performance Co-Pilot (PCP) provides a framework and services to support
system-level performance monitoring and performance management. 

The PCP open source release provides a unifying abstraction for all of
the interesting performance data in a system, and allows client
applications to easily retrieve and process any subset of that data. 

#
# pcp-libs
#
%package libs
License: LGPLv2
Group: Development/Libraries
Summary: Performance Co-Pilot run-time libraries
URL: http://oss.sgi.com/projects/pcp/

%description libs
Performance Co-Pilot (PCP) run-time libraries

#
# pcp-libs-devel
#
%package libs-devel
License: GPLv2
Group: Development/Libraries
Summary: Performance Co-Pilot (PCP) development headers and documentation
URL: http://oss.sgi.com/projects/pcp/

Requires: pcp-libs = %{version}-%{release}

%description libs-devel
Performance Co-Pilot (PCP) headers, documentation and tools for development.

#
# pcp-testsuite
#
%package testsuite
License: GPLv2
Group: Development/Libraries
Summary: Performance Co-Pilot (PCP) test suite
URL: http://oss.sgi.com/projects/pcp/
Requires: pcp = %{version}-%{release}
Requires: pcp-libs-devel = %{version}-%{release}
# Requires: valgrind	# arch-specific

%description testsuite
Quality assurance test suite for Performance Co-Pilot (PCP).

#
# perl-PCP-PMDA. This is the PCP agent perl binding.
#
%package -n perl-PCP-PMDA
License: GPLv2
Group: Development/Libraries
Summary: Performance Co-Pilot (PCP) Perl bindings and documentation
URL: http://oss.sgi.com/projects/pcp/
Requires: pcp-libs = %{version}-%{release}

%description -n perl-PCP-PMDA
The PCP::PMDA Perl module contains the language bindings for
building Performance Metric Domain Agents (PMDAs) using Perl.
Each PMDA exports performance data for one specific domain, for
example the operating system kernel, Cisco routers, a database,
an application, etc.

#
# perl-PCP-MMV
#
%package -n perl-PCP-MMV
License: GPLv2
Group: Development/Libraries
Summary: Performance Co-Pilot (PCP) Perl bindings for PCP Memory Mapped Values
URL: http://oss.sgi.com/projects/pcp/
Requires: pcp >= %{version}-%{release}

%description -n perl-PCP-MMV
The PCP::MMV module contains the Perl language bindings for
building scripts instrumented with the Performance Co-Pilot
(PCP) Memory Mapped Value (MMV) mechanism.
This mechanism allows arbitrary values to be exported from an
instrumented script into the PCP infrastructure for monitoring
and analysis with pmchart, pmie, pmlogger and other PCP tools.

#
# perl-PCP-LogImport
#
%package -n perl-PCP-LogImport
License: GPLv2
Group: Development/Libraries
Summary: Performance Co-Pilot (PCP) Perl bindings for importing external data into PCP archives
URL: http://oss.sgi.com/projects/pcp/
Requires: pcp >= %{version}-%{release}

%description -n perl-PCP-LogImport
The PCP::LogImport module contains the Perl language bindings for
importing data in various 3rd party formats into PCP archives so
they can be replayed with standard PCP monitoring tools.

 #
# perl-PCP-LogSummary
#
%package -n perl-PCP-LogSummary
License: GPLv2
Group: Development/Libraries
Summary: Performance Co-Pilot (PCP) Perl bindings for post-processing output of pmlogsummary
URL: http://oss.sgi.com/projects/pcp/
Requires: pcp >= %{version}-%{release}

%description -n perl-PCP-LogSummary
The PCP::LogSummary module provides a Perl module for using the
statistical summary data produced by the Performance Co-Pilot
pmlogsummary utility.  This utility produces various averages,
minima, maxima, and other calculations based on the performance
data stored in a PCP archive.  The Perl interface is ideal for
exporting this data into third-party tools (e.g. spreadsheets).

#
# pcp-import-sar2pcp
#
%package import-sar2pcp
License: LGPLv2+
Group: Applications/System
Summary: Performance Co-Pilot tools for importing sar data into PCP archive logs
URL: http://oss.sgi.com/projects/pcp/
Requires: pcp-libs >= %{version}-%{release}
Requires: perl-PCP-LogImport >= %{version}-%{release}
Requires: sysstat

%description import-sar2pcp
Performance Co-Pilot (PCP) front-end tools for importing sar data
into standard PCP archive logs for replay with any PCP monitoring tool.

#
# pcp-import-iostat2pcp
#
%package import-iostat2pcp
License: LGPLv2+
Group: Applications/System
Summary: Performance Co-Pilot tools for importing iostat data into PCP archive logs
URL: http://oss.sgi.com/projects/pcp/
Requires: pcp-libs >= %{version}-%{release}
Requires: perl-PCP-LogImport >= %{version}-%{release}
Requires: sysstat

%description import-iostat2pcp
Performance Co-Pilot (PCP) front-end tools for importing iostat data
into standard PCP archive logs for replay with any PCP monitoring tool.

#
# pcp-import-mrtg2pcp
#
%package import-mrtg2pcp
License: LGPLv2+
Group: Applications/System
Summary: Performance Co-Pilot tools for importing MTRG data into PCP archive logs
URL: http://oss.sgi.com/projects/pcp/
Requires: pcp-libs >= %{version}-%{release}
Requires: perl-PCP-LogImport >= %{version}-%{release}

%description import-mrtg2pcp
Performance Co-Pilot (PCP) front-end tools for importing MTRG data
into standard PCP archive logs for replay with any PCP monitoring tool.

#
# pcp-import-collectl2pcp
#
%package import-collectl2pcp
License: LGPLv2+
Group: Applications/System
Summary: Performance Co-Pilot tools for importing collectl log files into PCP archive logs
URL: http://oss.sgi.com/projects/pcp/
Requires: pcp-libs >= %{version}-%{release}

%description import-collectl2pcp
Performance Co-Pilot (PCP) front-end tools for importing collectl data
into standard PCP archive logs for replay with any PCP monitoring tool.

#
# pcp-pmda-infiniband
#
%package pmda-infiniband
License: GPLv2
Group: Applications/System
Summary: Performance Co-Pilot (PCP) metrics for Infiniband HCAs and switches
URL: http://oss.sgi.com/projects/pcp/
Requires: pcp-libs >= %{version}-%{release}
Requires: libibmad >= 1.1.7 libibumad >= 1.1.7
BuildRequires: libibmad-devel >= 1.1.7 libibumad-devel >= 1.1.7

%description pmda-infiniband
This package contains the PCP Performance Metrics Domain Agent (PMDA) for
collecting Infiniband statistics.  By default, it monitors the local HCAs
but can also be configured to monitor remote GUIDs such as IB switches.

#
# python-pcp. This is the PCP library bindings for python.
#
%package -n python-pcp
License: GPLv2
Group: Development/Libraries
Summary: Performance Co-Pilot (PCP) Python bindings and documentation
URL: http://oss.sgi.com/projects/pcp/
Requires: pcp-libs = %{version}-%{release}

%description -n python-pcp
The python PCP module contains the language bindings for
building Performance Metric API (PMAPI) tools using Python.

%prep
%setup -q

%clean
rm -Rf $RPM_BUILD_ROOT

%build
%configure --with-rcdir=%{_initddir} --with-tmpdir=%{_tempsdir} %{_with_doc}
make default_pcp

%install
rm -Rf $RPM_BUILD_ROOT
export NO_CHOWN=true DIST_ROOT=$RPM_BUILD_ROOT
make install_pcp

# Fix stuff we do/don't want to ship
rm -f $RPM_BUILD_ROOT/%{_libdir}/*.a
mkdir -p $RPM_BUILD_ROOT/%{_localstatedir}/run/pcp

# remove sheet2pcp until BZ 830923 and BZ 754678 are resolved.
rm -f $RPM_BUILD_ROOT/%{_bindir}/sheet2pcp $RPM_BUILD_ROOT/%{_mandir}/man1/sheet2pcp.1.gz

# default chkconfig off for Fedora and RHEL
for f in $RPM_BUILD_ROOT/%{_initddir}/{pcp,pmcd,pmlogger,pmie,pmwebd,pmproxy}; do
	sed -i -e '/^# chkconfig/s/:.*$/: - 95 05/' -e '/^# Default-Start:/s/:.*$/:/' $f
done

# list of PMDAs in the base pkg
ls -1 $RPM_BUILD_ROOT/%{_pmdasdir} | egrep -v 'simple|sample|trivial|txmon' |\
sed -e 's#^#'%{_pmdasdir}'\/#' >base_pmdas.list

# bin and man1 files except those split out into sub packages
ls -1 $RPM_BUILD_ROOT/%{_bindir} | grep -v '2pcp' |\
sed -e 's#^#'%{_bindir}'\/#' >base_binfiles.list
ls -1 $RPM_BUILD_ROOT/%{_mandir}/man1 | grep -v '2pcp' |\
sed -e 's#^#'%{_mandir}'\/man1\/#' >base_man1files.list

cat base_pmdas.list base_binfiles.list base_man1files.list > base_specialfiles.list

%pre testsuite
getent group pcpqa >/dev/null || groupadd -r pcpqa
getent passwd pcpqa >/dev/null || \
  useradd -c "PCP Quality Assurance" -g pcpqa -m -r -s /bin/bash pcpqa 2>/dev/null
exit 0

%post testsuite
chown -R pcpqa:pcpqa %{_testsdir} 2>/dev/null
exit 0

%pre
getent group pcp >/dev/null || groupadd -r pcp
getent passwd pcp >/dev/null || \
  useradd -c "Performance Co-Pilot" -g pcp -d %{_localstatedir}/lib/pcp -M -r -s /sbin/nologin pcp
PCP_SYSCONF_DIR=%{_confdir}
PCP_LOG_DIR=%{_logsdir}
# produce a script to run post-install to move configs to their new homes
save_configs_script()
{
    _new="$1"
    shift
    for _dir
    do
        [ "$_dir" = "$_new" ] && continue
        if [ -d "$_dir" ]
        then
            ( cd "$_dir" ; find . -type f -print ) | sed -e 's/^\.\///' \
            | while read _file
            do
                _want=true
                if [ -f "$_new/$_file" ]
                then
                    # file exists in both directories, pick the more
                    # recently modified one
                    _try=`find "$_dir/$_file" -newer "$_new/$_file" -print`
                    [ -n "$_try" ] || _want=false
                fi
                $_want && echo cp -p "$_dir/$_file" "$_new/$_file"
            done
        fi
    done
}
# migrate and clean configs if we have had a previous in-use installation
[ -d "$PCP_LOG_DIR" ] || exit 0	# no configuration file upgrades required
rm -f "$PCP_LOG_DIR/configs.sh"
for daemon in pmcd pmie pmlogger pmwebd pmproxy
do
    save_configs_script >> "$PCP_LOG_DIR/configs.sh" "$PCP_SYSCONF_DIR/$daemon" \
        /var/lib/pcp/config/$daemon /etc/$daemon /etc/pcp/$daemon /etc/sysconfig/$daemon
done
exit 0

%preun
if [ "$1" -eq 0 ]
then
    #
    # Stop daemons before erasing the package
    #
    /sbin/service pmlogger stop >/dev/null 2>&1
    /sbin/service pmie stop >/dev/null 2>&1
    /sbin/service pmproxy stop >/dev/null 2>&1
    /sbin/service pmwebd stop >/dev/null 2>&1
    /sbin/service pmcd stop >/dev/null 2>&1

    /sbin/chkconfig --del pcp >/dev/null 2>&1
    /sbin/chkconfig --del pmcd >/dev/null 2>&1
    /sbin/chkconfig --del pmlogger >/dev/null 2>&1
    /sbin/chkconfig --del pmie >/dev/null 2>&1
    /sbin/chkconfig --del pmwebd >/dev/null 2>&1
    /sbin/chkconfig --del pmproxy >/dev/null 2>&1
fi

%post
PCP_LOG_DIR=%{_logsdir}
PCP_PMNS_DIR=%{_pmnsdir}
# restore saved configs, if any
test -s "$PCP_LOG_DIR/configs.sh" && source "$PCP_LOG_DIR/configs.sh"
rm -f $PCP_LOG_DIR/configs.sh

# migrate old to new temp dir locations (within the same filesystem)
migrate_tempdirs()
{
    _sub="$1"
    _new_tmp_dir=%{_tempsdir}
    _old_tmp_dir=%{_localstatedir}/tmp

    for d in "$_old_tmp_dir/$_sub" ; do
        test -d "$d" -a -k "$d" || continue
        cd "$d" || continue
        for f in * ; do
            [ "$f" != "*" ] || continue
            source="$d/$f"
            target="$_new_tmp_dir/$_sub/$f"
            [ "$source" != "$target" ] || continue
	    [ -f "$target" ] || mv -fu "$source" "$target"
        done
        cd && rmdir "$d" 2>/dev/null
    done
}
for daemon in mmv pmdabash pmie pmlogger
do
    migrate_tempdirs $daemon
done
chown -R pcp:pcp %{_logsdir}/pmcd 2>/dev/null
chown -R pcp:pcp %{_logsdir}/pmlogger 2>/dev/null
chown -R pcp:pcp %{_logsdir}/pmie 2>/dev/null
chown -R pcp:pcp %{_logsdir}/pmwebd 2>/dev/null
chown -R pcp:pcp %{_logsdir}/pmproxy 2>/dev/null
# we only need this manual Rebuild as long as pmcd is condstart below
[ -f "$PCP_PMNS_DIR/root" ] || ( cd "$PCP_PMNS_DIR" && ./Rebuild -sud )
/sbin/chkconfig --add pmcd >/dev/null 2>&1
/sbin/service pmcd condrestart
/sbin/chkconfig --add pmlogger >/dev/null 2>&1
/sbin/service pmlogger condrestart
/sbin/chkconfig --add pmie >/dev/null 2>&1
/sbin/service pmie condrestart
/sbin/chkconfig --add pmwebd >/dev/null 2>&1
/sbin/service pmwebd condrestart
/sbin/chkconfig --add pmproxy >/dev/null 2>&1
/sbin/service pmproxy condrestart

%post libs -p /sbin/ldconfig
%postun libs -p /sbin/ldconfig

%files -f base_specialfiles.list
#
# Note: there are some headers (e.g. domain.h) and in a few cases some
# C source files that rpmlint complains about. These are not devel files,
# but rather they are (slightly obscure) PMDA config files.
#
%defattr(-,root,root)
%doc CHANGELOG COPYING INSTALL README VERSION.pcp pcp.lsm

%dir %{_pmdasdir}
%dir %{_datadir}/pcp
%dir %attr(0775,pcp,pcp) %{_localstatedir}/run/pcp
%dir %{_localstatedir}/lib/pcp
%dir %{_localstatedir}/lib/pcp/config
%dir %attr(0775,pcp,pcp) %{_localstatedir}/lib/pcp/config/pmda
%dir %attr(1777,root,root) %{_tempsdir}

%{_libexecdir}/pcp
%{_datadir}/pcp/lib
%{_logsdir}
%attr(0775,pcp,pcp) %{_logsdir}/pmcd
%attr(0775,pcp,pcp) %{_logsdir}/pmlogger
%attr(0775,pcp,pcp) %{_logsdir}/pmie
%attr(0775,pcp,pcp) %{_logsdir}/pmwebd
%attr(0775,pcp,pcp) %{_logsdir}/pmproxy
%{_localstatedir}/lib/pcp/pmns
%{_initddir}/pcp
%{_initddir}/pmcd
%{_initddir}/pmlogger
%{_initddir}/pmie
%{_initddir}/pmwebd
%{_initddir}/pmproxy
%{_mandir}/man5/*
%config(noreplace) %{_sysconfdir}/sasl2/pmcd.conf
%config(noreplace) %{_sysconfdir}/cron.d/pmlogger
%config(noreplace) %{_sysconfdir}/cron.d/pmie
%config %{_sysconfdir}/bash_completion.d/pcp
%config %{_sysconfdir}/pcp.env
%{_sysconfdir}/pcp.sh
%{_sysconfdir}/pcp
%config(noreplace) %{_confdir}/pmcd/pmcd.conf
%config(noreplace) %{_confdir}/pmcd/pmcd.options
%config(noreplace) %{_confdir}/pmcd/rc.local
%config(noreplace) %{_confdir}/pmwebd/pmwebd.options
%config(noreplace) %{_confdir}/pmproxy/pmproxy.options
%dir %attr(0775,pcp,pcp) %{_confdir}/pmie
%attr(0664,pcp,pcp) %config(noreplace) %{_confdir}/pmie/control
%dir %attr(0775,pcp,pcp) %{_confdir}/pmlogger
%attr(0664,pcp,pcp) %config(noreplace) %{_confdir}/pmlogger/control
%{_localstatedir}/lib/pcp/config/*
%{tapsetdir}/pmcd.stp

%files libs
%defattr(-,root,root)

%dir %{_includedir}/pcp
%{_includedir}/pcp/builddefs
%{_includedir}/pcp/buildrules
%config %{_sysconfdir}/pcp.conf
%{_libdir}/libpcp.so.3
%{_libdir}/libpcp_gui.so.2
%{_libdir}/libpcp_mmv.so.1
%{_libdir}/libpcp_pmda.so.3
%{_libdir}/libpcp_trace.so.2
%{_libdir}/libpcp_import.so.1

%files libs-devel
%defattr(-,root,root)

%{_libdir}/libpcp.so
%{_libdir}/libpcp.so.2
%{_libdir}/libpcp_gui.so
%{_libdir}/libpcp_gui.so.1
%{_libdir}/libpcp_mmv.so
%{_libdir}/libpcp_pmda.so
%{_libdir}/libpcp_pmda.so.2
%{_libdir}/libpcp_trace.so
%{_libdir}/libpcp_import.so
%{_includedir}/pcp/*.h
%{_mandir}/man3/*.3.gz
%{_datadir}/pcp/demos
%{_datadir}/pcp/examples

# PMDAs that ship src and are not for production use
# straight out-of-the-box, for devel or QA use only.
%{_pmdasdir}/simple
%{_pmdasdir}/sample
%{_pmdasdir}/trivial
%{_pmdasdir}/txmon

%files testsuite
%defattr(-,pcpqa,pcpqa)
%{_testsdir}

%files import-sar2pcp
%defattr(-,root,root)
%{_bindir}/sar2pcp
%{_mandir}/man1/sar2pcp.1.gz

%files import-iostat2pcp
%defattr(-,root,root)
%{_bindir}/iostat2pcp
%{_mandir}/man1/iostat2pcp.1.gz

%files import-mrtg2pcp
%defattr(-,root,root)
%{_bindir}/mrtg2pcp
%{_mandir}/man1/mrtg2pcp.1.gz

%files import-collectl2pcp
%defattr(-,root,root)
%{_bindir}/collectl2pcp
%{_mandir}/man1/collectl2pcp.1.gz

%files pmda-infiniband
%defattr(-,root,root)
%{_pmdasdir}/ib
%{_pmdasdir}/infiniband
%{_mandir}/man1/pmdaib.1.gz

%files -n perl-PCP-PMDA -f perl-pcp-pmda.list
%defattr(-,root,root)

%files -n perl-PCP-MMV -f perl-pcp-mmv.list
%defattr(-,root,root)

%files -n perl-PCP-LogImport -f perl-pcp-logimport.list
%defattr(-,root,root)

%files -n perl-PCP-LogSummary -f perl-pcp-logsummary.list
%defattr(-,root,root)

%files -n python-pcp -f python-pcp.list.rpm
%defattr(-,root,root)

%changelog
* Mon Sep 09 2013 Nathan Scott <nathans@redhat.com> - 3.8.3-1
- Default to Unix domain socket (authenticated) local connections.
- Introduces new pcp-pmda-infiniband sub-package.

* Wed Jul 31 2013 Nathan Scott <nathans@redhat.com> - 3.8.2-1
- Update to latest PCP sources.
- Integrate gluster related stats with PCP (BZ 969348)
- Fix for iostat2pcp not parsing iostat output (BZ 981545)
- Start pmlogger with usable config by default (BZ 953759)
- Fix pmatop failing to start, gives stacktrace (BZ 963085)

* Wed Jun 19 2013 Nathan Scott <nathans@redhat.com> - 3.8.1-1
- Update to latest PCP sources.
- Fix log import silently dropping >1024 metrics (BZ 968210)
- Move some commonly used tools on the usual PATH (BZ 967709)
- Improve pmatop handling of missing proc metrics (BZ 963085)
- Stop out-of-order records corrupting import logs (BZ 958745)

* Tue May 14 2013 Nathan Scott <nathans@redhat.com> - 3.8.0-1
- Update to latest PCP sources.
- Validate metric names passed into pmiAddMetric (BZ 958019)
- Install log directories with correct ownership (BZ 960858)

* Fri Apr 19 2013 Nathan Scott <nathans@redhat.com> - 3.7.2-1
- Update to latest PCP sources.
- Ensure root namespace exists at the end of install (BZ 952977)

* Wed Mar 20 2013 Nathan Scott <nathans@redhat.com> - 3.7.1-1
- Update to latest PCP sources.
- Migrate all tempfiles correctly to the new tempdir hierarchy.

* Sun Mar 10 2013 Nathan Scott <nathans@redhat.com> - 3.7.0-1
- Update to latest PCP sources.
- Migrate all configuration files below the /etc/pcp hierarchy.

* Thu Feb 14 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 3.6.10-2.1
- Rebuilt for https://fedoraproject.org/wiki/Fedora_19_Mass_Rebuild

* Wed Nov 28 2012 Nathan Scott <nathans@redhat.com> - 3.6.10-2
- Ensure tmpfile directories created in %%files section.
- Resolve tmpfile create/teardown race conditions.

* Mon Nov 19 2012 Nathan Scott <nathans@redhat.com> - 3.6.10-1
- Update to latest PCP sources.
- Resolve tmpfile security flaws: CVE-2012-5530
- Introduces new "pcp" user account for all daemons to use.

* Fri Oct 12 2012 Nathan Scott <nathans@redhat.com> - 3.6.9-1
- Update to latest PCP sources.
- Fix pmcd sigsegv in NUMA/CPU indom setup (BZ 858384)
- Fix sar2pcp uninitialised perl variable warning (BZ 859117)
- Fix pcp.py and pmcollectl with older python versions (BZ 852234)

* Fri Sep 14 2012 Nathan Scott <nathans@redhat.com> - 3.6.8-1
- Update to latest PCP sources.

* Wed Sep 05 2012 Nathan Scott <nathans@redhat.com> - 3.6.6-1.1
- Move configure step from prep to build section of spec (BZ 854128)

* Tue Aug 28 2012 Mark Goodwin <mgoodwin@redhat.com> - 3.6.6-1
- Update to latest PCP sources, see installed CHANGELOG for details.
- Introduces new python-pcp and pcp-testsuite sub-packages.

* Thu Aug 16 2012 Mark Goodwin <mgoodwin@redhat.com> - 3.6.5-1
- Update to latest PCP sources, see installed CHANGELOG for details.
- Fix security flaws: CVE-2012-3418 CVE-2012-3419 CVE-2012-3420 and CVE-2012-3421 (BZ 848629)

* Thu Jul 19 2012 Mark Goodwin <mgoodwin@redhat.com>
- pmcd and pmlogger services are not supposed to be enabled by default (BZ 840763) - 3.6.3-1.3

* Thu Jun 21 2012 Mark Goodwin <mgoodwin@redhat.com>
- remove pcp-import-sheet2pcp subpackage due to missing deps (BZ 830923) - 3.6.3-1.2

* Fri May 18 2012 Dan Hork <dan[at]danny.cz> - 3.6.3-1.1
- fix build on s390x

* Mon Apr 30 2012 Mark Goodwin - 3.6.3-1
- Update to latest PCP sources

* Thu Apr 26 2012 Mark Goodwin - 3.6.2-1
- Update to latest PCP sources

* Thu Apr 12 2012 Mark Goodwin - 3.6.1-1
- Update to latest PCP sources

* Thu Mar 22 2012 Mark Goodwin - 3.6.0-1
- use %%configure macro for correct libdir logic
- update to latest PCP sources

* Thu Dec 15 2011 Mark Goodwin - 3.5.11-2
- patched configure.in for libdir=/usr/lib64 on ppc64

* Thu Dec 01 2011 Mark Goodwin - 3.5.11-1
- Update to latest PCP sources.

* Fri Nov 04 2011 Mark Goodwin - 3.5.10-1
- Update to latest PCP sources.

* Mon Oct 24 2011 Mark Goodwin - 3.5.9-1
- Update to latest PCP sources.

* Mon Aug 08 2011 Mark Goodwin - 3.5.8-1
- Update to latest PCP sources.

* Fri Aug 05 2011 Mark Goodwin - 3.5.7-1
- Update to latest PCP sources.

* Fri Jul 22 2011 Mark Goodwin - 3.5.6-1
- Update to latest PCP sources.

* Tue Jul 19 2011 Mark Goodwin - 3.5.5-1
- Update to latest PCP sources.

* Thu Feb 03 2011 Mark Goodwin - 3.5.0-1
- Update to latest PCP sources.

* Thu Sep 30 2010 Mark Goodwin - 3.4.0-1
- Update to latest PCP sources.

* Fri Jul 16 2010 Mark Goodwin - 3.3.3-1
- Update to latest PCP sources.

* Sat Jul 10 2010 Mark Goodwin - 3.3.2-1
- Update to latest PCP sources.

* Tue Jun 29 2010 Mark Goodwin - 3.3.1-1
- Update to latest PCP sources.

* Fri Jun 25 2010 Mark Goodwin - 3.3.0-1
- Update to latest PCP sources.

* Thu Mar 18 2010 Mark Goodwin - 3.1.2-1
- Update to latest PCP sources.

* Wed Jan 27 2010 Mark Goodwin - 3.1.0-1
- BuildRequires: initscripts for %%{_vendor} == redhat.

* Thu Dec 10 2009 Mark Goodwin - 3.0.3-1
- BuildRequires: initscripts for FC12.

* Wed Dec 02 2009 Mark Goodwin - 3.0.2-1
- Added sysfs.kernel metrics, rebased to minor community release.

* Mon Oct 19 2009 Martin Hicks <mort@sgi.com> - 3.0.1-2
- Remove IB dependencies.  The Infiniband PMDA is being moved to
  a stand-alone package.
- Move cluster PMDA to a stand-alone package.

* Fri Oct 09 2009 Mark Goodwin <mgoodwin@redhat.com> - 3.0.0-9
- This is the initial import for Fedora
- See 3.0.0 details in CHANGELOG
