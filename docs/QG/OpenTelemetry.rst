.. _ImportData:

PCP with OpenTelemetry
################################################

.. contents::

Overview
***********

Unlock full OpenTelemetry interoperability within PCP using two powerful new tools: 
**pcp2opentelemetry** for metric egress and **pmdaopentelemetry** for metric ingress.

Prerequisites
**************

* pcp-export-pcp2opentelemetry package
* pcp-pmda-opentelemetry package
* pmcd enabled and running

Exporting PCP Metrics to OpenTelemetry
**************************************

* Basic metric query:

   .. code-block:: bash

      # pcp2opentelemetry -s 1 hinv.ncpu

* Send a single metric to a local collector:

   .. code-block:: bash

      # pcp2opentelemetry -s 1 -u http://127.0.0.1:4318/v1/metrics kernel.all.load

* Send multiple metrics to a pre-configured OpenTelemetry collector every 5 seconds:

   .. code-block:: bash

      # pcp2opentelemetry -t 5s -u http://otel-collector:4318/v1/metrics disk.dev.write disk.dev.read

* Exporting archive logs:

   .. code-block:: bash

      # pcp2opentelemetry -a /var/log/pmlogger/hostname/20260101 -u http://127.0.0.1:4318/v1/metrics

* Specifying a configuration file:

   .. code-block:: bash

      # pcp2opentelemetry -c /etc/pcp/pcp2opentelemetry/pcp2opentelemetry.conf

Importing OpenTelemetry Metrics into PCP
****************************************

Installation
============

Install and enable the OpenTelemetry PMDA:

   .. code-block:: bash

      # cd $PCP_PMDAS_DIR/opentelemetry
      # sudo ./Install

Verify the PMDA is running:

   .. code-block:: bash

      # pminfo -f opentelemetry.control.debug

      opentelemetry.control.debug
          value 0

Configuration
=============

The PMDA reads configuration files from **$PCP_PMDAS_DIR/opentelemetry/config.d/**

Configuration files can be either:

* **.url files** - containing OpenTelemetry endpoint URLs
* **executable scripts** - that output OpenTelemetry formatted metrics

URL Configuration Example
-------------------------

Create a configuration file for a remote endpoint:

   .. code-block:: bash

      # cat > $PCP_PMDAS_DIR/opentelemetry/config.d/app-metrics.url << 'EOF'
      http://localhost:4318/v1/metrics
      HEADER: Content-Type: application/json
      HEADER: Authorization: Bearer YOUR_TOKEN_HERE
      EOF

For local file access:

   .. code-block:: bash

      # cat > $PCP_PMDAS_DIR/opentelemetry/config.d/local-metrics.url << 'EOF'
      file:///var/log/otel/metrics.json
      EOF

Metric Filtering
----------------

Include only specific metrics using FILTER directives:

   .. code-block:: bash

      # cat > $PCP_PMDAS_DIR/opentelemetry/config.d/filtered-app.url << 'EOF'
      http://app-server:4318/v1/metrics
      FILTER: INCLUDE METRIC http_.*
      FILTER: INCLUDE METRIC cpu_.*
      FILTER: EXCLUDE METRIC .*
      EOF

This configuration includes only HTTP and CPU metrics, excluding all others.

Exclude unwanted metrics:

   .. code-block:: bash

      FILTER: EXCLUDE METRIC debug_.*
      FILTER: EXCLUDE METRIC test_.*

Label Filtering
---------------

Control which labels appear in metric instances:

   .. code-block:: bash

      # Exclude high-cardinality labels
      FILTER: EXCLUDE LABEL request_id
      FILTER: EXCLUDE LABEL trace_id

      # Make non-identifying labels optional
      FILTER: OPTIONAL LABEL hostname
      FILTER: OPTIONAL LABEL version

Metric Discovery and Verification
==================================

List all available OpenTelemetry metrics:

   .. code-block:: bash

      # pminfo opentelemetry

List metrics from a specific source (matching config filename):

   .. code-block:: bash

      # pminfo opentelemetry.app_metrics

View metric values with instance details:

   .. code-block:: bash

      # pminfo -f opentelemetry.app_metrics.http_requests_total

      opentelemetry.app_metrics.http_requests_total
          inst [0 or "0 method=GET,status=200"] value 1523
          inst [1 or "1 method=POST,status=201"] value 89

View metric metadata:

   .. code-block:: bash

      # pminfo -d opentelemetry.app_metrics.http_requests_total

Common Use Cases
================

Monitoring Kubernetes Applications
-----------------------------------

Configure endpoint for a Kubernetes service exposing OpenTelemetry metrics:

   .. code-block:: bash

      # cat > $PCP_PMDAS_DIR/opentelemetry/config.d/k8s-app.url << 'EOF'
      http://my-app-service.default.svc.cluster.local:4318/v1/metrics
      FILTER: INCLUDE METRIC request_.*
      FILTER: INCLUDE METRIC response_.*
      FILTER: OPTIONAL LABEL pod_name
      FILTER: OPTIONAL LABEL namespace
      EOF

Combining with Native PCP Metrics
----------------------------------

Query both OpenTelemetry and native PCP metrics together:

   .. code-block:: bash

      # pmrep opentelemetry.app_metrics.cpu_usage kernel.all.cpu.idle disk.dev.total

Removal
=======

   .. code-block:: bash

      # cd $PCP_PMDAS_DIR/opentelemetry
      # sudo ./Remove

Troubleshooting and Debugging
*****************************

Enable Debugging

* Enabling verbose outputs using command flags **(-D)**
* Dynamically toggle live debug mode for pmdaopentelemetry

   .. code-block:: bash
        
        # pmstore opentelemetry.control.debug 1

Checkout the log files:

* pcp2opentelemetry -- **system logs**

* pmdaopentelemetry -- **$PCP_LOG_DIR/pmcd/opentelemetry.log**

Next steps
**********

See **man pcp2opentelemetry** and **man pmdaopentelemetry** for more information.

See also **man PMWEBAPI** for OpenTelemetry metric egress using the /metrics API.
