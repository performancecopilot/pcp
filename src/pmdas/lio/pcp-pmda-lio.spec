Name:		pcp-pmda-lio
Version:	1.0
Release:	1%{?dist}
Summary:	Performance Co-Pilot PMDA extracting performance data from LIO
Group:		Applications/System
License:	GPLv2+

URL:		https://github.com/pcuzner/pcp-pmda-lio
Source0:	https://github.com/pcuzner/%{name}/archive/%{version}/%{name}-%{version}.tar.gz
BuildArch:  noarch

%global pcp_pmdas_dir %{_localstatedir}/lib/pcp/pmdas
%global pcp_etc_dir %{_sysconfdir}

Requires: python-rtslib >= 2.1
Requires: python-pcp

%description
This package provides a PMDA to gather performance metrics from the kernels
iscsi target interface (LIO). The metrics are stored by LIO within the
kernel's configfs filesystem. The PMDA provides per LUN level stats, and a
summary instance per iSCSI target, which aggregates all LUN metrics within the
target.

NB. With SELINUX in enforcing mode, the pmda may not be able to access the 
configfs directory. In this scenario, build and activate a local SELINUX policy
to grant the pmda access to configfs

%prep
%setup -q

%build
# Nothing to build - package just deploys scripts

%install
mkdir -p %{buildroot}%{pcp_pmdas_dir}/lio

install -m 0755 ./lio/Install %{buildroot}%{pcp_pmdas_dir}/lio
install -m 0755 ./lio/Remove %{buildroot}%{pcp_pmdas_dir}/lio
install -m 0755 ./lio/pmdalio.python %{buildroot}/%{pcp_pmdas_dir}/lio

%post
PCP_PMDAS_DIR=%{pcp_pmdas_dir}
PCP_ETC_DIR=%{pcp_etc_dir}
if [ -f "$PCP_ETC_DIR/pcp/pmcd/pmcd.conf" ] && [ -f "/var/run/pcp/pmcd.pid" ] ; then 
  cd $PCP_PMDAS_DIR/lio/ && ./Install >/dev/null 2>&1
else
  echo "--> pmcd not active - please run the PMDAs 'Install' script manually"
fi


%preun
PCP_PMDAS_DIR=%{pcp_pmdas_dir}
PCP_ETC_DIR=%{pcp_etc_dir}
if [ -f "$PCP_ETC_DIR/pcp/pmcd/pmcd.conf" ] && [ -f "$PCP_PMDAS_DIR/lio/domain.h" ] ; then 
  cd $PCP_PMDAS_DIR/lio/ && ./Remove >/dev/null 2>&1
fi


%files
%{pcp_pmdas_dir}/lio/

%changelog
* Thu Jan 5 2017 Paul Cuzner <pcuzner@redhat.com> 1.0-1
- initial rpm packaging
