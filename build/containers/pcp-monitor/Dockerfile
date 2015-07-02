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
MAINTAINER PCP Development Team <pcp@oss.sgi.com>

#
# install pcp and it's dependencies, clean the cache.
RUN dnf --enablerepo=pcp-devel -y install pcp pcp-monitor && dnf clean all

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
