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
# Dockerfile to build the pcp-pmlogger container image, layered on pcp-base.
#
FROM pcp-base:latest
MAINTAINER PCP Development Team <pcp@oss.sgi.com>

#
# install pcp and it's dependencies, clean the cache.
RUN dnf --enablerepo=pcp-devel -y install pcp && dnf clean all

#
# Expose pmloggers/pmlc ports on the host interfaces.
EXPOSE 4330

#
# pmlogger start script complains if pmlogger.service is not enabled.
RUN systemctl enable pmlogger.service

#
# Remove the crontab entry - we install our own container specific version
# on startup into the /etc/cron.d bind mount and use the host cron service.
# See the pmlogger rc script.
RUN rm -f /etc/cron.d/pcp-pmlogger

#
# denote this as a container environment, for rc scripts
ENV PCP_CONTAINER_IMAGE pcp-pmlogger
ENV NAME pcp-pmlogger
ENV IMAGE pcp-pmlogger

#
# The RUN label is for the atomic command, e.g. atomic run pcp-pmlogger
LABEL RUN docker run -d --privileged --net=host --pid=host --ipc=host -v /sys:/sys:ro -v /proc:/proc:ro -v /etc/localtime:/etc/localtime:ro -v /var/lib/docker:/var/lib/docker:ro -v /run:/run -v /var/log:/var/log -v /etc/cron.d:/etc/cron.d --name=pcp-pmlogger pcp-pmlogger

#
# The command to run - in this case the pmlogger service script.
# When this command exits, then the container exits.
ENV PATH /usr/share/pcp/lib:/usr/libexec/pcp/bin:$PATH
ENTRYPOINT ["pmlogger"]
CMD ["start"]
