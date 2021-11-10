.. _AuthenticatedConnections:

Setup Authenticated Connections
################################################

.. contents::

An authenticated connection is one where the PCP collector (pmcd and PMDAs) is made aware of the credentials of the user running the monitor tools. This allows the PCP collector software to perform two important functions:

1. Grant additional access, allowing potentially sensitive information to be accessed by the authenticated user;

2. Make access control decisions for users and groups in order to prevent inappropriate access to metrics or over-subscription of server resources.

Overview
*************

All connections made to the PCP metrics collector daemon (**pmcd**) are made using the PCP protocol, which is TCP/IP based.

The Performance Co-Pilot has two facilities for transfering credentials between the monitoring and collecting components - a local-host-only facility using Unix domain sockets, and SASL-based authentication which can be over either IPv4 or IPv6 sockets (local or remote).

Local Credentials
******************

For local connections, there is a fast local transport mechanism using Unix domain sockets. These sockets provide a facility whereby the userid and groupid of the monitor process is automatically made available to the collector process, with no user intervention being required.

This will become the default localhost transport mechanism over time. However, its use can be requested via the *unix*: host specification (**-h** option) with all monitor tools.

In the following exercise, we make use of per-process metrics from the **pmdaproc** PCP Collector agent. This PMDA is enabled by default, and makes use of authenticated credentials from the monitor tools to gate access to sensitive information - process identifiers, names, arguments, and so forth. In modern versions of PCP, this information is unavailable (PM_ERR_PERMISSION is returned) unless the PMDA has been given user credentials.

.. sourcecode::

    Check for support, then establish a Unix domain socket connection with automatic credentials transfer:
    $ $PCP_BINADM_DIR/pmconfig -L unix_domain_sockets
    unix_domain_sockets=true

    $ pmprobe -f -h localhost proc.fd.count
    proc.fd.count -12387 No permission to perform requested operation

    $ pmprobe -f -h unix: proc.fd.count
    proc.fd.count 118

    $ pminfo -dmt -h unix: proc.fd.count

    proc.fd.count PMID: 3.51.0 [open file descriptors]
        Data Type: 32-bit unsigned int  InDom: 3.9 0xc00009
        Semantics: instant  Units: count

A *local:* specification is also available, which indicates that the monitor tool should first attempt to establish a Unix domain socket connection (with automatic credentials transfer), but failing that fall back to the traditional socket connection style (with no credentials transfer, unless mandated by collector or explicitly requested by the monitor user - described later in this tutorial).

.. note::
    
   This facility is completely independent and separate to the remote access facility (described later). Therefore, it can still be used even when support for the remote authentication facilities is unavailable or not configured. This local facility is always enabled on platforms that support it - these include Linux, Mac OS X and Solaris.
 
Remote Access
****************

The alternative authentication facility is the "Simple Authentication and Security Layer" (SASL) which provides for remote authentication. Usually this would be used with a secure connection to ensure sensitive information is not transmitted in the clear.

SASL can be configured with many different authentication mechanisms, and this configuration is done transparently (outside of PCP, as described shortly). This list of mechanisms can be changed via the *mech_list* entry in the ``/etc/sasl2/pmcd.conf`` file, along with other critical security parameters defining how **pmcd** should handle authentication requests. The *mech_list* can be set to any one of the installed SASL mechanisms, or a space-separated list of mechanisms from which the monitor tools can select. There are also situations where it makes sense to not set *mech_list* at all, which we will explore shortly.

To make use of SASL-based authentication, both monitor and collector systems need to be capable of running the SASL code (PCP support, in particular, must be present on each end of the connection). As demonstrated in the following examples, we can use both the **pmconfig** PCP command and the **pluginviewer** SASL command to interogate aspects of the installations on each end of a connection. The **pluginviewer** command has separate options for reporting on client (**-c**) and server (**-s**) components of an installation.

Using sasldb
=============

The SASL library provides a custom authentication technique, independent to the set of users and groups on a system, called sasldb. This involves the creation of a new authentication database separate to the native login mechanisms that a host provides. One advantage of using it is that it does not require any special privileges on the part of the daemon performing authentication (pmcd in our case - which does not run as a privileged process and thus typically cannot be used with the common authentication databases, such as the /etc/shadow mechanism).

