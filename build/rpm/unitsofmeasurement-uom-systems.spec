Summary: Units of Measurement Systems (JSR 363)
Name: unitsofmeasurement-uom-systems
Version: 0.5
%global buildversion 3
%global uom_systems uom-systems-%{version}

Release: %{buildversion}%{?dist}
License: BSD
URL: https://github.com/unitsofmeasurement/uom-systems
Group: Development/Languages
Source0: https://github.com/unitsofmeasurement/uom-systems/archive/%{version}.tar.gz
Patch1: uom-systems-remove-ROENTGEN.patch
Patch2: uom-systems-remove-Priority.patch
Patch3: uom-systems-AbstractONE.patch
Patch4: uom-systems-remove-plurals.patch

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
%setup -q -c -n unitsofmeasurement
cd %{uom_systems}
%patch1 -p0
%patch2 -p0
%patch3 -p0
%patch4 -p0
%pom_disable_module common	# use only Java 8+
%pom_disable_module unicode	# use only Java 8+

%build
cd %{uom_systems}
%mvn_build

%install
cd %{uom_systems}
%mvn_install

%files -f %{uom_systems}/.mfiles
%doc %{uom_systems}/README.md

%files javadoc -f %{uom_systems}/.mfiles-javadoc

%changelog
* Tue Feb 28 2017 Nathan Scott <nathans@redhat.com> - 0.5-3
- Resolve lintian errors - source, license, documentation.

* Fri Feb 24 2017 Nathan Scott <nathans@redhat.com> - 0.5-2
- Add unitsofmeasurement prefix to package name.

* Fri Oct 14 2016 Nathan Scott <nathans@redhat.com> - 0.5-1
- Initial version.
