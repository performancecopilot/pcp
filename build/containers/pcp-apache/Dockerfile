#
# Sample Dockerfile starting the Apache web server with a minimal pmcd
#
# Build:   $ docker build -t pcp-apache .
# Name:    $ sudo mkdir -p /run/containers/pcp-apache
# Run:     $ `docker inspect --format='{{.Config.Labels.RUN}}' pcp-apache`
# Observe: $ pminfo --host local:/run/containers/pcp-apache/pmcd.socket \
#                   --fetch proc network apache
#

FROM pcp-standalone:latest
ENV NAME pcp-apache
ENV IMAGE pcp-apache

RUN dnf -y install httpd pcp-pmda-apache && dnf clean all
RUN . /etc/pcp.conf && touch $PCP_PMDAS_DIR/apache/.NeedInstall
COPY status.conf /etc/httpd/conf.d/status.conf
RUN systemctl enable httpd
EXPOSE 80

LABEL RUN docker run \
--publish 80:80 \
--tmpfs=/run --tmpfs=/tmp \
--volume=/run/containers/pcp-apache:/run/pcp \
--volume=/sys/fs/cgroup:/sys/fs/cgroup:ro \
--name=pcp-apache pcp-apache

STOPSIGNAL SIGRTMIN+3
CMD ["/sbin/init"]
