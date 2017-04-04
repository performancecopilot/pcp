# Copyright (c) 2016 Red Hat.
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
# Dockerfile to build the pcp-pmwebd container image, layered on pcp-base.
#
FROM pcp-base:latest
MAINTAINER PCP Development Team <pcp@groups.io>

#
# install pcp-pmwebd and it's dependencies, clean the cache.
RUN dnf -y install pcp pcp-webapi pcp-webjs && dnf clean all

#
# Expose pmwebd's main port on the host interfaces.
EXPOSE 44323

#
# Disable service advertising with -S option to the daemon
RUN . /etc/pcp.conf && echo OPTIONS=\"\$OPTIONS -S\" >> $PCP_PMWEBDOPTIONS_PATH

#
# denote this as a container environment, for rc scripts
ENV PCP_CONTAINER_IMAGE pcp-pmwebd
ENV NAME pcp-pmwebd
ENV IMAGE pcp-pmwebd

#
# The RUN label is used by 'atomic' command, e.g. atomic run pcp-pmwebd
# Other platforms without the 'atomic' command can use docker inspect to
# extract the RUN label  and use it in a script.
LABEL RUN docker run -d --privileged --net=host --pid=host --ipc=host -v /sys:/sys:ro -v /etc/localtime:/etc/localtime:ro -v /var/lib/docker:/var/lib/docker:ro -v /run:/run -v /var/log:/var/log -v /dev/log:/dev/log --name=pcp-pmwebd pcp-pmwebd

#
# The command to run - in this case the pmwebd service script. When this command
# exits, then the container exits. This is the default command and parameters
# and can be overridden by passing additional parameters to docker run.
ENV PATH /usr/share/pcp/lib:/usr/libexec/pcp/bin:$PATH
ENTRYPOINT ["pmwebd"]
CMD ["start"]
