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
# Dockerfile to build the pcp-testsuite container image. For now, this
# just installs everything, and the default CMD is a bash shell. For
# future work, QA will need to know how to start/stop the pcp-collector
# and pcp-pmlogger containers.
#
FROM pcp-base:latest
MAINTAINER PCP Development Team <pcp@oss.sgi.com>

#
# install everything and dependencies, clean the cache.
RUN dnf --enablerepo=pcp-devel -y install pcp pcp-collector pcp-monitor pcp-testsuite && dnf clean all

#
# Disable service advertising - no avahi daemon in the container
# (dodges warnings from pmcd attempting to connect during startup)
RUN . /etc/pcp.conf && echo "-A" >> $PCP_PMCDOPTIONS_PATH

#
# Expose pmcd's main port on the host interfaces.
EXPOSE 44321

#
# denote this as a container environment, for rc scripts
ENV PCP_CONTAINER_IMAGE pcp-testsuite
ENV NAME pcp-testsuite
ENV IMAGE pcp-testsuite

#
# The RUN label is used by 'atomic' command, e.g. atomic run pcp-testsuite
# Other platforms without the 'atomic' command can use docker inspect to
# extract it and use it in a script.
LABEL RUN docker run -it --privileged --net=host --pid=host --ipc=host -v /sys:/sys:ro -v /proc:/proc:ro -v /etc/localtime:/etc/localtime:ro -v /var/lib/docker:/var/lib/docker:ro -v /run:/run -v /var/log:/var/log --name=pcp-testsuite pcp-testsuite

#
# The command to run - in this case the pmcd service script. When this command
# exits, then the container exits. This is the default command and parameters
# and can be overridden by passing additional parameters to docker run.
# CMD ["/usr/share/pcp/lib/pmcd", "start"]
CMD ["/bin/bash", "-l"]
