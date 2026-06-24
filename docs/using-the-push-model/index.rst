.. include:: ../../refs.rst

Using the pmlogger PUSH model
#############################

This tech note shows how to use the PUSH model.

Classic PULL metric collection
******************************

When data is collected, we often use 2 kinds of systems:

* ``client:`` The system whose data/metrics should be collected
* ``collector:`` A central system, collecting the data from one or multiple 
  client systems

Setting up PULL collection follows roughly these steps:

* On the client system: PCP gets installed, and a configuration is deployed 
  to allow access from the collector system
* On the collector system: pmlogger instances are configured to pull data 
  from the client system

Later, when the client system is decommissioned, the pmlogger config for the 
client needs to be removed from the collector system.

Concept of the PUSH model
*************************

PCP 7.0.3 and later feature the pmlogger PUSH model, allowing us to configure 
our collector system once, and we no longer have to touch it for 
configuration of new (or decommissioned) client systems. 

The client systems are all running with the same config, and they just push 
the PCP data to the collector system.

Configuring the collect system for the PUSH model
-------------------------------------------------

As the first step, the collector system needs to be prepared to accept PUSH 
data from clients. We install PCP, ensure 2 daemons are started, and allow 
incoming TCP connections to port 44322. For example, on Fedora:

.. code-block:: bash

    # dnf \-y install pcp
    [..]
    # systemctl enable --now pmcd pmproxy
    # firewall-cmd --permanent --add-port 44322/tcp
    success
    # systemctl reload firewalld

From now on clients can send their data to the collector system.

Example of sending a pmlogger archive via PUSH
----------------------------------------------

On a system with already configured and running pmlogger, archives with 
metrics are available. These can be pushed to the collector system like this:

.. code-block:: bash

    # pmlogpush -h collect.example.com \
    /var/log/pcp/pmlogger/fedora44.local/20260608.14.45-00

On our collecthost, pmproxy is receiving and storing the data. If all went 
well, it will now receive and store the data we sent. Verification:

.. code-block:: bash

    # ls -al /var/log/pcp/pmproxy/
    drwxr-xr-x. 2 pcp pcp 4096 Jun  8 15:25 fedora44.local
    [..]

The data has been stored again in PCP archive files, can be investigated with 
tools like ``pmrep``, visualized via Grafana and so on.



Example of having a client system send metrics data live, via PUSH
------------------------------------------------------------------

To configure the client to constantly send data, the following can be used. 
We are modifying an existing pmlogger config which stores metrics locally in 
archive files:

.. code-block:: bash

    # cd /etc/pcp/pmlogger/control.d
    # mv local /tmp
    # echo 'LOCALHOSTNAME   y   n   +PCP_ARCHIVE_DIR/LOCALHOSTNAME  -r -T24h10m -c config.default -v 100Mb http://collector.example.com:44322' >remote
    # systemctl restart pmlogger

We are moving the previously used file ‘local’ away, to deactivate the 
logging into local files on the remote system. We then create a file 
‘remote’, including a reference to our collector system. Then we restart 
pmlogger. On the collector system, we see now the incoming data being 
reported:

.. code-block:: bash

    # pcp
    Performance Co-Pilot configuration on collector.example.com:
    [..]
    pmlogger: primary logger: /var/log/pcp/pmlogger/collector.example.com/20260608.04.58
    pmproxy: /var/log/pcp/pmproxy/fedora44.local/20260608.16.39
    [..]

The line starting with “pmlogger” is referring to our collector system 
itself. The line starting with ”pmproxy” is reporting the incoming data 
from our remote system. The pmlogsummary utility can show details:

.. code-block:: bash

    # pmlogsummary -a \
    /var/log/pcp/pmproxy/fedora44.local/20260608.16.39|head -4
    Log Label (Log Format Version 3\)
    Performance metrics from host fedora44.local
    commencing Mon Jun  8 16:39:11.150408313 2026
    ending     Mon Jun  8 16:59:21.212261127 2026

This confirms the archive files are now spanning a time frame of 20 minutes. 
