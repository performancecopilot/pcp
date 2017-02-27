Summary: Unit Standard (JSR 363) Implementation for Java SE 8 and above
Name: uom-se
Version: 1.0.3
%global buildversion 1

Release: %{buildversion}%{?dist}
License: BSD3
URL: https://github.com/unitsofmeasurement/uom-se
Group: Development/Languages
# https://github.com/unitsofmeasurement/uom-se/archive/%{version}.tar.gz
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
JSR 363 Implementation got Java SE 8 and above.

JDK Integration of Unit-API / JSR 363.  This implementation aims at
Java SE 8 and above, allowing the use of new features like Lambdas
together with Units of Measurement API.

%package javadoc
Group: Documentation
BuildArch: noarch
Summary: Javadoc for the Units Standard (JSR 363) Java SE 8 Implementation

%description javadoc
This package contains documentation for the Units Standard (JSR 363)
Java SE 8 Implementation.

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
* Thu Feb 16 2017 Nathan Scott <nathans@redhat.com> - 1.0.3-1
- Update to latest upstream sources.

* Fri Nov 25 2016 Nathan Scott <nathans@redhat.com> - 1.0.1-1
- Initial version.
