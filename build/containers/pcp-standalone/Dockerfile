#
# Copyright (c) 2017 Red Hat.
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
# Dockerfile to build the pcp-standalone container image, layered on pcp-base.
#
FROM pcp-base:latest
MAINTAINER PCP Development Team <pcp@groups.io>

ENV NAME pcp-standalone
ENV IMAGE pcp-standalone

# setup for systemd containers
ENV container docker

# Install pcp (and its dependencies) for standalone pmcd use, clean the cache.
RUN dnf -y install pcp && dnf clean all

# Register pmcd as a service to be started by systemd
RUN systemctl enable pmcd

# Setup pmcd to run in unprivileged mode of operation
RUN . /etc/pcp.conf; \
    rm -f $PCP_SYSCONFIG_DIR/pmcd; \
    echo "PMCD_ROOT_AGENT=0" >> $PCP_SYSCONFIG_DIR/pmcd

# Configure pmcd with a minimal set of DSO agents
RUN . /etc/pcp.conf; \
    rm -f $PCP_PMCDCONF_PATH; \
    echo "# Name  ID  IPC  IPC Params  File/Cmd" >> $PCP_PMCDCONF_PATH; \
    echo "pmcd     2  dso  pmcd_init   $PCP_PMDAS_DIR/pmcd/pmda_pmcd.so"   >> $PCP_PMCDCONF_PATH; \
    echo "proc     3  dso  proc_init   $PCP_PMDAS_DIR/proc/pmda_proc.so"   >> $PCP_PMCDCONF_PATH; \
    echo "linux   60  dso  linux_init  $PCP_PMDAS_DIR/linux/pmda_linux.so" >> $PCP_PMCDCONF_PATH; \
    echo "mmv     70  dso  mmv_init    $PCP_PMDAS_DIR/mmv/pmda_mmv.so"     >> $PCP_PMCDCONF_PATH; \
    rm -f $PCP_VAR_DIR/pmns/root_xfs $PCP_VAR_DIR/pmns/root_jbd2 $PCP_VAR_DIR/pmns/root_root; \
    touch $PCP_VAR_DIR/pmns/.NeedRebuild

# Disable service advertising - no avahi support in the container
# (dodges warnings from pmcd attempting to connect during startup)
RUN . /etc/pcp.conf && echo "-A" >> $PCP_PMCDOPTIONS_PATH

# allow unauthenticated access to proc.* metrics (default is false)
ENV PROC_ACCESS 1

# Expose pmcd's main port on the host interface
EXPOSE 44321

# Debugging mode - startup with a shell, no application.
LABEL RUN docker run --tmpfs /run --tmpfs /tmp -v /sys/fs/cgroup:/sys/fs/cgroup:ro --name=pcp-standalone pcp-standalone
CMD ["/bin/bash", "-l"]
