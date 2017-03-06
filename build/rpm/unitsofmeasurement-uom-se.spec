Summary: Unit Standard (JSR 363) implementation for Java SE 8 and above
Name: unitsofmeasurement-uom-se
Version: 1.0.2
%global buildversion 2
%global uom_se uom-se-%{version}

Release: %{buildversion}%{?dist}
License: BSD
URL: https://github.com/unitsofmeasurement/uom-se
Group: Development/Languages
Source0: https://github.com/unitsofmeasurement/uom-se/archive/%{version}.tar.gz

BuildArch: noarch
BuildRequires: junit
BuildRequires: maven-local
BuildRequires: maven-jar-plugin
BuildRequires: maven-install-plugin
BuildRequires: maven-license-plugin
BuildRequires: maven-dependency-plugin
BuildRequires: mvn(org.hamcrest:hamcrest-all)
BuildRequires: mvn(org.jacoco:jacoco-maven-plugin)
BuildRequires: mvn(javax.annotation:javax.annotation-api:pom:)
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
%setup -q -c -n unitsofmeasurement
cd %{uom_se}
%pom_remove_plugin com.mycila:license-maven-plugin
%pom_remove_plugin net.revelc.code:formatter-maven-plugin

%build
cd %{uom_se}
%mvn_build

%install
cd %{uom_se}
%mvn_install

%files -f %{uom_se}/.mfiles
%doc %{uom_se}/README.md

%files javadoc -f %{uom_se}/.mfiles-javadoc

%changelog
* Tue Feb 28 2017 Nathan Scott <nathans@redhat.com> - 1.0.2-2
- Resolve lintian errors - source, license, documentation.

* Fri Feb 24 2017 Nathan Scott <nathans@redhat.com> - 1.0.2-1
- Add unitsofmeasurement prefix to package name.
- Update to latest upstream sources.

* Fri Nov 25 2016 Nathan Scott <nathans@redhat.com> - 1.0.1-1
- Initial version.
