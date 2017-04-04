#
# Sample Dockerfile starting the Java ACME Factory example application
#
# Build:   $ docker build -t pcp-java-app .
# Name:    $ sudo mkdir -p /run/containers/pcp-java-app
# Run:     $ `docker inspect --format='{{.Config.Labels.RUN}}' pcp-java-app`
# Observe: $ pminfo --host=local:/run/containers/pcp-java-app/pmcd.socket \
#                   --fetch proc network mmv.acme
#

FROM pcp-standalone:latest
ENV NAME pcp-java-app
ENV IMAGE pcp-java-app

RUN dnf -y install pcp-parfait-agent parfait-examples && dnf clean all
COPY acme.service /usr/lib/systemd/system/acme.service
RUN systemctl enable acme

LABEL RUN docker run \
--tmpfs=/run --tmpfs=/tmp \
--volume=/run/containers/pcp-java-app:/run/pcp \
--volume=/sys/fs/cgroup:/sys/fs/cgroup:ro \
--name=pcp-java-app pcp-java-app

STOPSIGNAL SIGRTMIN+3
CMD ["/sbin/init"]