The sasldb approach can be used with several SASL mechanisms, and it is commonly used to verify a SASL setup. Here, we will configure PCP to use sasldb by allowing the "plain" SASL mechanism to authenticate against it.


Check for support::

    $ $PCP_BINADM_DIR/pmconfig -L unix_domain_sockets authentication
    secure_sockets=true
    authentication=true

    $ pluginviewer -s -m PLAIN
    [...]
    Plugin "plain" [loaded], 	API version: 4
        SASL mechanism: PLAIN, best SSF: 0, supports setpass: no
        security flags: NO_ANONYMOUS|PASS_CREDENTIALS
        features: WANT_CLIENT_FIRST|PROXY_AUTHENTICATION

Modify the ``/etc/sasl2/pmcd.conf`` configuration file, so that it makes "plain" authentication available to PCP monitor tools, and also to specify the location of the SASL credentials database file.

By default, this has been specified as ``/etc/pcp/passwd.db``. SASL commands allow this database to be manipulated - adding, listing and deleting users, setting their passwords, and so on.

Configure the SASL library::

    # grep PCP_SASLCONF_DIR /etc/pcp.conf
    PCP_SASLCONF_DIR=/etc/sasl2

    # cat $PCP_SASLCONF_DIR/sasl2/pmcd.conf
    mech_list: plain scram-sha-256 gssapi
    sasldb_path: /etc/pcp/passwd.db

Next we create the database and add some users to it. Note that requires the previous step to have been performed, as that informs the tools about the location of the database.

Administer the SASL database (add, list and disable users)::

    # saslpasswd2 -a pmcd jack
    Password: xxxxxx
    Again (for verification): xxxxxx

    # saslpasswd2 -a pmcd jill
    Password: xxxxxx
    Again (for verification): xxxxxx

    # sasldblistusers2 -f /etc/pcp/passwd.db
    jack@server.demo.net: userPassword
    jill@server.demo.net: userPassword

    # saslpasswd2 -a pmcd -d jill

    # $PCP_RC_DIR/pmcd restart

Finally, we're ready to try it out. As we have just restarted **pmcd** (above), its worth checking its log file - ``$PCP_LOG_DIR/pmcd/pmcd.log`` - and if that is free of errors, we can attempt to authenticate.

Establish an authenticated connection::

    $ pminfo -fmdt -h pcps://server.demo.net?method=plain proc.fd.count
    Username: jack
    Password: xxxxxx

    proc.fd.count PMID: 3.51.0 [open file descriptors]
        Data Type: 32-bit unsigned int  InDom: 3.9 0xc00009
        Semantics: instant  Units: count
        inst [1281 or "001281 bash"] value 4
        inst [1287 or "001287 make -j 8 default_pcp"] value 5
        inst [1802 or "001802 bash"] value 4
        [...]

Using saslauthd
================

The SASL mechanism configured by default on a PCP collector system is sasldb which provides a basic username and password style authentication mechanism with no reliance on external daemons, package dependencies, and so on. Without customisation for individual users, as described above, it allows no access and can thus be considered secure-by-default.

However, it is often more convenient to use the same authentication mechanism that is used when logging in to a host (e.g. PAM on Linux and Solaris). Because the PCP daemons run without superuser privileges, they cannot perform this authentication themselves, and so it must be achieved using a separate, privileged process. SASL provides saslauthd for this purpose.

Check for support::

    $ pmconfig -L secure_sockets authentication
    secure_sockets=true
    authentication=true

    $ pluginviewer -s -m PLAIN
    [...]
    Plugin "plain" [loaded], 	API version: 4
        SASL mechanism: PLAIN, best SSF: 0, supports setpass: no
        security flags: NO_ANONYMOUS|PASS_CREDENTIALS
        features: WANT_CLIENT_FIRST|PROXY_AUTHENTICATION

    $ ps ax | grep saslauthd
    2857 ?        Ss     0:00 /usr/sbin/saslauthd -m /var/run/saslauthd -a pam
    2858 ?        S      0:00 /usr/sbin/saslauthd -m /var/run/saslauthd -a pam
    [...]

In this case, the SASL mechanism configuration is entirely handled by saslauthd so we simply need to ensure we pass authentication through to the daemon - no configuration beyond that should be needed.

