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
# Dockerfile to build the pcp-pmlogger container image. See docker-build(1).
# e.g. docker build -t pcp-pmlogger .
#
FROM fedora:latest
MAINTAINER PCP developers <pcp@pcp.io>

# update the Fedora base image and clean the yum cache
# This is disabled during development.
# RUN yum -y update && yum clean all

# Set up a repo for the packages to be installed. This is only for use
# during development. Production builds would use the Fedora packages.
RUN mkdir -p /tmp/RPMS/repodata; \
echo -e '[pcp-testing]\nname=PCP Test Yum Repository\n\
baseurl=file:///tmp/RPMS\ngpgcheck = 0\nenabled = 0'\
>/etc/yum.repos.d/pcp-testing.repo
COPY RPMS/*.rpm /tmp/RPMS/
COPY RPMS/repodata/* /tmp/RPMS/repodata/

#
# install pcp and it's dependencies, clean the cache.
RUN yum --enablerepo=pcp-testing -y install pcp && yum clean all

#
# during development only: remove the test repo
RUN rm -rf /tmp/RPMS

#
# Run in the container as root - avoid PCP_USER mismatches
RUN sed -i -e 's/PCP_USER=.*$/PCP_USER=root/' /etc/pcp.conf

#
# Expose pmloggers/pmlc ports on the host interfaces.
EXPOSE 4330

#
# denote this as a container environment, for rc scripts
ENV PCP_CONTAINER_IMAGE pcp-pmlogger
ENV NAME pcp-pmlogger
ENV IMAGE pcp-pmlogger

#
# The RUN label is for the atomic command, e.g. atomic run pcp-pmlogger
LABEL RUN docker run -d --privileged --net=host -v /sys:/sys:ro -v /proc:/proc:ro -v /etc/cron.d:/etc/cron.d -v /etc/localtime:/etc/localtime:ro -v /var/lib/docker:/var/lib/docker:ro -v /run:/run -v /etc/pcp:/etc/pcp -v /var/lib/pcp:/var/lib/pcp -v /var/log/pcp:/var/log/pcp --name=pcp-pmlogger pcp-pmlogger

#
# The command to run - in this case the pmlogger service script.
# When this command exits, then the container exits.
ENV PATH /usr/share/pcp/lib:$PATH
ENTRYPOINT ["pmlogger"]
CMD ["start"]
