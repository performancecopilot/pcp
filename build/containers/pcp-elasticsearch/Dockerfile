#
# Sample Dockerfile starting an Elasticsearch instance with a minimal pmcd
#
# Build:   $ docker build -t pcp-elasticsearch .
# Name:    $ sudo mkdir -p /run/containers/pcp-elasticsearch
# Run:     $ `docker inspect --format='{{.Config.Labels.RUN}}' pcp-elasticsearch`
# Observe: $ pminfo --host=local:/run/containers/pcp-elasticsearch/pmcd.socket \
#                   --fetch proc network elasticsearch
#

FROM pcp-standalone:latest
ENV NAME pcp-elasticsearch
ENV IMAGE pcp-elasticsearch

RUN dnf -y install elasticsearch pcp-pmda-elasticsearch && dnf clean all
RUN . /etc/pcp.conf && touch $PCP_PMDAS_DIR/elasticsearch/.NeedInstall
RUN systemctl enable elasticsearch
EXPOSE 9200 9300

# elasticsearch transport on 9200, REST API on 9300
LABEL RUN docker run \
--publish 9200:9200 \
--publish 9300:9300 \
--tmpfs=/run --tmpfs=/tmp \
--volume=/sys/fs/cgroup:/sys/fs/cgroup:ro \
--volume=/run/containers/pcp-elasticsearch:/run/pcp \
--name=pcp-elasticsearch pcp-elasticsearch

STOPSIGNAL SIGRTMIN+3
CMD ["/sbin/init"]
