.. _AddNewMetrics:

Add new metrics to the available set
################################################

1. Open the command shell and run the corresponding script:

.. code-block:: bash

    $ cd /var/lib/pcp/pmdas/mounts
    $ cat mounts.conf
    $ sudo su
    # echo / > mounts.conf
    # echo /home >> mounts.conf
    # echo /production >> mounts.conf

2. Now, run the corresponding install script:

.. code-block:: bash

    # ./Install 

    Updating the Performance Metrics Name Space (PMNS) ...
    Terminate PMDA if already installed ...
    Updating the PMCD control file, and notifying PMCD ...
    Check mounts metrics have appeared ... 15 metrics and 34 values

3. To display the new available metrics values:

.. code-block:: bash

    # pminfo -f mounts.up

    mounts.up
        inst [0 or "/"] value 1
        inst [1 or "/home"] value 1
        inst [2 or "/production"] value 0

4. To display all the enabled performance metrics on a host with a short description:

.. code-block:: bash

    # pminfo -t mounts
