.. _ListAvailableMetrics:

List the available performance metrics
#################################################

1. To list all the available performance metrics:

.. code-block:: bash

   pminfo

2. To list a group of related metrics:: 

.. code-block:: bash

   pminfo <metric_name>
 
3. To get one-line help text for a performace metric:: 

.. code-block:: bash

   pminfo -t <metric_name> 

4. To get detailed help text for a performace metric:: 

.. code-block:: bash

   pminfo -T <metric_name> 

5. To search for a performace metric, when only part of the full name is known:: 

.. code-block:: bash

   pminfo | grep <part_of_full_name>

6. To verify whether the specified metric exists or not:: 

.. code-block:: bash

   pminfo <metric1_full_name> <metric2_full_name>
