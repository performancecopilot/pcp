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
# Dockerfile to build the pcp-collector container image, layered on pcp-base.
#
FROM pcp-base:latest
MAINTAINER PCP developers <pcp@pcp.io>

#
# install pcp-collector and it's dependencies, clean the cache.
RUN yum --enablerepo=pcp-testing -y install pcp pcp-collector && yum clean all

#
# Run in the container as root - avoids PCP_USER mismatches
RUN sed -i -e 's/PCP_USER=.*$/PCP_USER=root/' -e 's/PCP_GROUP=.*$/PCP_GROUP=root/' /etc/pcp.conf

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
# extract the RUN label  and use it in a script.
LABEL RUN docker run -d --privileged --net=host --pid=host --ipc=host -v /sys:/sys:ro -v /proc:/proc:ro -v /etc/localtime:/etc/localtime:ro -v /var/lib/docker:/var/lib/docker:ro -v /run:/run -v /var/log:/var/log --name=pcp-collector pcp-collector

#
# The command to run - in this case the pmcd service script. When this command
# exits, then the container exits. This is the default command and parameters
# and can be overridden by passing additional parameters to docker run.
ENV PATH /usr/share/pcp/lib:/usr/libexec/pcp/bin:$PATH
ENTRYPOINT ["pmcd"]
CMD ["start"]
