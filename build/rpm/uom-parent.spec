Summary: Units of Measurement Project Parent POM
Name: uom-parent
Version: 1.0.2
%global buildversion 1

Release: %{buildversion}%{?dist}
License: BSD3
URL: https://github.com/unitsofmeasurement/uom-parent
Group: Development/Languages
# https://github.com/unitsofmeasurement/uom-parent/archive/%{version}.tar.gz
Source0: %{name}-%{version}.tar.gz

BuildArch: noarch
BuildRequires: maven-local
BuildRequires: maven-install-plugin

%description
Main parent POM for all Units of Measurement Maven projects.

%prep
%setup -q
%pom_remove_parent

%build
%mvn_build

%install
%mvn_install

%files -f .mfiles

%changelog
* Thu Feb 16 2017 Nathan Scott <nathans@redhat.com> - 1.0.2-1
- Update to latest upstream sources.

* Thu Oct 13 2016 Nathan Scott <nathans@redhat.com> - 1.0.1-1
- Initial version.
