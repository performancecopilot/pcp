FROM registry.access.redhat.com/ubi8/ubi:8.4
RUN echo $'[performancecopilot]\n\
name=Performance Co-Pilot\n\
baseurl=https://performancecopilot.jfrog.io/artifactory/pcp-rpm-release/centos/$releasever/$basearch\n\
enabled=1\n\
gpgcheck=0\n\
gpgkey=https://performancecopilot.jfrog.io/artifactory/pcp-rpm-release/centos/$releasever/$basearch/repodata/repomd.xml.key\n\
repo_gpgcheck=1\n'\
>> /etc/yum.repos.d/performancecopilot.repo
RUN dnf update -y
RUN dnf install -y pcp python3-pcp python3-pcp-debuginfo procps-ng
RUN pip3 install hdbcli
RUN systemctl enable pmcd
CMD [ "/sbin/init" ]