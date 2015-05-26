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
# Dockerfile to build the pcp-collector container image. See docker-build(1).
# We have to turn the cache off. e.g. docker build --no-cache -t pcp-collector
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
RUN yum --enablerepo=pcp-testing -y install pcp pcp-collector && yum clean all

# during development only: remove the test repo
RUN rm -rf /tmp/RPMS

#
# Run in the container as root - avoids PCP_USER mismatches
RUN sed -i -e 's/PCP_USER=.*$/PCP_USER=root/' /etc/pcp.conf

#
# Disable service advertising - no avahi daemon in the container
# (dodges warnings from pmcd attempting to connect during startup)
RUN . /etc/pcp.conf && echo "-A" >> $PCP_PMCDOPTIONS_PATH

#
# Expose pmcd's main port on the host interfaces.
EXPOSE 44321

#
# denote this as a container environment, for rc scripts
ENV PCP_CONTAINER_IMAGE pcp-collector
ENV NAME pcp-collector
ENV IMAGE pcp-collector

#
# The RUN label is used by 'atomic' command, e.g. atomic run pcp-collector
# Other platforms without the 'atomic' command can use docker inspect to
# extract it and use it in a script.
LABEL RUN docker run -d --privileged --net=host -v /sys:/sys:ro -v /proc:/proc:ro -v /etc/cron.d:/etc/cron.d -v /etc/localtime:/etc/localtime:ro -v /var/lib/docker:/var/lib/docker:ro -v /run:/run -v /etc/pcp:/etc/pcp -v /var/lib/pcp:/var/lib/pcp -v /var/log/pcp:/var/log/pcp --name=pcp-collector pcp-collector

#
# The command to run - in this case the pmcd service script.
# when this command exits, then the container exits.
ENV PATH /usr/share/pcp/lib:$PATH
ENTRYPOINT ["pmcd"]
CMD ["start"]

# Example command to deploy this container on a RHEL Atomic server:
# atomic run pcp-collector  (this uses the LABEL defined above).
#
# Example command to deploy this container on a regular docker server :
#   docker run -d \
#   --privileged --net=host \
#   -v /sys:/sys:ro \
#   -v /proc:/proc:ro \
#   -v /etc/cron.d:/etc/cron.d \
#   -v /etc/localtime:/etc/localtime:ro \
#   -v /var/lib/docker:/var/lib/docker:ro \
#   -v /etc/pcp:/etc/pcp \
#   -v /var/run/pcp:/var/run/pcp \
#   -v /var/log/pcp:/var/log/pcp \
#   IMAGE-ID
#
# -d means to detach and run in the background. Use -it (instead of -d) if you
#    need a pty and "bash" as the command to run after IMAGE-ID on the command
#    line - useful for debugging.
#
# --privileged and --net=host  allow pmcd access to host network/ipc namespaces
#
# -v /proc:/proc:ro -v /sys:/sys:ro  allows PMDAs to see the global /proc and /sys
# -v /var/lib/docker:/var/lib/docker:ro  so pmdaroot can inspect /var/lib/docker
# -v /var/run/pcp:/var/run/pcp  for pid files and unix domain sockets
# -v /var/log/pcp:/var/log/pcp  for pmcd and PMDA log files
# -v /etc/cron.d:/etc/cron.d  PCP cron services
# -v /etc/localtime:/etc/localtime:ro  for correct local timezone reporting
#
# IMAGE-ID is the pcp-collector image id. Use "docker images" to list images.
#
# After IMAGE-ID you can optionally specify an alternate command to run, e.g. /bin/bash
# (which would typically be used with -it instead of -d) to run a shell instead of the
# command specified in the CMD line of the Dockerfile. In this case pmcd would not be
# started - you'd have to start it manually in the background.
