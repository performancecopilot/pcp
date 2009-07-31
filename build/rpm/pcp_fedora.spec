Summary: System-level performance monitoring and performance management
Name: pcp
Version: 3.0.0
Release: 1%{?dist}
License: GPL+ and LGPLv2+
URL: http://oss.sgi.com/projects/pcp
Group: Applications/System
Source0: ftp://oss.sgi.com/projects/pcp/download/v3/pcp-3.0.0-1.src.tar.gz

# Infiniband monitoring support turned off (for now)
%define have_ibdev 0

%if %{have_ibdev}
%define ib_prereqs libibmad libibumad  libibcommon 
%define ib_build_prereqs %{ib_prereqs} libibmad-devel libibumad-devel libibcommon-devel
%endif

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: gcc-c++ libstdc++-devel procps autoconf bison flex ncurses-devel %{?ib_build_prereqs}

Requires: bash gawk sed grep fileutils findutils cpp initscripts %{?ib_prereqs}

%ifarch ia64
Requires: libunwind
%endif

Requires: pcp-libs = %{version}

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
Group: Applications/System
Summary: Performance Co-Pilot run-time libraries
Vendor: Silicon Graphics, Inc.
URL: http://oss.sgi.com/projects/pcp/

#
# Prior to v3, the PCP package implicitly "provides" -libs and -devel.
# So pcp-libs needs to obsolete the entire v2 PCP package. Note that
# pcp-devel doesn't need to do this because it requires pcp-libs.
Obsoletes: pcp < 3.0

%description libs
Performance Co-Pilot (PCP) run-time libraries

#
# pcp-devel
#
%package devel
Group: Applications/System
Summary: Performance Co-Pilot (PCP) development headers and static libraries
Vendor: Silicon Graphics, Inc.
URL: http://oss.sgi.com/projects/pcp/

Requires: pcp-libs = %{version}

%description devel
Performance Co-Pilot (PCP) headers, static libraries, documentation
and tools for development.

%prep
%setup -q
autoconf

# the %configure macro should be used here
./configure --libdir=%{_libdir}

%clean
rm -Rf $RPM_BUILD_ROOT

%build
make default_pcp

%install
rm -Rf $RPM_BUILD_ROOT
export DIST_ROOT=$RPM_BUILD_ROOT
%makeinstall

# Remove stuff we don't want to ship
rm -f $RPM_BUILD_ROOT/%{_libdir}/*.a

%files
%defattr(-,root,root)

%dir %{_defaultdocdir}/pcp-*
%dir %{_libdir}*/pcp
%dir %{_libdir}*/pcp/bin
%dir %{_datadir}/pcp/demos
%dir %{_datadir}/pcp/demos/*
%dir %{_datadir}/pcp/examples
%dir %{_datadir}/pcp/examples/*
%dir %{_datadir}/pcp/lib
%dir %{_localstatedir}/lib/pcp
%dir %{_localstatedir}/lib/pcp/config
%dir %{_localstatedir}/lib/pcp/config/*
%dir %{_localstatedir}/lib/pcp/config/pmie/cisco
%dir %{_localstatedir}/lib/pcp/config/pmieconf/shping
%dir %{_localstatedir}/lib/pcp/pmdas
%dir %{_localstatedir}/lib/pcp/pmdas/*
%dir %{_localstatedir}/lib/pcp/pmns
%dir %{_localstatedir}/log/pcp
%dir %{_localstatedir}/log/pcp/pmcd
%dir %{_localstatedir}/log/pcp/pmproxy
%{_bindir}/*
%{_libdir}*/pcp/bin/*
%{_defaultdocdir}/pcp-*/*
%{_datadir}/pcp/demos/*/*
%{_datadir}/pcp/examples/*/*
%{_datadir}/pcp/lib/*
%{_initrddir}/pcp
%{_initrddir}/pmie
%{_initrddir}/pmproxy
%{_mandir}/man1/*
%{_mandir}/man4/*
%{_sysconfdir}/bash_completion.d/pcp
%{_sysconfdir}/pcp.env
%{_localstatedir}/lib/pcp/pmns/Makefile
%{_localstatedir}/lib/pcp/pmns/Make.stdpmid
%{_localstatedir}/lib/pcp/pmns/Rebuild
%{_localstatedir}/lib/pcp/pmns/root_linux
%{_localstatedir}/lib/pcp/pmns/root_pmcd
%{_localstatedir}/lib/pcp/pmns/stdpmid.local
%{_localstatedir}/lib/pcp/pmns/stdpmid.pcp
%config(noreplace) %{_localstatedir}/lib/pcp/config/pmcd/pmcd.conf
%config(noreplace) %{_localstatedir}/lib/pcp/config/pmcd/pmcd.options
%{_localstatedir}/lib/pcp/config/pmcd/rc.local
%{_localstatedir}/lib/pcp/config/pmchart/*
%{_localstatedir}/lib/pcp/config/pmafm/*
%{_localstatedir}/lib/pcp/config/pmie/cisco/in_util
%{_localstatedir}/lib/pcp/config/pmie/cisco/out_util
%config(noreplace) %{_localstatedir}/lib/pcp/config/pmie/config.default
%{_localstatedir}/lib/pcp/config/pmieconf/shping/response
%{_localstatedir}/lib/pcp/config/pmieconf/shping/status
%config(noreplace) %{_localstatedir}/lib/pcp/config/pmie/control
%config(noreplace) %{_localstatedir}/lib/pcp/config/pmie/crontab
%{_localstatedir}/lib/pcp/config/pmie/stomp
%{_localstatedir}/lib/pcp/config/pmlogger/config.base
%config(noreplace) %{_localstatedir}/lib/pcp/config/pmlogger/config.default
%{_localstatedir}/lib/pcp/config/pmlogger/config.pcp
%{_localstatedir}/lib/pcp/config/pmlogger/config.pmclient
%{_localstatedir}/lib/pcp/config/pmlogger/config.pmstat
%{_localstatedir}/lib/pcp/config/pmlogger/config.sar

%config(noreplace) %{_localstatedir}/lib/pcp/config/pmlogger/control
%config(noreplace) %{_localstatedir}/lib/pcp/config/pmlogger/crontab
%{_localstatedir}/lib/pcp/config/pmlogger/Makefile
%config(noreplace) %{_localstatedir}/lib/pcp/config/pmproxy/pmproxy.options
%{_localstatedir}/lib/pcp/pmdas/*/*

