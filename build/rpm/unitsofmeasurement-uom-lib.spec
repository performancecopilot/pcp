Summary: Units of Measurement Libraries (JSR 363)
Name: unitsofmeasurement-uom-lib
Version: 1.0.1
%global buildversion 3
%global uom_lib uom-lib-%{version}

Release: %{buildversion}%{?dist}
License: BSD
URL: https://github.com/unitsofmeasurement/uom-lib
Group: Development/Languages
Source0: https://github.com/unitsofmeasurement/uom-lib/archive/%{version}.tar.gz

BuildArch: noarch
BuildRequires: junit
BuildRequires: maven-local
BuildRequires: maven-jar-plugin
BuildRequires: maven-install-plugin
BuildRequires: maven-license-plugin
BuildRequires: maven-dependency-plugin
BuildRequires: mvn(tec.uom:uom-parent:pom:)
BuildRequires: mvn(javax.measure:unit-api)

%description
Units of Measurement Libraries - extending and complementing
JSR 363.

%package javadoc
Group: Documentation
BuildArch: noarch
Summary: Javadoc for the Units of Measurement Libraries

%description javadoc
This package contains documentation for the Units of Measurement
Libraries (JSR 363).

%prep
%setup -q -c -n unitsofmeasurement

%build
cd %{uom_lib}
%mvn_build

%install
cd %{uom_lib}
%mvn_install

%files -f %{uom_lib}/.mfiles
%doc %{uom_lib}/README.md

%files javadoc -f %{uom_lib}/.mfiles-javadoc

%changelog
* Tue Feb 28 2017 Nathan Scott <nathans@redhat.com> - 1.0.1-3
- Resolve lintian errors - source, license, documentation.

* Fri Feb 24 2017 Nathan Scott <nathans@redhat.com> - 1.0.1-2
- Add unitsofmeasurement prefix to package name.

* Thu Oct 13 2016 Nathan Scott <nathans@redhat.com> - 1.0.1-1
- Initial version.
