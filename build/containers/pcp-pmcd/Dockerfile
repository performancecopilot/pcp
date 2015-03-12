#
# Dockerfile to build the pcp-pmcd container image. See docker-build(1).
# e.g. docker build -t pcp-pmcd .
#
FROM fedora:latest
MAINTAINER Mark Goodwin <mgoodwin@redhat.com>

# update the fedora base image and clean the yum cache (we don't want to ship the cache!)
RUN yum -y update && yum clean all

# install pcp and it's dependencies, clean the cache.
RUN yum -y install pcp && yum clean all

# souce the pcp config environment and setup a few directories
# rc.pause tells the pmcd start script we're in a container and so do not exit
RUN . /etc/pcp.conf && touch $PCP_ETC_DIR/pcp/rc.pause && mkdir -p $PCP_RUN_DIR $PCP_TMP_DIR/pmcd $PCP_LOG_DIR/pmcd

# we expose pmcd's main port on the host interfaces. Other ports can be added here as needed.
EXPOSE 44321

# the command to run - in this case the pmcd service script.
# If this command exits, then the container exits.
CMD . /etc/pcp.conf && $PCP_RC_DIR/pmcd start

# Example command to deploy this container on a docker server :
#   docker run -d --privileged --net=host -v /proc:/proc:ro -v /sys:/sys:ro \
#   -v /run:/run -v /var/run:/var/run -v /var/lib/docker:/var/lib/docker:ro \
#   -v /var/log/pcp:/var/log/pcp -v /etc/cron.d:/etc/cron.d:ro IMAGE-ID
#
# -d means to detach and run in the background. Use -it (instead of -d) if you want
#    a pty and "bash" as the command to run after IMAGE-ID on the command line.
#
# --privileged and --net=host  allow pmcd to bind to port 44321 on the host interfaces
#
# -v /proc:/proc:ro -v /sys:/sys:ro  allows pmcd to see the global /proc and /sys pseudo filesystems
#
# -v /run:/run -v /var/run:/var/run  for pid files and unix domain sockets
#
# -v /var/lib/docker:/var/lib/docker:ro  so the root PMDA can see /var/lib/docker/*
#
# -v /var/log/pcp:/var/log/pcp  for pmcd.log and pmda log files
#    (this would also be used by a pcp-pmlogger container to write PCP archives)
#
# -v /etc/cron.d:/etc/cron.d:ro  PCP cron services, e.g. pmlogger_daily etc
#
# IMAGE-ID is the pcp-pmcd image id. Use "docker images" to list images.
#
# After IMAGE-ID you can optionally specify an alternate command to run, e.g. /bin/bash
# (which would typically be used with -it instead of -d) to run a shell instead of the
# command specified in the CMD line of the Dockerfile. In this case pmcd would not be
# started - you'd have to start it manually in the background.
