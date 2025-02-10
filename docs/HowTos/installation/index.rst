Installation
############

Distribution Packages
*********************

Performance Co-Pilot is already included in all popular Linux distributions (RHEL, Debian, Fedora, SUSE, Ubuntu).
These packages can be installed with the distribution package manager.

Upstream Packages
*****************

Additionally we build packages of each PCP version for multiple distribution on our custom build infrastructure.
These packages are usually more recent than the distribution packages.

Fedora / RHEL / CentOS
----------------------

Follow the ``Quick install instructions`` for RPM.

Install PCP using the package manager:

.. code-block:: bash

    $ sudo dnf update
    $ sudo dnf install pcp-zeroconf

Debian / Ubuntu
---------------

Follow the ``Quick install instructions`` for Debian.

Install PCP using the package manager:

.. code-block:: bash

    $ sudo apt-get update
    $ sudo apt-get install pcp-zeroconf
