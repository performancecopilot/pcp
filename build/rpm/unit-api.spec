Summary: Units of Measurement API (JSR 363)
Name: unit-api
Version: 1.0
%global buildversion 1

Release: %{buildversion}%{?dist}
License: BSD3
URL: https://github.com/unitsofmeasurement/unit-api
Group: Development/Languages
# https://github.com/unitsofmeasurement/unit-api/archive/%{version}.tar.gz
Source0: %{name}-%{version}.tar.gz

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
%setup -q
%pom_remove_parent
%pom_remove_plugin com.mycila:license-maven-plugin
%pom_remove_plugin net.revelc.code:formatter-maven-plugin

%build
%mvn_build

%install
%mvn_install

%files -f .mfiles

%files javadoc -f .mfiles-javadoc

%changelog
* Thu Oct 13 2016 Nathan Scott <nathans@redhat.com> - 1.0-1
- Initial version.
