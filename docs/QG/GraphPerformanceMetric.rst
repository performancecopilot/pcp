.. _GraphPerformanceMetric:

Graph a performance metric
################################################

1. Open the command shell and start *pmchart*:

.. code-block:: bash

    pmchart

2. From the File menu select the 'New Chart' option.

3. The three tabs on the right side of the window show the current settings for the chart in three categories:

    * Chart : Properties relating to the entire chart (Title, Legend settings, Y-Axis scaling, etc);

    * Metrics : Available metrics (performance data) for plotting in the chart

    * Plots : Properties related to each individual chart plot (color, label)

4. In the Available Metrics list on the Metrics tab, select leaf nodes of any metric.

5. Now, the “Add Metric” button and the “Metric Info” button become enabled.

6. Click on the “Metric Info” button to view the metric descriptor (pminfo) and current values (pmval). Dismiss that dialog, and then press the “Add Metric” button, to add this metric into the list of metrics for our new chart.

7. To save the graphs, from the File menu select the Save View option. Save the current charts to a View named /tmp/ExampleView.

8. In a command shell, take a look through the saved View configuration:

.. code-block:: bash

    $ cat /tmp/ExampleView
