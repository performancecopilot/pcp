#
# Sample Dockerfile starting the Apache web server with a minimal pmcd
#
# $ docker build -t pcp-apache .
#
# $ sudo mkdir -p /run/containers/pcp-apache
# $ sudo chown pcp:pcp /run/containers/pcp-apache
#
# $ docker run -ti --tmpfs /run --tmpfs /tmp -p 80:80 \
#	-v /run/containers/pcp-apache:/run/pcp \
#	-v /sys/fs/cgroup:/sys/fs/cgroup:ro \
#	pcp-apache
#
# $ pminfo --fetch --host unix:/run/containers/pcp-apache/pmcd.socket
#

FROM pcp-standalone:latest
ENV NAME pcp-apache
ENV IMAGE pcp-apache

RUN dnf -y install httpd pcp-pmda-apache && dnf clean all
RUN . /etc/pcp.conf && touch $PCP_PMDAS_DIR/apache/.NeedInstall
RUN rm -f /etc/httpd/conf.d/status.conf; \
    echo 'ExtendedStatus on'             >> /etc/httpd/conf.d/status.conf; \
    echo '<Location /server-status>'     >> /etc/httpd/conf.d/status.conf; \
    echo '     SetHandler server-status' >> /etc/httpd/conf.d/status.conf; \
    echo '    Allow from all'            >> /etc/httpd/conf.d/status.conf; \
    echo '</Location>'                   >> /etc/httpd/conf.d/status.conf
RUN systemctl enable httpd
EXPOSE 80

LABEL RUN docker run -it --tmpfs /run --tmpfs /tmp -p 80:80 \
        -v /run/containers/pcp-apache:/run/pcp \
        -v /sys/fs/cgroup:/sys/fs/cgroup:ro \
        --name=pcp-apache pcp-apache

STOPSIGNAL SIGRTMIN+3
CMD ["/sbin/init"]
