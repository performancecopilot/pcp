.. _SecureClientConnections:

Establishing Secure Client Connections
################################################

.. contents::

Overview
**********

Check local PCP collector installation (requires the pcp-verify utility)::

    $ pcp verify --secure

Enabling Client and Server TLS/SSL: Steps Involved
*****************************************************

Before the PCP Collector system can be requested to communicate with TLS/SSL, certificates must be properly configured on the Collector Server and Client Monitor hosts.

This typically involves:

    * Obtain and install certificates for your PCP Collector systems, and configure each system to trust the certification authority's (CA's) certificate. This tutorial shows how to setup your own local CA to generate certificates.
    
    * Enable secure connections in the *pmcd* and *pmproxy* daemons by configuring the system certificate database with the PCP Collector certificate.
    
    * Creating a certificate that is trusted by the collector and installing this on the client.
    
    * Configuring the collector to reject non-local connections without a trusted client certificate.
    
    * Ensure that each user monitoring a PCP Collector system obtains and installs a personal certificate for the tools that will communicate with that collector.

This can be done by manually updating a monitor-side certificate database, or automatically by reviewing and accepting the certificate delivered to the monitor tools during the first attempt to access the PCP Collector system.

CA Setup
*********

We will be using a local CA to generate the locally trusted certificates. At a high-level: a certificate request (CR) must be generated, then sent to the certificate authority (CA) you will be using. The CA will generate a new trusted certificate and send it to you. Once this certificate has been received, it will be installed on the client.


Create the local CA::

    $ openssl genrsa -out rootCA.key 2048
    $ openssl req -new -x509 -extensions v3_ca -key rootCA.key -out rootCA.crt -days 3650
    .....
    Common Name (eg, your name or your server's hostname) []:myCA
    .....

Create the client certificate::

    $ openssl genrsa -out pmclient.key 2048
    $ openssl req -new -key pmclient.key -out pmclient.csr
    ....
    Common Name (eg, your name or your server's hostname) []:pmclient
    ....
    $ openssl x509 -req -in pmclient.csr -CA rootCA.crt -CAkey rootCA.key -CAcreateserial -out pmclient.crt -days 500 -sha256
    $ openssl pkcs12 -export -out pmclient_pkcs12.key -inkey pmclient.key  -in pmclient.crt -certfile rootCA.crt

Create the server certificate::

    $ openssl genrsa -out pmserver.key 2048
    $ openssl req -new -key pmserver.key -out pmserver.csr
    ....
    Common Name (eg, your name or your server's hostname) []:pmserver
    ....
    $ openssl x509 -req -in pmserver.csr -CA rootCA.crt -CAkey rootCA.key -CAcreateserial -out pmserver.crt -days 500 -sha256
    $ openssl pkcs12 -export -out pmserver_pkcs12.key -inkey pmserver.key  -in pmserver.crt -certfile rootCA.crt

Collector Setup
*****************

All PCP Collector systems must have a valid certificate in order to participate in secure PCP protocol exchanges. We will use the server certificate created above and install it into a pcp specific directory that can be used by the pcp server components. This directory should exists with current PCP versions.

Install the CA and server certificate(As the PCP user)::

    $ echo > /tmp/empty
    $ certutil -d sql:/etc/pcp/nssdb  -N -f /tmp/empty
    $ certutil -d sql:/etc/pcp/nssdb -A -t "CT,," -n "Root CA" -i rootCA.crt
    $ certutil -d sql:/etc/pcp/nssdb -A -t "P,," -n "pmserver_cert" -i pmserver.crt
    $ pk12util -i pmserver_pkcs12.key -d sql:/etc/pcp/nssdb
    $ certutil -d sql:/etc/pcp/nssdb -L

    Certificate Nickname                                         Trust Attributes
                                                                SSL,S/MIME,JAR/XPI

    Root CA                                                      CT,, 
    pmserver_cert                                                P,,  

Configure pmcd to use our certificate and enforce client certificates::

    $ cat /etc/pcp/pmcd/pmcd.options
    ....
    -C sql:/etc/pcp/nssdb
    -M pmserver_cert
    -Q

At this stage, attempts to restart the PCP Collector infrastructure will begin to take notice of the new contents of the certificate database.

Monitor Setup
***************

Attempts to connect to the server without a certificate will now fail:


Test remote connection::

    $ pminfo -h remote.host
    pminfo: Cannot connect to PMCD on host "remote.host": PMCD requires a client certificate

In this configuration, PCP Monitoring (client) tools require 2 certificates. A client certificate that can be sent to the server for authentication, and a trusted certificate to validate the server in a TLS/SSL connection. The first certificate was generated above and will be installed manually.

Install the CA and client certificate(As local user)::

    $ echo > /tmp/empty
    $ mkdir -p -m 0755 $HOME/.pki/nssdb
    $ certutil -d sql:$HOME/.pki/nssdb -N -f /tmp/empty
    $ certutil -d sql:$HOME/.pki/nssdb -A -t "CT,," -n "Root CA" -i ./rootCA.crt
    $ certutil -d sql:$HOME/.pki/nssdb -A -t "P,," -n "pmclient_cert" -i pmclient.crt
    $ pk12util -i pmclient_pkcs12.key -d sql:$HOME/.pki/nssdb

The second certificate can be installed beforehand or can be delivered via the TLS/SSL connection exchange. In the latter situation, the user is prompted as to whether the certificate is to be trusted (see example below).

Once certificates are in place, we are ready to attempt to establish secure connections between remote PCP Monitor and Collector hosts. This can be achieved by specifically requesting a secure connection for individual host connections. Alternatively, an environment variable can be set to request that all client connections within that shell environment be made securely. This environment variable should have the value **enforce** meaning "all connections must be secure, fail if this cannot be achieved".

Using the approach of certificate delivery via the TLS/SSL protocol, the database and certificate will be automatically setup in the correct location on your behalf. You can also set some environment variables if you are using self signed certs or if the domainname in the cert does not match. Without these, you will be interactively prompted to approve the certificate.

To establish a secure connection, in a shell enter::

    $ export PCP_SECURE_SOCKETS=enforce
    $ export PCP_ALLOW_BAD_CERT_DOMAIN=1
    $ export PCP_ALLOW_SERVER_SELF_CERT=1
    $ pminfo -h remote.host -f kernel.uname.nodename

    kernel.uname.nodename
        value "remote.host"
