.. _CompareArchivesAndReportDifferences:

Compare Archives and Report Significant Differences
###############################################################

* *pmdiff* compares the average values for every metric in either one or two sets of archives, in a given time window, for changes that
  are likely to be of interest when searching for performance regressions.

* In PCP, *pmdiff* (1) is a tool that takes our recordings and time windows of interest ("Tuesday 10 a.m. all was well, but in the half hour after midday everything broke loose"), 
  analyzes like-to-like metrics amongst the many thousands of recorded values, and reports back with those metrics having the most variance — automating the task of separating out 
  the performance noise. 

.. code-block:: bash

    pmdiff -X ./cull --threshold 10 --start @10:00 --finish @10:30 --begin @12:00 --end @12:30 ./archives/app3/20120510 | less

Let's understand the above command.

* A recording from the day of our performance crisis is passed into *pmdiff* (./archives/app3/20120510) along with two time windows of interest (--start/ --finish for the "before" window, --begin/ --end for "after")

* The tool reports four columns: "before" values, "after" values, how much those average values changed (Ratio), and individual performance metric names (Metric-Instance).

* The --threshold parameter (10) sets the point at which the Ratio column should be culled. We look for average values that are 10x (or more) and 1/10th (or less) between the time windows.

* The first five rows show a Ratio of ``|+|`` - this simply indicates that the average value changed from completely zero "before" to non-zero "after."  Interestingly, these are all metrics relating to the Linux virtual memory subsystem—our first insight.

* The next 15 or so rows contain the value >100 in the Ratio column (i.e., the average values for these metrics during the second time window has increased by more than 100 times!). Again, we have strong indicators that page compaction (a function of the 
  kernel’s virtual memory subsystem) is behaving in radically different ways during the two time windows. We also see aggregate disk read I/O is way up, and we see the specific device (the ``sda`` metric instance) that has caused this change.

**Check out the video guide on pmdiff:**

.. raw:: html

   <div style="position:relative; margin-left:auto; margin-right:auto; height:490px; width:448.562px;">
      <script type="text/javascript" src="https://asciinema.org/a/420315.js" 
         id="asciicast-420315" async 
         data-autoplay="false" data-loop="false">
      </script>
   </div>