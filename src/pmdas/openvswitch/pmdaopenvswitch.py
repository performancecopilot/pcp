#!/usr/bin/env pmpython

# Copyright (C) 2020 Ashwin Nayak <ashwinnayak111@gmail.com>

# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.

# pylint: disable=bad-whitespace, line-too-long, too-many-return-statements
# pylint: disable=broad-except, too-many-branches, too-many-statements, inconsistent-return-statements
# pylint: disable=no-name-in-module, too-many-instance-attributes

""" Performance Metrics Domain Agent exporting openvswitch metrics. """

import json
import subprocess
from pcp.pmda import PMDA, pmdaMetric, pmdaIndom, pmdaInstid
from pcp.pmapi import pmUnits
from pcp.pmapi import pmContext as PCP
from cpmapi import PM_TYPE_STRING
from cpmapi import PM_SEM_COUNTER, PM_SEM_DISCRETE
from cpmapi import PM_ERR_APPVERSION
from cpmda import PMDA_FETCH_NOVALUES

class OpenvswitchPMDA(PMDA):
    """ PCP openvswitch PMDA """
    def __init__(self, name, domain):
        """ (Constructor) Initialisation - register metrics, callbacks, drop privileges """
        PMDA.__init__(self, name, domain)
        self.bridge_info_json = dict()
        self.bridge_names = []
        self.get_bridge_info_json()

        self.connect_pmcd()

        self.bridge_indom = self.indom(0)
        self.bridge_instances()
        self.bridge_cluster = 0
        self.bridge_metrics = [
            # Name - type - semantics - units - help
            [ 'bridge.uuid',                                                            PM_TYPE_STRING, PM_SEM_DISCRETE, pmUnits(),  'bridge id'], # 0
            [ 'bridge.autoattach',                                                      PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set:Shortest Path Bridging (SPB) network to automatically attach network devices to individual services in a SPB network'], # 1
            [ 'bridge.controller',                                                      PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set:controller'], # 2
            [ 'bridge.datapath_id',                                                     PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'datapath_id'], # 3
            [ 'bridge.datapath_type',                                                   PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'datapath_type'], # 4
            [ 'bridge.datapath_version',                                                PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'datapath_version'], #5
            [ 'bridge.external_ids',                                                    PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'map:These values are  intended to identify entities external to Open vSwitch with which bridge is associated, e.g. the bridges  identifier  in  avirtualization  management  platform.  The Open vSwitch databaseschema specifies well-known key values, but key  and  value  are otherwise arbitrary strings'], # 6
            [ 'bridge.fail_mode',                                                       PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set:failmode'], # 7
            [ 'bridge.flood_vlans',                                                     PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set: flood_vlans'], # 8
            [ 'bridge.flow_tables',                                                     PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'map: flow tables'], # 9
            [ 'bridge.ipfix',                                                           PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'The IPFIX Protocol Specification has been designed to be transport protocol independent.'], # 10
            [ 'bridge.mcast_snooping_enable',                                           PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'multicast snooping status'], # 11
            [ 'bridge.mirrors',                                                         PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set:packets mirrored'], # 12
            [ 'bridge.netflow',                                                         PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'NetFlow is a network protocol developed by Cisco for collecting IP traffic information and monitoring network traffic. By analyzing flow data, a picture of network traffic flow and volume can be built.'], # 14
            [ 'bridge.other_config',                                                    PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'other configs to bridge'], # 15
            [ 'bridge.ports',                                                           PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set:list of ports attached to the bridge'], # 16
            [ 'bridge.protocols',                                                       PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set: protocols ex:openflow12'], # 17
            [ 'bridge.rstp_enable',                                                     PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'rapid spanning tree protocol enabled status'], # 18
            [ 'bridge.rstp_status',                                                     PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'rapid spanning tree protocol status'], # 19
            [ 'bridge.sflow',                                                           PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'sFlow is a sampling technology that meets the key requirements for a network traffic monitoring solution'], # 20
            [ 'bridge.status',                                                          PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'status'], # 21
            [ 'bridge.stp_enable',                                                      PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'spanning tree protocol'] # 22
        ]

        for item in range(len(self.bridge_metrics)):
            self.add_metric(name + '.' +
                            self.bridge_metrics[item][0],
                            pmdaMetric(self.pmid(self.bridge_cluster, item),
                                       self.bridge_metrics[item][1],
                                       self.bridge_indom,
                                       self.bridge_metrics[item][2],
                                       self.bridge_metrics[item][3]),
                            self.bridge_metrics[item][4],
                            self.bridge_metrics[item][4])

        self.set_fetch_callback(self.openvswitch_fetch_callback)
        self.set_refresh(self.openvswitch_refresh)
        self.set_user(PCP.pmGetConfig('PCP_USER'))

    def fetch_bridge_info(self):
        """ fetches result from command line """

        query = ['sudo', 'ovs-vsctl', '--format=json', 'list', 'bridge']
        out = subprocess.Popen(query, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout, stderr = out.communicate()
        stdout = stdout.decode("utf-8")
        return stdout, stderr

    def get_bridge_info_json(self):
        """ Convert the commandline output to json """

        stdout, stderr = self.fetch_bridge_info()

        if stderr is None:
            temp = json.loads(stdout)
            # reorganize json a bit
            self.bridge_info_json = dict()
            self.bridge_names = []
            for idx in range(len(temp)):
                self.bridge_info_json[str(temp["data"][idx][13])] = temp["data"][idx]
                self.bridge_names.append(str(temp["data"][idx][13]))

    def bridge_instances(self):
        """ set names for openvswitch instances """
        insts = []
        for idx, val in enumerate(self.bridge_names):
            insts.append(pmdaInstid(idx, val))
        self.add_indom(pmdaIndom(self.bridge_indom, insts))

    def openvswitch_refresh(self, cluster):
        """refresh function"""
        if cluster == self.bridge_cluster:
            # self.get_bridge_info_json()
            insts = []
            for idx, val in enumerate(self.bridge_names):
                insts.append(pmdaInstid(idx, val))

            self.replace_indom(self.bridge_indom, insts)

    def openvswitch_fetch_callback(self, cluster, item, inst):
        """ fetch callback method"""

        # self.get_bridge_info_json()

        if cluster == self.bridge_cluster:
            if self.bridge_info_json is None:
                return [PMDA_FETCH_NOVALUES]
            try:
                bridge = self.inst_name_lookup(self.bridge_indom,inst)
                if item == 0:
                    return [str(self.bridge_info_json[bridge][0][1]),1]
                if item == 1:
                    return [str(self.bridge_info_json[bridge][1][1]),1]
                if item == 2:
                    return [str(self.bridge_info_json[bridge][2][1]),1]
                if item == 3:
                    return [str(self.bridge_info_json[bridge][3]),1]
                if item == 4:
                    return [str(self.bridge_info_json[bridge][4]),1]
                if item == 5:
                    return [str(self.bridge_info_json[bridge][5]),1]
                if item == 6:
                    return [str(self.bridge_info_json[bridge][6][1]),1]
                if item == 7:
                    return [str(self.bridge_info_json[bridge][7][1]),1]
                if item == 8:
                    return [str(self.bridge_info_json[bridge][8][1]),1]
                if item == 9:
                    return [str(self.bridge_info_json[bridge][9][1]),1]
                if item == 10:
                    return [str(self.bridge_info_json[bridge][10][1]),1]
                if item == 11:
                    return [str(self.bridge_info_json[bridge][11]),1]
                if item == 12:
                    return [str(self.bridge_info_json[bridge][12][1]),1]
                if item == 14:
                    return [str(self.bridge_info_json[bridge][14][1]),1]
                if item == 15:
                    return [str(self.bridge_info_json[bridge][15][1]),1]
                if item == 16:
                    return [str(self.bridge_info_json[bridge][16]),1]
                if item == 17:
                    return [str(self.bridge_info_json[bridge][17][1]),1]
                if item == 18:
                    return [str(self.bridge_info_json[bridge][18]),1]
                if item == 19:
                    return [str(self.bridge_info_json[bridge][19][1]),1]
                if item == 20:
                    return [str(self.bridge_info_json[bridge][20][1]),1]
                if item == 21:
                    return [str(self.bridge_info_json[bridge][21][1]),1]
                if item == 22:
                    return [str(self.bridge_info_json[bridge][22]),1]

            except Exception:
                return [PM_ERR_APPVERSION,0]

if __name__ == '__main__':
    OpenvswitchPMDA('openvswitch', 126).run()
