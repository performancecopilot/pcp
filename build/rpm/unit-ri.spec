Summary: Unit Standard (JSR 363) Reference Implementation
Name: unit-ri
Version: 1.0.1
%global buildversion 1

Release: %{buildversion}%{?dist}
License: BSD3
URL: https://github.com/unitsofmeasurement/unit-api
Group: Development/Languages
# https://github.com/unitsofmeasurement/unit-ri/archive/%{version}.tar.gz
Source0: %{name}-%{version}.tar.gz

BuildArch: noarch
BuildRequires: junit
BuildRequires: maven-local
BuildRequires: maven-jar-plugin
BuildRequires: maven-install-plugin
BuildRequires: maven-license-plugin
BuildRequires: maven-dependency-plugin
BuildRequires: mvn(org.hamcrest:hamcrest-all)
BuildRequires: mvn(org.jacoco:jacoco-maven-plugin)
BuildRequires: mvn(javax.measure:unit-api)
BuildRequires: mvn(tec.uom:uom-parent:pom:)
BuildRequires: mvn(tec.uom.lib:uom-lib:pom:)

%description
JSR 363 Reference Implementation (RI).

The RI aims at Java Embedded, both SE 6/7 or above and Java ME 8 Embedded.

%package javadoc
Group: Documentation
BuildArch: noarch
Summary: Javadoc for the Units Standard (JSR 363) Reference Implementation

%description javadoc
This package contains documentation for the Units Standard (JSR 363)
Reference Implementation.

%prep
%setup -q
%pom_remove_plugin com.mycila:license-maven-plugin
%pom_remove_plugin net.revelc.code:formatter-maven-plugin

%build
%mvn_build

%install
%mvn_install

%files -f .mfiles

%files javadoc -f .mfiles-javadoc

%changelog
* Thu Oct 13 2016 Nathan Scott <nathans@redhat.com> - 1.0.1-1
- Initial version.
