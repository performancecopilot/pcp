#
# Sample Dockerfile starting the Nginx web server with a minimal pmcd
#
# $ docker build -t pcp-nginx .
#
# $ sudo mkdir -p /run/containers/pcp-nginx
# $ sudo chown pcp:pcp /run/containers/pcp-nginx
#
# $ docker run -ti --tmpfs /run --tmpfs /tmp -p 80:80 \
#	-v /run/containers/pcp-nginx:/run/pcp \
#	-v /sys/fs/cgroup:/sys/fs/cgroup:ro  \
#	pcp-nginx
#
# $ pminfo --fetch --host unix:/run/containers/pcp-nginx/pmcd.socket
#

FROM pcp-standalone:latest
ENV NAME pcp-nginx
ENV IMAGE pcp-nginx

RUN dnf -y install nginx pcp-pmda-nginx && dnf clean all
RUN . /etc/pcp.conf && touch $PCP_PMDAS_DIR/nginx/.NeedInstall
RUN rm -f /etc/nginx/default.d/status.conf; \
    echo 'location /nginx_status {' >> /etc/nginx/default.d/status.conf; \
    echo '    stub_status on;'      >> /etc/nginx/default.d/status.conf; \
    echo '    access_log off;'      >> /etc/nginx/default.d/status.conf; \
    echo '    allow 127.0.0.1;'     >> /etc/nginx/default.d/status.conf; \
    echo '    allow ::1;'           >> /etc/nginx/default.d/status.conf; \
    echo '    deny all;'            >> /etc/nginx/default.d/status.conf; \
    echo '}'                        >> /etc/nginx/default.d/status.conf
RUN systemctl enable nginx
EXPOSE 80

LABEL RUN docker run -it --tmpfs /run --tmpfs /tmp -p 80:80 \
        -v /sys/fs/cgroup:/sys/fs/cgroup:ro \
        -v /run/containers/pcp-nginx:/run/pcp \
        --name=pcp-nginx pcp-nginx

STOPSIGNAL SIGRTMIN+3
CMD ["/sbin/init"]
