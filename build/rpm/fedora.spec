Name: pcp-gui
Version: 1.5.5
Release: 2%{?dist}
License: GPLv2+ and LGPLv2+ and LGPLv2+ with exceptions
URL: http://oss.sgi.com/projects/pcp

Source0: ftp://oss.sgi.com/projects/pcp/download/rpm/pcp-gui/pcp-gui-%{version}.src.tar.gz
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: autoconf, bison, flex, gawk, pcp >= 2.0, pcp-libs-devel >= 2.0, qt4-devel >= 4.2
BuildRequires: desktop-file-utils
%if (0%{?fedora} > 12 || 0%{?rhel} > 5)
BuildRequires: qt-assistant-adp-devel
%endif
Requires: pcp

Group: Applications/System
Summary: Visualization tools for the Performance Co-Pilot toolkit

%description
Visualization tools for the Performance Co-Pilot toolkit.
Performance Co-Pilot (PCP) provides a framework and services to support
system-level performance monitoring and performance management.

The PCP GUI package primarily includes visualization tools for
monitoring systems using live and archived Performance Co-Pilot
(PCP) sources.

These tools have dependencies on graphics libraries which may or
may not be installed on server machines, so PCP GUI is delivered,
managed and maintained as a separate (source and binary) package
to the core PCP infrastructure.


%package -n pcp-doc
Group: Documentation
Summary: Documentation and tutorial for the Performance Co-Pilot

%description -n pcp-doc
Documentation and tutorial for the Performance Co-Pilot
Performance Co-Pilot (PCP) provides a framework and services to support
system-level performance monitoring and performance management.

The pcp-doc package provides useful information on using and
configuring the Performance Co-Pilot (PCP) toolkit for system
level performance management.  It includes tutorials, HOWTOs,
and other detailed documentation about the internals of core
PCP utilities and daemons, and the PCP graphical tools.


%prep
%setup -q
autoconf

%build
%configure
make %{?_smp_mflags}


%install
rm -rf $RPM_BUILD_ROOT
export DIST_ROOT=$RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
rm -rf $RPM_BUILD_ROOT/usr/share/doc/pcp-gui
desktop-file-validate $RPM_BUILD_ROOT/%{_datadir}/applications/pmchart.desktop


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README doc/CHANGES doc/COPYING
%{_bindir}/*
%{_libexecdir}/*
%{_localstatedir}/lib/pcp
%{_mandir}/man1/*
%{_datadir}/pcp
%{_datadir}/pixmaps/*
%{_datadir}/applications/pmchart.desktop

%files -n pcp-doc
%defattr(-,root,root,-)
%{_datadir}/doc/pcp-doc


%changelog
* Fri Jul 20 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.5.5-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Fri Mar 23 2012 Mark Goodwin <mgoodwin@redhat.com> - 1.5.5-1
- update to latest sources (rolls in desktop patch too)

* Fri Jan 13 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.5.2-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_17_Mass_Rebuild

* Fri Dec 02 2011 Mark Goodwin <mgoodwin@redhat.com> - 1.5.2-2
- patched pmchart.desktop, needed for RHEL builds

* Thu Dec 01 2011 Mark Goodwin <mgoodwin@redhat.com> - 1.5.2-1
- fixed License and assorted minor rpmlint issues following review

* Mon Sep 19 2011 Harshula Jayasuriya <harshula@redhat.com> - 1.5.1-1
- Initial Fedora release
