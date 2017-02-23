#
# Sample Dockerfile with speed installed and all the example binaries built
#
# Build:   $ docker build -t pcp-go-app .
# Name:    $ sudo mkdir -p /run/containers/pcp-go-app
# Run:     $ `docker inspect --format='{{.Config.Labels.RUN}}' pcp-go-app`
#

FROM pcp-standalone:latest
ENV NAME pcp-go-app
ENV IMAGE pcp-go-app

#
# https://github.com/docker-library/golang/blob/master/1.8/Dockerfile
#

ENV GOLANG_VERSION 1.8
ENV GOLANG_DOWNLOAD_URL https://golang.org/dl/go$GOLANG_VERSION.linux-amd64.tar.gz
ENV GOLANG_DOWNLOAD_SHA256 53ab94104ee3923e228a2cb2116e5e462ad3ebaeea06ff04463479d7f12d27ca

RUN curl -fsSL "$GOLANG_DOWNLOAD_URL" -o golang.tar.gz \
    && echo "$GOLANG_DOWNLOAD_SHA256  golang.tar.gz" | sha256sum -c - \
    && tar -C /usr/local -xzf golang.tar.gz \
    && rm golang.tar.gz

ENV GOPATH /go
ENV PATH $GOPATH/bin:/usr/local/go/bin:$PATH

RUN mkdir -p "$GOPATH/src" "$GOPATH/bin" && chmod -R 777 "$GOPATH"
WORKDIR $GOPATH/src

#
# speed
#

# Will not work because no git, and not installing for now
# RUN go get github.com/performancecopilot/speed/...

ENV SPEED_DOWNLOAD_URL https://github.com/performancecopilot/speed/archive/master.tar.gz

RUN curl -fsSL "$SPEED_DOWNLOAD_URL" -o speed.tar.gz \
    && mkdir -p $GOPATH/src/github.com/performancecopilot/speed \
    && tar -C $GOPATH/src/github.com/performancecopilot/speed -xzf speed.tar.gz --strip=1 \
    && rm speed.tar.gz

RUN go install github.com/performancecopilot/speed/...

LABEL RUN docker run \
    --interactive --tty \
    --tmpfs=/tmp \
    --name=pcp-go-app pcp-go-app

STOPSIGNAL SIGRTMIN+3
CMD ["/bin/bash"]
