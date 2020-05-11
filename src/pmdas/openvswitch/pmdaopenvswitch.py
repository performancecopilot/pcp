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
# pylint: disable=no-name-in-module, too-many-instance-attributes, no-self-use

""" Performance Metrics Domain Agent exporting openvswitch metrics. """

import os
import json
import subprocess
from pcp.pmda import PMDA, pmdaMetric, pmdaIndom, pmdaInstid
from pcp.pmapi import pmUnits
from pcp.pmapi import pmContext as PCP
from cpmapi import PM_TYPE_STRING, PM_TYPE_U64, PM_TYPE_FLOAT
from cpmapi import PM_SEM_COUNTER, PM_SEM_DISCRETE, PM_SEM_INSTANT
from cpmapi import PM_COUNT_ONE, PM_SPACE_BYTE, PM_TIME_SEC
from cpmapi import PM_ERR_APPVERSION
from cpmda import PMDA_FETCH_NOVALUES
import cpmapi as c_api

PMDA_DIR = PCP.pmGetConfig('PCP_PMDAS_DIR')

class OpenvswitchPMDA(PMDA):
    """ PCP openvswitch PMDA """
    def __init__(self, name, domain):
        """ (Constructor) Initialisation - register metrics, callbacks, drop privileges """
        PMDA.__init__(self, name, domain)
        self.switch_info_json = dict()
        self.port_info_json = dict()
        self.flow_json = dict()
        self.switch_names = []
        self.port_info_names = []
        self.flow_names = []
        self.get_switch_info_json()
        self.get_port_info_json()
        self.get_flow_json()

        self.connect_pmcd()

        self.switch_indom = self.indom(0)
        self.switch_instances()
        self.switch_cluster = 0
        self.switch_metrics = [
            # Name - type - semantics - units - help
            [ 'switch.uuid',                                                            PM_TYPE_STRING, PM_SEM_DISCRETE, pmUnits(),  'switch id'], # 0
            [ 'switch.autoattach',                                                      PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set:Shortest Path Bridging (SPB) network to automatically attach network devices to individual services in a SPB network'], # 1
            [ 'switch.controller',                                                      PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set:controller'], # 2
            [ 'switch.datapath_id',                                                     PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'datapath_id'], # 3
            [ 'switch.datapath_type',                                                   PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'datapath_type'], # 4
            [ 'switch.datapath_version',                                                PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'datapath_version'], #5
            [ 'switch.external_ids',                                                    PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'map:These values are  intended to identify entities external to Open vSwitch with which switch is associated, e.g. the switchs  identifier  in  avirtualization  management  platform.  The Open vSwitch databaseschema specifies well-known key values, but key  and  value  are otherwise arbitrary strings'], # 6
            [ 'switch.fail_mode',                                                       PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set:failmode'], # 7
            [ 'switch.flood_vlans',                                                     PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set: flood_vlans'], # 8
            [ 'switch.flow_tables',                                                     PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'map: flow tables'], # 9
            [ 'switch.ipfix',                                                           PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'The IPFIX Protocol Specification has been designed to be transport protocol independent.'], # 10
            [ 'switch.mcast_snooping_enable',                                           PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'multicast snooping status'], # 11
            [ 'switch.mirrors',                                                         PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set:packets mirrored'], # 12
            [ 'switch.netflow',                                                         PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'NetFlow is a network protocol developed by Cisco for collecting IP traffic information and monitoring network traffic. By analyzing flow data, a picture of network traffic flow and volume can be built.'], # 14
            [ 'switch.other_config',                                                    PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'other configs to switch'], # 15
            [ 'switch.ports',                                                           PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set:list of ports attached to the switch'], # 16
            [ 'switch.protocols',                                                       PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'set: protocols ex:openflow12'], # 17
            [ 'switch.rstp_enable',                                                     PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'rapid spanning tree protocol enabled status'], # 18
            [ 'switch.rstp_status',                                                     PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'rapid spanning tree protocol status'], # 19
            [ 'switch.sflow',                                                           PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'sFlow is a sampling technology that meets the key requirements for a network traffic monitoring solution'], # 20
            [ 'switch.status',                                                          PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'status'], # 21
            [ 'switch.stp_enable',                                                      PM_TYPE_STRING, PM_SEM_COUNTER, pmUnits(),  'spanning tree protocol'] # 22
        ]

        for item in range(len(self.switch_metrics)):
            self.add_metric(name + '.' +
                            self.switch_metrics[item][0],
                            pmdaMetric(self.pmid(self.switch_cluster, item),
                                       self.switch_metrics[item][1],
                                       self.switch_indom,
                                       self.switch_metrics[item][2],
                                       self.switch_metrics[item][3]),
                            self.switch_metrics[item][4],
                            self.switch_metrics[item][4])

        self.port_info_indom = self.indom(1)
        self.port_info_instances()
        self.port_info_cluster = 1
        self.port_info_metrics = [
            # Name - type - semantics - units - help
            [ 'port_info.rx.pkts',                                                     PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''], # 0
            [ 'port_info.rx.bytes',                                                    PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(1,0,0,PM_SPACE_BYTE,0,0),  ''], # 1
            [ 'port_info.rx.drop',                                                     PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''], # 2
            [ 'port_info.rx.errs',                                                     PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''], # 3
            [ 'port_info.rx.frame',                                                    PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''], # 4
            [ 'port_info.rx.over',                                                     PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''], #5
            [ 'port_info.rx.crc',                                                      PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''], # 6
            [ 'port_info.tx.pkts',                                                     PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''], # 7
            [ 'port_info.tx.bytes',                                                    PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(1,0,0,PM_SPACE_BYTE,0,0),  ''], # 8
            [ 'port_info.tx.drop',                                                     PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''], # 9
            [ 'port_info.tx.errs',                                                     PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''], # 10
            [ 'port_info.tx.coll',                                                     PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''] # 11
        ]

        for item in range(len(self.port_info_metrics)):
            self.add_metric(name + '.' +
                            self.port_info_metrics[item][0],
                            pmdaMetric(self.pmid(self.port_info_cluster, item),
                                       self.port_info_metrics[item][1],
                                       self.port_info_indom,
                                       self.port_info_metrics[item][2],
                                       self.port_info_metrics[item][3]),
                            self.port_info_metrics[item][4],
                            self.port_info_metrics[item][4])

        self.flow_indom = self.indom(2)
        self.flow_instances()
        self.flow_cluster = 2
        self.flow_metrics = [
            # Name - type - semantics - units - help
            [ 'flow.cookie',                                                           PM_TYPE_STRING, PM_SEM_INSTANT, pmUnits(),  ''], # 0
            [ 'flow.duration',                                                         PM_TYPE_FLOAT, PM_SEM_INSTANT, pmUnits(0,1,0,0,PM_TIME_SEC,0),  ''], # 1
            [ 'flow.table',                                                            PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''], # 2
            [ 'flow.n_packets',                                                        PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''], # 3
            [ 'flow.n_bytes',                                                          PM_TYPE_U64, PM_SEM_INSTANT, pmUnits(0,0,1,0,0,PM_COUNT_ONE),  ''], # 4
            [ 'flow.actions',                                                          PM_TYPE_STRING, PM_SEM_DISCRETE, pmUnits(),  ''], #5
        ]

        for item in range(len(self.flow_metrics)):
            self.add_metric(name + '.' +
                            self.flow_metrics[item][0],
                            pmdaMetric(self.pmid(self.flow_cluster, item),
                                       self.flow_metrics[item][1],
                                       self.flow_indom,
                                       self.flow_metrics[item][2],
                                       self.flow_metrics[item][3]),
                            self.flow_metrics[item][4],
                            self.flow_metrics[item][4])


        self.set_fetch_callback(self.openvswitch_fetch_callback)
        self.set_refresh(self.openvswitch_refresh)
        self.set_label(self.openvswitch_label)
        self.set_label_callback(self.openvswitch_label_callback)

    def fetch_switch_info(self):
        """ fetches result from command line """
        query = ['ovs-vsctl --format=json list bridge']
        out = subprocess.Popen(query, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
        stdout, stderr = out.communicate()
        stdout = stdout.decode("utf-8")
        return stdout, stderr

    def get_switch_info_json(self):
        """ Convert the commandline output to json """
        stdout, _ = self.fetch_switch_info()
        # temp = json.loads(stdout)

        # # Store it in a file
        text_file = open("switch_output.txt", "w")
        text_file.write(stdout)
        text_file.close()

        # Read
        with open(os.path.join(PMDA_DIR, 'openvswitch','switch_output.txt'), 'r') as file:
            temp = json.load(file)

        # reorganize json a bit
        self.switch_info_json = dict()
        self.switch_names = []
        for idx in range(len(temp["data"])):

            self.switch_info_json[str(temp["data"][idx][13])] = temp["data"][idx]
            self.switch_names.append(str(temp["data"][idx][13]))

    def switch_instances(self):
        """ set names for openvswitch  switch instances """
        insts = []
        for idx, val in enumerate(self.switch_names):
            insts.append(pmdaInstid(idx, val))
        self.add_indom(pmdaIndom(self.switch_indom, insts))

    def fetch_port_info(self, switch):
        """ fetches result from command line """
        query = ['ovs-ofctl', 'dump-ports', switch]
        out = subprocess.Popen(query, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout, stderr = out.communicate()
        stdout = stdout.decode("utf-8")
        return stdout, stderr

    def get_port_info_json(self):
        """ Convert the commandline output to json """
        self.port_info_json = dict()
        self.port_info_names = []

        for val in self.switch_names:
            stdout, _ = self.fetch_port_info(val)
            # string manipulation to get required results
            output = stdout.split('\n')
            # get number of ports
            # print(output[0])
            num_ports = int(output[0].split(':')[1].strip().split(' ')[0])
            # discard line 1
            output = output[1:]
            for i in range(num_ports):
                temp = output[2*i]+','+output[2*i+1].strip()
                # get port name
                port, temp = temp.split(':')[0].strip(), temp.split(':')[1]
                temp = temp.split(',')
                port_vals = []
                # put all port values in an array
                for _,value in enumerate(temp):
                    port_vals.append(value.split('=')[1])

                self.port_info_json[val+'::'+port] = port_vals
                self.port_info_names.append(val+'::'+port)

    def port_info_instances(self):
        """ set up ovs switch's port instances"""
        insts = []
        for idx, val in enumerate(self.port_info_names):
            insts.append(pmdaInstid(idx, val))
        self.add_indom(pmdaIndom(self.port_info_indom, insts))

    def fetch_flow_info(self, switch):
        """ fetches result from command line """
        query = ['ovs-ofctl','dump-flows', switch]
        out = subprocess.Popen(query, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout, stderr = out.communicate()
        stdout = stdout.decode("utf-8")
        return stdout, stderr

    def get_flow_json(self):
        """ Convert the commandline output to json """
        self.flow_json = dict()
        self.flow_names = []

        for val in self.switch_names:
            stdout, _ = self.fetch_flow_info(val)

            output = stdout.split('\n')
            # Remove the excess line
            output = output[1:]
            # Remove the empty line
            if output[-1] == '':
                output = output[:-1]

            #Get number of flows per switch
            num_flows = len(output)

            # String manipulation to get data
            for i in range(num_flows):
                temp = output[i].strip()
                temp = temp.split(',')
                temp2 = []
                for j in range(5):
                    if j != 1:
                        temp2.append(temp[j].split('=')[1])
                    else:
                        temp2.append(temp[j].split('=')[1][:-1])

                temp2.append(output[i].strip().split(' ')[-1].split('=')[1])
                self.flow_json[val+'::'+str(i)] = temp2
                self.flow_names.append(val+'::'+str(i))

    def flow_instances(self):
        """ set up ovs switch's port instances"""
        insts = []
        for idx, val in enumerate(self.flow_names):
            insts.append(pmdaInstid(idx, val))
        self.add_indom(pmdaIndom(self.flow_indom, insts))

    def openvswitch_refresh(self, cluster):
        """refresh function"""
        if cluster == self.switch_cluster:
            self.get_switch_info_json()
            insts = []
            for idx, val in enumerate(self.switch_names):
                insts.append(pmdaInstid(idx, val))

            self.replace_indom(self.switch_indom, insts)

        if cluster == self.port_info_cluster:
            self.get_port_info_json()
            insts = []
            for idx, val in enumerate(self.port_info_names):
                insts.append(pmdaInstid(idx, val))

            self.replace_indom(self.port_info_indom, insts)

        if cluster == self.flow_cluster:
            self.get_flow_json()
            insts = []
            for idx, val in enumerate(self.flow_names):
                insts.append(pmdaInstid(idx, val))

            self.replace_indom(self.flow_indom, insts)
    
    def openvswitch_label(self, ident, type):
        '''
        Return JSONB format labelset for identifier of given type, as a string
        '''
        if type == c_api.PM_LABEL_DOMAIN:
            ret = '"role":"testing"'
        elif type == c_api.PM_LABEL_INDOM:
            indom = ident
            if indom == self.switch_indom:
                ret = '"indom_name":"switch"'
            elif indom == self.port_info_indom:
                ret = '"indom_name":"port_info"'
            elif indom == self.flow_indom:
                ret = '"indom_name":"flow_info"'
        else: # no labels added for types PM_LABEL_CLUSTER, PM_LABEL_ITEM
            ret = '' # default is an empty labelset string

        return '{%s}' % ret

    def openvswitch_label_callback(self, indom, inst):
        '''
        Return JSONB format labelset for an inst in given indom, as a string
        '''
        if indom == self.switch_indom:
            ret = '"switch":"%s"' % (self.inst_name_lookup(self.switch_indom, inst))
        elif indom == self.port_info_indom:
            ret = '"port":"%s"' % (self.inst_name_lookup(self.switch_indom, inst).split['::'][1])
        else:
            ret = ''

        return '{%s}' % ret

    def openvswitch_fetch_callback(self, cluster, item, inst):
        """ fetch callback method"""
        
        if cluster == self.switch_cluster:
            if self.switch_info_json is None:
                return [PMDA_FETCH_NOVALUES]
            try:
                switch = self.inst_name_lookup(self.switch_indom,inst)
                if item == 0:
                    return [str(self.switch_info_json[switch][0][1]), 1]
                if item == 1:
                    return [str(self.switch_info_json[switch][1][1]), 1]
                if item == 2:
                    return [str(self.switch_info_json[switch][2][1]), 1]
                if item == 3:
                    return [str(self.switch_info_json[switch][3]), 1]
                if item == 4:
                    return [str(self.switch_info_json[switch][4]), 1]
                if item == 5:
                    return [str(self.switch_info_json[switch][5]), 1]
                if item == 6:
                    return [str(self.switch_info_json[switch][6][1]), 1]
                if item == 7:
                    return [str(self.switch_info_json[switch][7][1]), 1]
                if item == 8:
                    return [str(self.switch_info_json[switch][8][1]), 1]
                if item == 9:
                    return [str(self.switch_info_json[switch][9][1]), 1]
                if item == 10:
                    return [str(self.switch_info_json[switch][10][1]), 1]
                if item == 11:
                    return [str(self.switch_info_json[switch][11]), 1]
                if item == 12:
                    return [str(self.switch_info_json[switch][12][1]), 1]
                if item == 14:
                    return [str(self.switch_info_json[switch][14][1]), 1]
                if item == 15:
                    return [str(self.switch_info_json[switch][15][1]), 1]
                if item == 16:
                    return [str(self.switch_info_json[switch][16]), 1]
                if item == 17:
                    return [str(self.switch_info_json[switch][17][1]), 1]
                if item == 18:
                    return [str(self.switch_info_json[switch][18]), 1]
                if item == 19:
                    return [str(self.switch_info_json[switch][19][1]), 1]
                if item == 20:
                    return [str(self.switch_info_json[switch][20][1]), 1]
                if item == 21:
                    return [str(self.switch_info_json[switch][21][1]), 1]
                if item == 22:
                    return [str(self.switch_info_json[switch][22]), 1]

            except Exception:
                return [PM_ERR_APPVERSION, 0]


        if cluster == self.port_info_cluster:
            if self.port_info_json is None:
                return [PMDA_FETCH_NOVALUES]
            try:
                port = self.inst_name_lookup(self.port_info_indom,inst)
                if item == 0:
                    return [int(self.port_info_json[port][0]), 1]
                if item == 1:
                    return [int(self.port_info_json[port][1]), 1]
                if item == 2:
                    return [int(self.port_info_json[port][2]), 1]
                if item == 3:
                    return [int(self.port_info_json[port][3]), 1]
                if item == 4:
                    return [int(self.port_info_json[port][4]), 1]
                if item == 5:
                    return [int(self.port_info_json[port][5]), 1]
                if item == 6:
                    return [int(self.port_info_json[port][6]), 1]
                if item == 7:
                    return [int(self.port_info_json[port][7]), 1]
                if item == 8:
                    return [int(self.port_info_json[port][8]), 1]
                if item == 9:
                    return [int(self.port_info_json[port][9]), 1]
                if item == 10:
                    return [int(self.port_info_json[port][10]), 1]
                if item == 11:
                    return [int(self.port_info_json[port][11]), 1]

            except Exception:
                return [PM_ERR_APPVERSION,0]

        if cluster == self.flow_cluster:
            if self.flow_json is None:
                return [PMDA_FETCH_NOVALUES]
            try:
                flow = self.inst_name_lookup(self.flow_indom,inst)
                if item == 0:
                    return [str(self.flow_json[flow][0]), 1]
                if item == 1:
                    return [float(self.flow_json[flow][1]), 1]
                if item == 2:
                    return [int(self.flow_json[flow][2]), 1]
                if item == 3:
                    return [int(self.flow_json[flow][3]), 1]
                if item == 4:
                    return [int(self.flow_json[flow][4]), 1]
                if item == 5:
                    return [str(self.flow_json[flow][5]), 1]

            except Exception:
                return [PM_ERR_APPVERSION, 0]

if __name__ == '__main__':
    OpenvswitchPMDA('openvswitch', 126).run()
