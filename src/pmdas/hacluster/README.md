# Performance Co-Pilot PMDA for Monitoring HA Clusters

This PMDA is capable of collecting metric information to enable the monitoring of Pacemaker based HA Clusters through Performance Co-Pilot.

The PMDA collects it's metric data from the following components that make up a Pacemaker based HA Cluster: Pacemaker, Corosync, SBD, DRBD.

## General Notes

### `ha_cluster.drbd.split_brain`

This metric signals if there is a split brain occurring in DRBD per instance resource:volume. The metric will return the value `1` if a split brain is detected, otherwise it will be `0`.

In order for this metric to function, you will need to set-up a custom DRBD split-brain handler.

#### Setting up the DRBD split-brain handler

1) Copy the hook into all DRBD nodes:

The hook is available from:
https://github.com/SUSE/ha-sap-terraform-deployments/blob/72c9d3ecf6c3f6dd18ccb7bcbde4b40722d5c641/salt/drbd_node/files/notify-split-brain-haclusterexporter-suse-metric.sh

2) on the DRBD configuration enable the hook:

```split_brain: "/usr/lib/drbd/notify-split-brain-haclusterexporter-suse-metric.sh"```

Refer to following upstream doc: https://docs.linbit.com/docs/users-guide-8.4/#s-configure-split-brain-behavior

It is important for the PMDA that the handler creates the files in that location and with the naming outlined in the handler as is.

Note: that the created split-brain detection files will need to be manually removed after the split brain is resolved.

### Further information on Metrics

The file ./help contains descriptions for all of the metrics which are
exposed by this PMDA.

Once the PMDA has been installed, the following command will list all of
the available metrics and their explanatory “help” text:

		# $ pminfo -fT ha_cluster

## Installation

		# cd $PCP_PMDAS_DIR/hacluster

Check that there is no clash in the Performance Metrics Domain defined in ./domain.h and the other PMDA's currently in use (see $PCP_PMCDCONF_PATH). If there is, edit ./domain.h to choose another domain number (This should only be an issue on installations with third party PMDA's installed as the domain number given has been reserved for the HACLUSTER PMDA with base PCP installations).

Then simply use:

		# ./Install

## De-Installation

Simply use:

		# cd $PCP_PMDAS_DIR/hacluster
		#./Remove

## Troubleshooting

After installing or restarting the agent, the PMCD log file ($PCP_LOG_DIR/pmcd/pmcd.log) and the PMDA log file ($PCP_LOG_DIR/PMCD/hacluster.log) should be checked for any warnings or errors.
