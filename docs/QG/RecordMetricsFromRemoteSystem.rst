.. _RecordMetricsFromRemoteSystem:

Record metrics from a remote system
###############################################

1. Setup instructions for client systems

   * Install required packages:

   .. code-block:: bash

      # yum -y install pcp
   
   * Allow incoming connections on TCP/44321:

   .. code-block:: bash

      # firewall-cmd --permanent --zone=public --add-port=44321/tcp
      # firewall-cmd --reload

   * Ensure that PMCD talks to the outer world:

   .. code-block:: bash

      #  if grep -q ^PMCD_LOCAL /etc/sysconfig/pmcd; then
            sed -ie 's,PMCD_LOCAL.*,PMCD_LOCAL=0,' /etc/sysconfig/pmcd
         else
            echo 'PMCD_LOCAL=0' >>/etc/sysconfig/pmcd
         fi
      # grep ^PMCD_LOCAL /etc/sysconfig/pmcd
      PMCD_LOCAL=0

   * Restart & activate PMCD:

   .. code-block:: bash

      # systemctl restart pmcd
      # systemctl enable pmcd

2. Setup instructions for master (collector) system

   * Install required packages:

   .. code-block:: bash

      # yum -y install pcp-zeroconf


   * Create a default config for the clients:

   .. code-block:: bash

      # CLIENT='rhel7u8a'
      # /usr/libexec/pcp/bin/pmlogconf /var/lib/pcp/config/pmlogger/config.$CLIENT

   * Optionally: execute again, to customize:

   .. code-block:: bash

      # /usr/libexec/pcp/bin/pmlogconf /var/lib/pcp/config/pmlogger/config.$CLIENT

   * Create the controller config for the client:

   .. code-block:: bash

      # echo "$CLIENT.local n n PCP_LOG_DIR/pmlogger/$CLIENT.local" " -r -T30d -c config.$CLIENT" \
      >/etc/pcp/pmlogger/control.d/$CLIENT

   * Restart pmlogger:

   .. code-block:: bash

      # /usr/libexec/pcp/bin/pmlogger_check

3. Verify data collection on master (collector) system

   .. code-block:: bash

      # pcp

      Performance Co-Pilot configuration on rhel8u2a.local:
      platform: Linux rhel8u2a.local 4.18.0-167.el8.x86_64 #1 SMP Sun [..] 2019 x86_64
      hardware: 4 cpus, 1 disk, 1 node, 3938MB RAM
      timezone: CET-1
      services: pmcd
      pmcd: Version 5.0.2-1, 11 agents, 4 clients
      pmda: root pmcd proc pmproxy xfs linux nfsclient mmv kvm jbd2 dm
      pmlogger: primary logger: /var/log/pcp/pmlogger/rhel8u2a.local/20200317.05.32
      rhel7u8a: /var/log/pcp/pmlogger/rhel7u8a.local/20200317.06.20
      pmie: primary engine: /var/log/pcp/pmie/rhel8u2a.local/pmie.log