Setup use of saslauthd on the PCP Collector::

    # grep PCP_SASLCONF_DIR /etc/pcp.conf
    PCP_SASLCONF_DIR=/etc/sasl2

    # cat $PCP_SASLCONF_DIR/sasl2/pmcd.conf
    pwcheck_method: saslauthd

    # $PCP_RC_DIR/pmcd restart

We are now ready to establish an authenticated connection. saslauthd will log into the system log, so any authorisation failures can be further diagnosed using information captured there.

Setup use of saslauthd on the PCP Collector::

    $ pminfo -h pcps://server.demo.net?method=plain -dfmt proc.fd.count
    Username: jack
    Password: xxxxxx

    proc.fd.count PMID: 3.51.0 [open file descriptors]
        Data Type: 32-bit unsigned int  InDom: 3.9 0xc00009
        Semantics: instant  Units: count
        inst [1281 or "001281 bash"] value 4
        inst [1287 or "001287 make -j 8 default_pcp"] value 5
        inst [1802 or "001802 bash"] value 4
        [...]

Kerberos Authentication
===========================


The PCP collector system can be configured to authenticate using Kerberos single-sign-on using the GSSAPI authentication mechanism. 
This mechanism is enabled via the *mech_list* option in ``/etc/sasl2/pmcd.conf``, and the keytab should be set to ``/etc/pcp/pmcd/krb5.tab``.

Check for support::

    $ pmconfig -L secure_sockets authentication
    secure_sockets=true
    authentication=true

    $ pluginviewer -s -m GSSAPI
    [...]
    Plugin "gssapiv2" [loaded], 	API version: 4
        SASL mechanism: GSSAPI, best SSF: 56, supports setpass: no
        security flags: NO_ANONYMOUS|NO_PLAINTEXT|NO_ACTIVE|PASS_CREDENTIALS|MUTUAL_AUTH
        features: WANT_CLIENT_FIRST|PROXY_AUTHENTICATION|DONTUSE_USERPASSWD

Setup Kerberos authentication on the PCP Collector::

    # kadmin.local
    kadmin.local: add_principal pmcd/server.demo.net
    Enter password for principal "pmcd/server.demo.net@DEMO.NET":
    Re-enter password for principal "pmcd/server.demo.net@DEMO.NET":
    Principal "pmcd/server.demo.net@DEMO.NET" created.

    kadmin.local:  ktadd -k /root/pmcd-server-demo.tab pmcd/server.demo.net@DEMO.NET
    Entry for principal pmcd/server.demo.net@DEMO.NET with kvno 4, encryption type Triple DES cbc mode with HMAC/sha1 added to keytab WRFILE:/root/pmcd-server-demo.tab.
    Entry for principal pmcd/server.demo.net@DEMO.NET with kvno 4, encryption type ArcFour with HMAC/md5 added to keytab WRFILE:/root/pmcd-server-demo.tab.
    Entry for principal pmcd/server.demo.net@DEMO.NET with kvno 4, encryption type DES with HMAC/sha1 added to keytab WRFILE:/root/pmcd-server-demo.tab.
    Entry for principal pmcd/server.demo.net@DEMO.NET with kvno 4, encryption type DES cbc mode with RSA-MD5 added to keytab WRFILE:/root/pmcd-server-demo.tab.

    kadmin.local: quit

    # scp /root/pmcd-server-demo.tab root@server.demo.net:/etc/pcp/pmcd/krb5.tab
    # rm /root/pmcd-server-demo.tab

    $ kinit jack@DEMO.NET
    Password for jack@DEMO.NET: xxxxxx

    $ klist
    Ticket cache: FILE:/tmp/krb5cc_500
    Default principal: jack@DEMO.NET

    Valid starting     Expires            Service principal
    02/07/13 20:46:40  02/08/13 06:46:40  krbtgt/DEMO.NET@DEMO.NET
        renew until 02/07/13 20:46:40

    $ pminfo -h 'pcps://server.demo.net?method=gssapi' -dfmt proc.fd.count

    proc.fd.count PMID: 3.51.0 [open file descriptors]
        Data Type: 32-bit unsigned int  InDom: 3.9 0xc00009
        Semantics: instant  Units: count
        inst [1281 or "001281 bash"] value 4
        inst [1287 or "001287 make -j 8 default_pcp"] value 5
        inst [1802 or "001802 bash"] value 4
        [...]
