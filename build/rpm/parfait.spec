Summary: Parfait Java libraries for Performance Co-Pilot (PCP)
Name: parfait
Version: 0.4.0
%global buildversion 2

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
for extracting performance metrics from the JVM and other sources.
It interfaces to Performance Co-Pilot (PCP) using the Memory Mapped
Value (MMV) machinery for extremely lightweight instrumentation.

%package agent
Group: Applications/System
BuildArch: noarch
Summary: Parfait Java Agent for Performance Co-Pilot (PCP)
Requires: java-headless >= 1:1.7

%description agent
This package contains the Parfait Agent for instrumenting Java
applications.  The agent can extract live performance metrics
from the JVM and other sources, in unmodified applications (via
the -java-agent java command line option).  It interfaces to
Performance Co-Pilot (PCP) using the Memory Mapped Value (MMV)
machinery for extremely lightweight instrumentation.

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
# re-instate not-shaded, not-with-all-dependencies agent jar
pushd parfait-agent/target
mv original-parfait-agent.jar parfait-agent.jar
popd

%install
%mvn_install
# special install of shaded, with-all-dependencies agent jar
pushd parfait-agent/target
install -m 644 parfait-agent-jar-with-dependencies.jar \
               %{buildroot}%{_javadir}/parfait/parfait.jar
popd

%files -f .mfiles

%files javadoc -f .mfiles-javadoc

%files agent
%dir %{_javadir}/parfait
%{_javadir}/parfait/parfait.jar

%changelog
* Thu Oct 20 2016 Nathan Scott <nathans@redhat.com> - 0.4.0-2
- Addition of the standalone parfait-agent package.
- Add in proxy mode from upstream parfait code too.

* Wed Oct 12 2016 Nathan Scott <nathans@redhat.com> - 0.4.0-1
- Initial version.
