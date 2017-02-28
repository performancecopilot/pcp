Summary: Units of Measurement Project Parent POM
Name: unitsofmeasurement-uom-parent
Version: 1.0.2
%global buildversion 2
%global uom_parent uom-parent-%{version}

Release: %{buildversion}%{?dist}
License: BSD
URL: https://github.com/unitsofmeasurement/uom-parent
Group: Development/Languages
Source0: https://github.com/unitsofmeasurement/uom-parent/archive/%{version}.tar.gz

BuildArch: noarch
BuildRequires: maven-local
BuildRequires: maven-install-plugin

%description
Main parent POM for all Units of Measurement Maven projects.

%prep
%setup -q -c -n unitsofmeasurement
cd %{uom_parent}
%pom_remove_parent

%build
cd %{uom_parent}
%mvn_build

%install
cd %{uom_parent}
%mvn_install

%files -f %{uom_parent}/.mfiles
%doc %{uom_parent}/README.md

%changelog
* Tue Feb 28 2017 Nathan Scott <nathans@redhat.com> - 1.0.2-2
- Resolve lintian errors - source, license, documentation.

* Fri Feb 17 2017 Nathan Scott <nathans@redhat.com> - 1.0.2-1
- Add unitsofmeasurement prefix to package name.
- Update to latest upstream sources.

* Thu Oct 13 2016 Nathan Scott <nathans@redhat.com> - 1.0.1-1
- Initial version.
