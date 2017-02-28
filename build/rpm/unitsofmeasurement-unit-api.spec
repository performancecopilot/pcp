Summary: Units of Measurement API (JSR 363)
Name: unitsofmeasurement-unit-api
Version: 1.0
%global buildversion 3
%global unit_api unit-api-%{version}

Release: %{buildversion}%{?dist}
License: BSD
URL: https://github.com/unitsofmeasurement/unit-api
Group: Development/Languages
Source0: https://github.com/unitsofmeasurement/unit-api/archive/%{version}.tar.gz

BuildArch: noarch
BuildRequires: junit
BuildRequires: maven-local
BuildRequires: maven-jar-plugin
BuildRequires: maven-install-plugin
BuildRequires: maven-surefire-plugin
BuildRequires: maven-surefire-provider-junit
BuildRequires: maven-verifier-plugin
BuildRequires: maven-dependency-plugin
BuildRequires: mvn(org.jacoco:jacoco-maven-plugin)

%description
The Unit of Measurement API provides a set of Java language
programming interfaces for handling units and quantities.

The interfaces provide a layer which separates client code, that
would call the API, from library code, which implements the API.

%package javadoc
Group: Documentation
BuildArch: noarch
Summary: Javadoc for the Units of Measurement API

%description javadoc
This package contains documentation for the Units of Measurement
API (JSR 363).

%prep
%setup -q -c -n unitsofmeasurement
cd %{unit_api}
%pom_remove_parent
%pom_remove_plugin com.mycila:license-maven-plugin
%pom_remove_plugin net.revelc.code:formatter-maven-plugin

%build
cd %{unit_api}
%mvn_build

%install
cd %{unit_api}
%mvn_install

%files -f %{unit_api}/.mfiles
%doc %{unit_api}/README.md

%files javadoc -f %{unit_api}/.mfiles-javadoc

%changelog
* Tue Feb 28 2017 Nathan Scott <nathans@redhat.com> - 1.0-3
- Resolve lintian errors - source, license, documentation.

* Fri Feb 24 2017 Nathan Scott <nathans@redhat.com> - 1.0-2
- Add unitsofmeasurement prefix to package name.

* Thu Oct 13 2016 Nathan Scott <nathans@redhat.com> - 1.0-1
- Initial version.
