#
# Sample Dockerfile with Go and Speed installed and running the example
# instrumented Acme service
#
# Build:   $ docker build -t pcp-go-app .
# Name:    $ sudo mkdir -p /run/containers/pcp-go-app
# Run:     $ `docker inspect --format='{{.Config.Labels.RUN}}' pcp-go-app`
#

FROM pcp-standalone:latest
ENV NAME pcp-go-app
ENV IMAGE pcp-go-app

RUN dnf -y install golang-bin golang-src git && dnf clean all
ENV GOPATH /go
ENV PATH $GOPATH/bin:$PATH

RUN mkdir -p -m 777 /go/bin /go/src
RUN go get github.com/performancecopilot/speed/examples/acme

COPY acme.service /usr/lib/systemd/system/acme.service
RUN systemctl enable acme

LABEL RUN docker run \
--tmpfs=/run --tmpfs=/tmp \
--volume=/run/containers/pcp-go-app:/run/pcp \
--volume=/sys/fs/cgroup:/sys/fs/cgroup:ro \
--name=pcp-go-app pcp-go-app

STOPSIGNAL SIGRTMIN+3
CMD ["/sbin/init"]