%files libs
%defattr(-,root,root)

%dir %{_includedir}/pcp
%{_includedir}/pcp/builddefs
%{_includedir}/pcp/buildrules
%config(noreplace) %{_sysconfdir}/pcp.conf
%{_libdir}/libpcp.so.3
%{_libdir}/libpcp_gui.so.2
%{_libdir}/libpcp_mmv.so.1
%{_libdir}/libpcp_pmda.so.3
%{_libdir}/libpcp_trace.so.2

%files devel
%defattr(-,root,root)

%{_libdir}/libpcp.so
%{_libdir}/libpcp.so.2
%{_libdir}/libpcp_gui.so
%{_libdir}/libpcp_gui.so.1
%{_libdir}/libpcp_mmv.so
%{_libdir}/libpcp_pmda.so
%{_libdir}/libpcp_pmda.so.2
%{_libdir}/libpcp_trace.so
%{_includedir}/pcp/*.h
%{_mandir}/man3/*

%preun
if [ "$1" -eq 0 ]
then
    #
    # Stop daemons before erasing the package
    #
    if [ -f /etc/pcp.env -a -f /etc/pcp.conf ] ; then
	. /etc/pcp.env
	/sbin/service pcp stop >/dev/null 2>&1
	/sbin/service pmie stop >/dev/null 2>&1
	/sbin/service pmproxy stop >/dev/null 2>&1
	rm -f $PCP_VAR_DIR/pmns/.NeedRebuild
    fi

    /sbin/chkconfig --del pcp >/dev/null 2>&1
    /sbin/chkconfig --del pmie >/dev/null 2>&1
    /sbin/chkconfig --del pmproxy >/dev/null 2>&1
fi
exit 0

%postun
exit 0

%post
if [ -f /etc/pcp.env -a -f /etc/pcp.conf ] ; then
    . /etc/pcp.env
    . $PCP_SHARE_DIR/lib/rc-proc.sh
    touch $PCP_VAR_DIR/pmns/.NeedRebuild
    chmod 644 $PCP_VAR_DIR/pmns/.NeedRebuild

    if [ ! -f $PCP_VAR_DIR/pmns/root ]
    then
	if [ -f $PCP_VAR_DIR/pmns/root.saved ]
	then
	    # restore the previous pmns after upgrade
	    mv $PCP_VAR_DIR/pmns/root.saved $PCP_VAR_DIR/pmns/root
	else
	    # empty initial name space (prior to Rebuild)
	    echo "root {" >$PCP_VAR_DIR/pmns/root
	    echo "}" >>$PCP_VAR_DIR/pmns/root
	fi
	chmod 644 $PCP_VAR_DIR/pmns/root
    else
	# root pmns already exists, so we need to restore
	# pmcd.conf and pmcd.options if they were saved.
	for f in $PCP_PMCDCONF_PATH $PCP_PMCDOPTIONS_PATH
	do
	    if [ -f $f -a -f $f.rpmsave ]
	    then
		mv $f $f.rpmnew
		mv $f.rpmsave $f
	    fi
	done
    fi

    /sbin/chkconfig --add pcp >/dev/null 2>&1
    /sbin/chkconfig --add pmie >/dev/null 2>&1
    /sbin/chkconfig --add pmproxy >/dev/null 2>&1

    #
    # delete *.rpmorig turds that are the same as their new version
    find $PCP_VAR_DIR/config -name \*.rpmorig -print \
    | while read f
    do
	if diff $f `basename $f .rpmorig` >/dev/null 2>&1
	then
	    rm -f $f
	fi
    done
fi
exit 0

%changelog
* Fri Jul 31 2009 Mark Goodwin <mgoodwin@redhat.com> - 3.0.0-1
- initial import into Fedora
