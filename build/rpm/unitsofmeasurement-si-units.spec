Summary: International System of Units (JSR 363)
Name: unitsofmeasurement-si-units
Version: 0.6.2
%global buildversion 2
%global si_units si-units-%{version}

Release: %{buildversion}%{?dist}
License: BSD
URL: https://github.com/unitsofmeasurement/si-units
Group: Development/Languages
Source0: https://github.com/unitsofmeasurement/si-units/archive/%{version}.tar.gz
Patch1: si-units-remove-plurals.patch
Patch2: si-units-remove-Priority.patch

BuildArch: noarch
BuildRequires: junit
BuildRequires: maven-local
BuildRequires: maven-jar-plugin
BuildRequires: maven-install-plugin
BuildRequires: maven-license-plugin
BuildRequires: maven-dependency-plugin
BuildRequires: mvn(org.jacoco:jacoco-maven-plugin)
BuildRequires: mvn(javax.measure:unit-api)
BuildRequires: mvn(tec.uom:uom-parent:pom:)
BuildRequires: mvn(tec.uom:uom-se:pom:)

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
%setup -q -c -n unitsofmeasurement
cd %{si_units}
%patch1 -p0
%patch2 -p0
%pom_disable_module units	# use only Java 8+

%build
cd %{si_units}
%mvn_build

%install
cd %{si_units}
%mvn_install

%files -f %{si_units}/.mfiles
%doc %{si_units}/README.md

%files javadoc -f %{si_units}/.mfiles-javadoc

%changelog
* Tue Feb 28 2017 Nathan Scott <nathans@redhat.com> - 0.6.2-2
- Resolve minor lintian errors - source, license, documentation.

* Fri Feb 24 2017 Nathan Scott <nathans@redhat.com> - 0.6.2-1
- Switch to enabling the Java 8+ maven modules only now.
- Add unitsofmeasurement prefix to package name.
- Update to latest upstream sources.

* Thu Oct 13 2016 Nathan Scott <nathans@redhat.com> - 0.6-1
- Initial version.
