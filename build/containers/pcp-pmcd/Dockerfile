#
# Dockerfile to build the pcp-pmcd container image. See docker-build(1).
# e.g. docker build -t pcp-pmcd .
#
FROM fedora:latest
MAINTAINER Mark Goodwin <mgoodwin@redhat.com>

# update the fedora base image and clean the yum cache
# (we don't want to ship the cache!)
RUN yum -y update && yum clean all

# install pcp and it's dependencies, clean the cache.
RUN yum -y install pcp && yum clean all

# debug: install any local RPMs for testing
# COPY *.rpm /upgrade/
# RUN rpm -Uvh --force /upgrade/*.rpm && rm -fr /upgrade

# disable service advertising - no avahi daemon in the container
# (dodges warnings from pmcd attempting to connect during startup)
RUN . /etc/pcp.conf && echo "-A" >> $PCP_PMCDOPTIONS_PATH

# expose pmcd's main port on the host interfaces.
EXPOSE 44321

# denote this as a container environment, for rc scripts
ENV PCP_CONTAINER_IMAGE pcp-pmcd

# the command to run - in this case the pmcd service script.
# when this command exits, then the container exits.
ENV PATH /usr/share/pcp/lib:$PATH
ENTRYPOINT ["pmcd"]
CMD ["start"]

# Example command to deploy this container on a docker server :
#   docker run -d \
#   --privileged --net=host \
#   -v /sys:/sys:ro \
#   -v /proc:/proc:ro \
#   -v /etc/cron.d:/etc/cron.d \
#   -v /etc/localtime:/etc/localtime:ro \
#   -v /var/lib/docker:/var/lib/docker:ro \
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
# IMAGE-ID is the pcp-pmcd image id. Use "docker images" to list images.
#
# After IMAGE-ID you can optionally specify an alternate command to run, e.g. /bin/bash
# (which would typically be used with -it instead of -d) to run a shell instead of the
# command specified in the CMD line of the Dockerfile. In this case pmcd would not be
# started - you'd have to start it manually in the background.
