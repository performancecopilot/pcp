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
# Dockerfile to build the pcp container image. This is based on
# Fedora rawhide (currently f22) and is intended as the base image
# for all other PCP containers, i.e. layered.
#
FROM fedora:latest
MAINTAINER PCP Development Team <pcp@groups.io>

# update the Fedora base image and clean the dnf cache
# (normally disabled during development)
RUN dnf -y install createrepo.noarch && dnf -y update && dnf clean all

# Set up a repo for latest bintray packages to be installed. This is the
# default. Note that this repo specifies enabled=1. All container images
# based on pcp-base will use the default enabled repo to install pcp
# packages, so to use e.g. the pcp-devel repo instead, just edit the
# appropriate enabled flag to specify the repo you want to use by default.
# You only need to change this in the pcp-base Dockerfile since all
# others are based on pcp-base and thus inherit the default repo settings.
RUN fver=`awk '{print $3}' /etc/redhat-release`;\
echo -e '[pcp-bintray]\nname=PCP Bintray Yum Repository\n\
baseurl=https://dl.bintray.com/pcp/f'$fver'\ngpgcheck = 0\nenabled = 1'\
>/etc/yum.repos.d/pcp-bintray.repo

# Set up a repo for local packages to be installed. This is only for use
# during development. To use this repo, change enabled=0 to enabled=1.
# Here we expect $(TOPDIR)/build/rpm to have the PCP RPMS to be
# installed in the container image, i.e. from the current Makepkgs build.
RUN mkdir -p /tmp/RPMS; \
echo -e '[pcp-devel]\nname=PCP Development Yum Repository\n\
baseurl=file:///tmp/RPMS\ngpgcheck = 0\nenabled = 0'\
>/etc/yum.repos.d/pcp-devel.repo
COPY RPMS/*.rpm /tmp/RPMS/
RUN createrepo /tmp/RPMS

#
# install minimal pcp (i.e. pcp-conf) and it's dependencies, clean the cache.
# This is intended as the base image for all other PCP containers. The
# bintray repo is enabled by default, see comments above.
RUN dnf -y install pcp-conf && dnf clean all

#
# Run in the container as root - avoids PCP_USER mismatches
RUN sed -i -e 's/PCP_USER=.*$/PCP_USER=root/' -e 's/PCP_GROUP=.*$/PCP_GROUP=root/' /etc/pcp.conf

#
# Since this can be invoked as an interactive container, set the bash prompt.
# All containers layered on pcp-base inherit this, with their own name.
RUN echo export PS1='"[$PCP_CONTAINER_IMAGE] "' >> /root/.bash_profile
RUN echo PATH=/usr/bin:$PATH >> /root/.bash_profile

#
# denote this as a container environment, for rc scripts
ENV PCP_CONTAINER_IMAGE pcp-base
ENV NAME pcp-base
ENV IMAGE pcp-base

#
# The RUN label is used by 'atomic' command, e.g. atomic run pcp
# Other platforms without the 'atomic' command can use docker inspect to
# extract it and use it in a script.
LABEL RUN docker run -it --privileged --net=host --pid=host --ipc=host -v /sys:/sys:ro -v /etc/localtime:/etc/localtime:ro -v /var/lib/docker:/var/lib/docker:ro -v /run:/run -v /var/log:/var/log -v /dev/log:/dev/log --name=pcp-base pcp-base

#
# The command to run (bash). When this command exits, then the container exits.
# This is the default command and parameters and can be overridden by passing
# additional parameters to docker run.
CMD ["/bin/bash", "-l"]
