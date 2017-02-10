#
# Dockerfile to build the base container image. This uses a specific
# Fedora version, and is intended as the base image for all the other
# PCP containers (i.e. they are layered on top of this one).
#
# Build:   $ docker build -t pcp-base .
# Debug:   $ `docker inspect --format='{{.Config.Labels.RUN}}' pcp-base`
#
FROM fedora:25
MAINTAINER PCP Development Team <pcp@groups.io>
ENV NAME pcp-base
ENV IMAGE pcp-base

# Update the Fedora base image and clean the dnf cache
# (normally disabled during development)
#
RUN dnf -y install createrepo.noarch && dnf -y update && dnf clean all

# Set up a repo for bintray packages to be installed. This is the default.
# Note that this repo specifies enabled=1. All container images
# based on pcp-base will use the default enabled repo to install
# packages, so to use e.g. the pcp-devel repo instead, just edit
# the enabled flag to specify an alternate repo you want to use.
#
COPY pcp.repo /etc/yum.repos.d/pcp.repo

# Set up a repo for local packages to be installed. This is only for use
# during development. To use this repo, change enabled=0 to enabled=1.
# Here we expect $(TOPDIR)/build/containers/RPMS to contain packages to be
# available for the container image, e.g. from $(TOPDIR)/Makepkgs builds.
#
#RUN mkdir -p /root/rpmbuild/RPMS
#COPY RPMS/*.rpm /root/rpmbuild/RPMS/
#RUN createrepo /root/rpmbuild/RPMS

# Install minimal pcp (i.e. pcp-conf) and it's dependencies, clean the cache.
# This is intended as the base image for all other PCP containers. The
# bintray repo is enabled by default, see comments above.
RUN dnf -y install pcp-conf && dnf clean all

# Run in the container as root - avoids host/container user mismatches
ENV PCP_USER root
ENV PCP_GROUP root

# Denote this as a container environment, for rc scripts
ENV PCP_CONTAINER_IMAGE pcp-base

# Since this can be invoked as an interactive container, setup bash.
COPY bash_profile /root/.bash_profile

# RUN labels are used by the 'atomic' command, e.g. atomic run pcp-base
# Without this command, one can use docker inspect (see "Debug:" above).
#
LABEL RUN docker run \
--interactive --tty \
--privileged --net=host --pid=host --ipc=host \
--volume=/run:/run \
--volume=/sys:/sys:ro \
--volume=/etc/localtime:/etc/localtime:ro \
--volume=/var/lib/docker:/var/lib/docker:ro \
--volume=/var/log:/var/log \
--volume=/dev/log:/dev/log \
--name=pcp-base pcp-base

# The command to run (bash). When this command exits, then the container exits.
# This is the default command and parameters, mainly for debugging, and usually
# would be overridden by higher layers of the container build.
#
CMD ["/bin/bash", "-l"]
