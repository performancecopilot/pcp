.. _ListAvailableMetrics:

List the available performance metrics
#################################################

1. To list all the available performance metrics:

.. code-block:: bash

   pminfo

2. To list a group of related metrics:: 

.. code-block:: bash

   pminfo <metric_name_prefix>

.. Note::

   ``<metric_name_prefix>`` requests all metrics whose names match with either ``<metric_name_prefix>`` or ``<metric_name_prefix>.<anything>``

For more information on performace metrics name space, please read `here <https://pcp.readthedocs.io/en/latest/UAG/IntroductionToPcp.html#performance-metrics-name-space>`_.

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

**Check out the video guide for more information on pminfo:**

.. raw:: html

   <div style="position:relative; margin-left:auto; margin-right:auto; height:490px; width:448.562px;">
      <script type="text/javascript" src="https://asciinema.org/a/420306.js" 
         id="asciicast-420306" async 
         data-autoplay="false" data-loop="false">
      </script>
   </div>
