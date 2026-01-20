.. root pcp documentation file, created by
   sphinx-quickstart on Wed Sep 16 15:12:41 2020.

Performance Co-Pilot
####################

`Performance Co-Pilot (PCP) <https://pcp.io/>`_ provides a framework and services to support system-level performance monitoring and management. It presents a unifying abstraction for all of the performance data in a system, and many tools for interrogating, retrieving and processing that data.

PCP is a feature-rich, mature, extensible, cross-platform toolkit supporting both live and retrospective analysis. The distributed PCP architecture makes it especially useful for those seeking centralized monitoring of distributed processing.

**Table of Contents**

* :doc:`HowTos/installation/index`
* :doc:`QG/QuickReferenceGuide`
* :doc:`QG/QuickGuides`
* :doc:`UAG/AboutUserAdministratorsGuide`
* :doc:`PG/AboutProgrammersGuide`
* :doc:`HowTos/nix/index`
* :doc:`HowTos/scaling/index`
* `REST API Specification <api/>`_

.. toctree::
   :caption: Guides
   :hidden:

   HowTos/installation/index
   QG/QuickReferenceGuide
   QG/QuickGuides
   UAG/AboutUserAdministratorsGuide
   PG/AboutProgrammersGuide
   HowTos/nix/index
   HowTos/scaling/index
   REST API Specification <https://pcp.readthedocs.io/en/latest/api/>

.. toctree::
   :caption: Quick Guides
   :numbered:
   :hidden:

   QG/ListAvailableMetrics
   QG/AddNewMetrics
   QG/RecordMetricsOnLocalSystem
   QG/RecordMetricsFromRemoteSystem
   QG/GraphPerformanceMetric
   QG/AutomateProblemDetection
   QG/SetupAutomatedRules
   QG/RecordHistoricalValues
   QG/ExportMetricValues
   QG/UseCharts
   QG/LoggingBasics
   QG/AutomatedReasoningBasics
   QG/ConfigureAutomatedReasoning
   QG/AnalyzeLinuxContainers
   QG/EncryptedConnections
   QG/AuthenticatedConnections
   QG/ImportData
   QG/Use3DViews
   QG/CompareArchivesAndReportDifferences

.. toctree::
   :caption: User's and Administrator's Guide
   :numbered:
   :hidden:

   UAG/IntroductionToPcp
   UAG/InstallingAndConfiguringPcp
   UAG/CommonConventionsAndArguments
   UAG/MonitoringSystemPerformance
   UAG/PerformanceMetricsInferenceEngine
   UAG/ArchiveLogging
   UAG/PcpDeploymentStrategies
   UAG/CustomizingAndExtendingPcpServices
   UAG/TimeSeriesQuerying


.. toctree::
   :caption: Programmer's Guide
   :numbered:
   :hidden:

   PG/ProgrammingPcp
   PG/WritingPMDA
   PG/PMAPI
   PG/InstrumentingApplications
