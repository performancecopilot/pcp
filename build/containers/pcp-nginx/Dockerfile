#
# Sample Dockerfile starting the Nginx web server with a minimal pmcd
#
# Build:   $ docker build -t pcp-nginx .
# Name:    $ sudo mkdir -p /run/containers/pcp-nginx
# Run:     $ `docker inspect --format='{{.Config.Labels.RUN}}' pcp-nginx`
# Observe: $ pminfo --host=local:/run/containers/pcp-nginx/pmcd.socket \
#                   --fetch proc network nginx
#

FROM pcp-standalone:latest
ENV NAME pcp-nginx
ENV IMAGE pcp-nginx

RUN dnf -y install nginx pcp-pmda-nginx && dnf clean all
RUN . /etc/pcp.conf && touch $PCP_PMDAS_DIR/nginx/.NeedInstall
COPY status.conf /etc/nginx/default.d/status.conf
RUN systemctl enable nginx
EXPOSE 80

LABEL RUN docker run \
--publish 80:80 \
--tmpfs=/run --tmpfs=/tmp \
--volume=/sys/fs/cgroup:/sys/fs/cgroup:ro \
--volume=/run/containers/pcp-nginx:/run/pcp \
--name=pcp-nginx pcp-nginx

STOPSIGNAL SIGRTMIN+3
CMD ["/sbin/init"]
