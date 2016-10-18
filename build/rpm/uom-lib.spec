Summary: Units of Measurement Libraries (JSR 363)
Name: uom-lib
Version: 1.0.1
%global buildversion 1

Release: %{buildversion}%{?dist}
License: BSD3
URL: https://github.com/unitsofmeasurement/uom-lib
Group: Development/Languages
# https://github.com/unitsofmeasurement/uom-lib/archive/%{version}.tar.gz
Source0: %{name}-%{version}.tar.gz

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
%setup -q

%build
%mvn_build

%install
%mvn_install

%files -f .mfiles

%files javadoc -f .mfiles-javadoc

%changelog
* Thu Oct 13 2016 Nathan Scott <nathans@redhat.com> - 1.0.1-1
- Initial version.
