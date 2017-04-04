Summary: Units of Measurement Systems (JSR 363)
Name: uom-systems
Version: 0.5
%global buildversion 2

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
BuildRequires: mvn(si.uom:si-units-java8:pom:)
BuildRequires: mvn(javax.measure:unit-api)
BuildRequires: mvn(tec.uom:uom-parent:pom:)
BuildRequires: mvn(tec.uom:uom-se:pom:)

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
%pom_disable_module common	# use only Java 8+
%pom_disable_module unicode	# use only Java 8+

%build
%mvn_build

%install
%mvn_install

%files -f .mfiles

%files javadoc -f .mfiles-javadoc

%changelog
* Thu Feb 16 2017 Nathan Scott <nathans@redhat.com> - 0.5-2
- Update to build with Java 8+ modules only.

* Fri Oct 14 2016 Nathan Scott <nathans@redhat.com> - 0.5-1
- Initial version.
