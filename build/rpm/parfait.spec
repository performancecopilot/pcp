Summary: Parfait Java agent and libraries for Performance Co-Pilot (PCP)
Name: parfait
Version: 0.3.8
%global buildversion 1

Release: %{buildversion}%{?dist}
License: ASL2.0
URL: http://www.pcp.io
Group: Development/Languages
# https://github.com/performancecopilot/parfait/archive/master.zip
Source0: parfait.tar.gz

BuildRequires: junit
BuildRequires: testng
BuildRequires: maven-local
BuildRequires: maven-jar-plugin
BuildRequires: maven-shade-plugin
BuildRequires: maven-surefire-plugin
BuildRequires: maven-surefire-provider-testng
BuildRequires: maven-surefire-provider-junit
BuildRequires: maven-verifier-plugin
BuildRequires: maven-dependency-plugin
BuildRequires: mvn(org.aspectj:aspectjweaver)
BuildRequires: mvn(org.springframework:spring-jdbc)
BuildRequires: mvn(org.springframework:spring-core)
BuildRequires: mvn(org.springframework:spring-beans)
BuildRequires: mvn(org.springframework:spring-context)
BuildRequires: mvn(org.springframework:spring-context)
# BuildRequires: mvn(javax.measure:units-api)

%if 0%{?rhel} == 0 || 0%{?rhel} > 5
BuildArch: noarch
%endif

%description
Parfait is a Java performance monitoring library that exposes and
collects metrics through a variety of outputs.  It provides APIs
and a Java Agent for extracting performance metrics from the JVM
and other sources, in either modified or unmodified (-java-agent)
applications.  It interfaces to Performance Co-Pilot (PCP) using
the Memory Mapped Value (MMV) machinery for extremely lightweight
instrumentation.

%package javadoc
Group: Documentation
%if 0%{?rhel} == 0 || 0%{?rhel} > 5
BuildArch: noarch
%endif
Summary: Javadoc for Parfait

%description javadoc
This package contains the API documentation for Parfait.

%prep
%setup -q -c -n parfait-%{version}
%pom_disable_module parfait-cxf
%pom_disable_module parfait-benchmark
%pom_disable_module parfait-dropwizard
# %%mvn_alias io.pcp.parfait: com.custardsource.parfait:

%build
%mvn_build

%install
%mvn_install

%files -f .mfiles

%files -f .mfiles-javadoc

%changelog
* Thu Apr 07 2016 Nathan Scott <nathans@redhat.com> - 0.3.8-1
- Work-in-progress, see http://pcp.io/roadmap
