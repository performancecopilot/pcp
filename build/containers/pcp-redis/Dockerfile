#
# Sample Dockerfile starting a Redis server with a minimal pmcd
#
# Build:   $ docker build -t pcp-redis .
# Name:    $ sudo mkdir -p /run/containers/pcp-redis
# Run:     $ `docker inspect --format='{{.Config.Labels.RUN}}' pcp-redis`
# Observe: $ pminfo --host=local:/run/containers/pcp-redis/pmcd.socket \
#                   --fetch proc network redis
#

FROM pcp-standalone:latest
ENV NAME pcp-redis
ENV IMAGE pcp-redis

RUN dnf -y install redis pcp-pmda-redis && dnf clean all
RUN . /etc/pcp.conf && touch $PCP_PMDAS_DIR/redis/.NeedInstall
RUN systemctl enable redis
EXPOSE 6378

LABEL RUN docker run \
--publish 6378:6378 \
--tmpfs=/run --tmpfs=/tmp \
--volume=/sys/fs/cgroup:/sys/fs/cgroup:ro \
--volume=/run/containers/pcp-redis:/run/pcp \
--name=pcp-redis pcp-redis

STOPSIGNAL SIGRTMIN+3
CMD ["/sbin/init"]
