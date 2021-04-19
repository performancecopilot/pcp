Installation
############

Distribution Packages
*********************

Performance Co-Pilot is already included in the most popular distributions (RHEL, Debian, Fedora, SUSE, Ubuntu).
These packages can be installed with the distribution package manager.

Upstream Packages
*****************

Additionally we build packages of each PCP version for multiple distribution on our custom build infrastructure.
These packages are usually more recent than the distribution packages.

Fedora / RHEL / CentOS
----------------------

Create a file ``/etc/yum.repos.d/performancecopilot.repo`` with the following content.
**Note:** Replace ``fedora`` with ``centos`` when using CentOS or RHEL.

.. code-block:: ini

    [performancecopilot]
    name=Performance Co-Pilot
    baseurl=https://performancecopilot.jfrog.io/artifactory/pcp-rpm-release/fedora/$releasever/$basearch
    enabled=1
    gpgcheck=0
    gpgkey=https://performancecopilot.jfrog.io/artifactory/pcp-rpm-release/fedora/$releasever/$basearch/repodata/repomd.xml.key
    repo_gpgcheck=1

Install PCP using the package manager:

.. code-block:: bash

    $ sudo dnf install pcp-zeroconf

Debian / Ubuntu
---------------

Run the following commands.
**Note:** Replace ``focal`` according to your installed Debian or Ubuntu release name.

.. code-block:: bash

    $ wget -qO - https://pcp.io/GPG-KEY-PCP | sudo apt-key add -
    $ echo 'deb https://performancecopilot.jfrog.io/artifactory/pcp-deb-release focal main' | sudo tee -a /etc/apt/sources.list
    $ sudo apt-get update
    $ sudo apt-get install pcp-zeroconf
