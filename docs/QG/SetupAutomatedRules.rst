.. _SetupAutomatedRules:

Setup automated rules to write to the system log
############################################################

1. Open the command shell and start **pmieconf** as the superuser:

.. code-block:: bash

    $ cd /var/lib/pcp/config/pmieconf
    $ sudo su
    # mkdir mounts
    # cd mounts
    # [save the below code here in a file named as 'available']

.. code-block:: bash

    #pmieconf-rules 1
    # --- DO NOT MODIFY THIS FILE --- see pmieconf(5)
    #

    rule	mounts.available
        default	= "$rule$"
        predicate =
    "some_inst (
        mounts.up $hosts$ $interfaces$ != 1
    )"
        enabled	= yes
        version	= 1
        help	=
    "For at least one monitored mount point, a filesystem has not
    been detected mounted at the configured path for that mount.";

    string	rule
        default	= "Mount point is not available"
        modify	= no
        display	= no;

    instlist	interfaces
        default	= ""
        help	=
    "May be set to a list of configured mount points for which the rule will
    be evaluated, as a subset of all configured pmdamounts(1) mount points.
    Mount points should be separated by white space and may be enclosed in
    single quotes, eg.  \"/var /home\".  Use the command:
        $ pminfo -f mounts.up
    to discover the names of all currently configured mount points.";

    string	action_expand
        default	= "[%i]@%h"
        display	= no
        modify	= no;

    string	email_expand
        default	= "host: %h mount %i is not available"
        display	= no
        modify	= no;

    #
    # --- DO NOT MODIFY THIS FILE --- see pmieconf(5)

2. To locate the mount points:

.. code-block:: bash

    # pmieconf help mounts

    rule: mounts.available  [Mount point is not available]
    help: For at least one monitored mount point, a filesystem has not
        been detected mounted at the configured path for that mount.

3. Start pmieconf interactively (as the superuser).

.. code-block:: bash

    # pmieconf -f /var/lib/pcp/config/pmie/config.default

4. To enable the rules:

.. code-block:: bash

    pmieconf> enable mounts

5. Quit the pmieconf:

.. code-block:: bash

    pmieconf> quit

    /var/lib/pcp/config/pmie/config.default is in use by 1 running pmie process: 526929 

6. Restart this process for the configuration change to take effect:

.. note::

   *  Use kill(1) to stop; e.g.	``kill -INT 526929`` 
   *  Refer to pmie_check(1) for a convenient mechanism for restarting pmie
      daemons launched under the control of /etc/pcp/pmie/control;
      e.g. ``/usr/libexec/pcp/bin/pmie_check -V``

.. code-block:: bash

    # systemctl restart pmie

7. Check the status again:

.. code-block:: bash

    # pmieconf status -f /var/lib/pcp/config/pmie/config.default 

    verbose:  off
    enabled rules:  21 of 29
    pmie configuration file:  /var/lib/pcp/config/pmie/config.default
    pmie process (PID) using this file:  749473

8. Search for the messages:

.. code-block:: bash

    # grep pcp-pmie /var/log/messages