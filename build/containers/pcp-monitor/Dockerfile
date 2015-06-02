# Copyright (c) 2015 Red Hat.
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# Dockerfile to build the pcp-monitor container image, layered on pcp-base.
#
FROM pcp-base:latest
MAINTAINER PCP developers <pcp@pcp.io>

# Set up a repo for the packages to be installed. This is only for use
# during development. Production builds would use the Fedora packages.
# The build expects build/containers/RPMS to have the PCP RPMS to be
# installed in the container (and createrepo build/containers/RPMS has
been run to set up the yum repo).
RUN mkdir -p /tmp/RPMS/repodata; \
echo -e '[pcp-testing]\nname=PCP Test Yum Repository\n\
baseurl=file:///tmp/RPMS\ngpgcheck = 0\nenabled = 0'\
>/etc/yum.repos.d/pcp-testing.repo
COPY RPMS/*.rpm /tmp/RPMS/
COPY RPMS/repodata/* /tmp/RPMS/repodata/

#
# install pcp and it's dependencies, clean the cache.
RUN yum --enablerepo=pcp-testing -y install pcp pcp-monitor && yum clean all

#
# during development only: remove the test repo
RUN rm -rf /tmp/RPMS

#
# Run in the container as root - avoid PCP_USER mismatch with the host
RUN sed -i -e 's/PCP_USER=.*$/PCP_USER=root/' -e 's/PCP_GROUP=.*$/PCP_GROUP=root/' /etc/pcp.conf

#
# Since this is an interactive container, set the bash prompt
RUN echo export PS1='"[pcp-monitor] "' >> /root/.bash_profile

#
# denote this as a container environment, for rc scripts
ENV PCP_CONTAINER_IMAGE pcp-monitor
ENV NAME pcp-monitor
ENV IMAGE pcp-monitor

#
# The RUN label used by the atomic command, e.g. atomic run pcp-monitor
# The pcp-monitor is an interactive container - start command is a shell.
LABEL RUN docker run -it --privileged --net=host --pid=host --ipc=host -v /sys:/sys:ro -v /proc:/proc:ro -v /etc/localtime:/etc/localtime:ro -v /var/lib/docker:/var/lib/docker:ro -v /run:/run -v /var/log:/var/log --name=pcp-monitor pcp-monitor

# The command to run. The LABEL defined above specifies -i -t (interactive,
# with terminal). An optional argument to the docker run command may be used
# to start a pcp monitoring tool rather than a shell. When the command exits,
# the container exits.
CMD ["/bin/bash", "-l"]
