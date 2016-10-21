Summary: International System of Units (JSR 363)
Name: si-units
Version: 0.6
%global buildversion 1

Release: %{buildversion}%{?dist}
License: BSD3
URL: https://github.com/unitsofmeasurement/si-units
Group: Development/Languages
# https://github.com/unitsofmeasurement/si-units/archive/%{version}.tar.gz
Source0: %{name}-%{version}.tar.gz

BuildArch: noarch
BuildRequires: junit
BuildRequires: maven-local
BuildRequires: maven-jar-plugin
BuildRequires: maven-install-plugin
BuildRequires: maven-license-plugin
BuildRequires: maven-dependency-plugin
BuildRequires: mvn(org.jacoco:jacoco-maven-plugin)
BuildRequires: mvn(tec.units:unit-ri)
BuildRequires: mvn(tec.uom:uom-parent:pom:)
BuildRequires: mvn(javax.measure:unit-api)

%description
A library of SI quantities and unit types (JSR 363).

%package javadoc
Group: Documentation
BuildArch: noarch
Summary: Javadoc for the library of SI quantities and unit types

%description javadoc
This package contains documentation for the International System
of Units - a library of SI quantities and unit types (JSR 363).

%prep
%setup -q
%pom_disable_module units-java8

%build
%mvn_build

%install
%mvn_install

%files -f .mfiles

%files javadoc -f .mfiles-javadoc

%changelog
* Thu Oct 13 2016 Nathan Scott <nathans@redhat.com> - 0.6-1
- Initial version.
