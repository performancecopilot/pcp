Summary: Parfait Java agent and libraries for Performance Co-Pilot (PCP)
Name: parfait
Version: 0.4.0
%global buildversion 1

Release: %{buildversion}%{?dist}
License: ASL2.0
URL: https://github.com/performancecopilot/parfait
Group: Development/Languages
# https://github.com/performancecopilot/parfait/archive/parfait-X.Y.Z.tar.gz
Source0: %{name}-%{version}.tar.gz

BuildArch: noarch
BuildRequires: junit
BuildRequires: testng
BuildRequires: maven-local
BuildRequires: maven-jar-plugin
BuildRequires: maven-shade-plugin
BuildRequires: maven-install-plugin
BuildRequires: maven-surefire-plugin
BuildRequires: maven-surefire-provider-testng
BuildRequires: maven-surefire-provider-junit
BuildRequires: maven-dependency-plugin
BuildRequires: maven-verifier-plugin
BuildRequires: mvn(net.jcip:jcip-annotations)
BuildRequires: mvn(org.apache.maven.wagon:wagon-ftp)
BuildRequires: mvn(org.aspectj:aspectjweaver)
BuildRequires: mvn(org.hsqldb:hsqldb)
BuildRequires: mvn(org.mockito:mockito-core)
BuildRequires: mvn(org.springframework:spring-jdbc)
BuildRequires: mvn(org.springframework:spring-core)
BuildRequires: mvn(org.springframework:spring-beans)
BuildRequires: mvn(org.springframework:spring-context)
BuildRequires: mvn(org.springframework:spring-test)
BuildRequires: mvn(com.codahale.metrics:metrics-core)
BuildRequires: mvn(systems.uom:systems-common:pom:)
BuildRequires: mvn(javax.measure:unit-api)
BuildRequires: mvn(tec.units:unit-ri)

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
BuildArch: noarch
Summary: Javadoc for Parfait

%description javadoc
This package contains the API documentation for Parfait.

%prep
%setup -q
%pom_disable_module parfait-benchmark
%pom_disable_module parfait-cxf
%pom_disable_module parfait-jdbc	# need hsqldb update?
%pom_disable_module parfait-dropwizard	# need metrics update

%build
%mvn_build

%install
%mvn_install

%files -f .mfiles

%files javadoc -f .mfiles-javadoc

%changelog
* Wed Oct 12 2016 Nathan Scott <nathans@redhat.com> - 0.4.0-1
- Work-in-progress, see http://pcp.io/roadmap
