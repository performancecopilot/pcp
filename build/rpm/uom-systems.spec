Summary: Units of Measurement Systems (JSR 363)
Name: uom-systems
Version: 0.5
%global buildversion 1

Release: %{buildversion}%{?dist}
License: BSD3
URL: https://github.com/unitsofmeasurement/uom-systems
Group: Development/Languages
# https://github.com/unitsofmeasurement/uom-systems/archive/%{version}.tar.gz
Source0: %{name}-%{version}.tar.gz

BuildArch: noarch
BuildRequires: junit
BuildRequires: maven-local
BuildRequires: maven-jar-plugin
BuildRequires: maven-install-plugin
BuildRequires: maven-license-plugin
BuildRequires: maven-dependency-plugin
BuildRequires: mvn(org.jacoco:jacoco-maven-plugin)
BuildRequires: mvn(si.uom:si-parent:pom:)
BuildRequires: mvn(tec.uom:uom-parent:pom:)
BuildRequires: mvn(tec.units:unit-ri)
BuildRequires: mvn(javax.measure:unit-api)

%description
Units of Measurement Systems - modules for JSR 363.

%package javadoc
Group: Documentation
BuildArch: noarch
Summary: Javadoc for the Units of Measurement Systems

%description javadoc
This package contains documentation for the Units of Measurement
Systems (JSR 363).

%prep
%setup -q
%pom_disable_module common-java8
%pom_disable_module unicode-java8

%build
%mvn_build

%install
%mvn_install

%files -f .mfiles

%files javadoc -f .mfiles-javadoc

%changelog
* Fri Oct 14 2016 Nathan Scott <nathans@redhat.com> - 0.5-1
- Initial version.
