.. _ExportMetricValues:

Export metric values in a comma-separated format
############################################################

1. To display network interface metrics on the local host:

.. code-block:: bash
    
    $ pmrep network.interface.total.bytes

2. To display all outgoing network metrics for the wlan0 interface:
           
.. code-block:: bash

    $ pmrep -i wlan0 -v network.interface.out

3. To display the three most CPU-using processes:
           
.. code-block:: bash

    $ pmrep -1gUJ 3 proc.hog.cpu

.. Important::

   *pmrep* use *output* target for reporting. The default target is *stdout*. **-o csv** or **--output=csv** print metrics in CSV (comma-separated values) format.

4. To display per-device disk reads and writes from the host server1 using two seconds interval in CSV output format:

.. code-block:: bash

    $ pmrep -h server1 -t 2s -o csv -k disk.dev.read disk.dev.write

.. Note::

   -l delimiter, --delimiter=delimiter
   
   Specify the delimiter that separates each column of csv or stdout output. The default for stdout is two spaces (“  ”) and comma (“,”) for csv. In case of CSV output or stdout output with non-whitespace delimiter, any instances of the delimiter in string values will be replaced by the underscore (“_”) character.

