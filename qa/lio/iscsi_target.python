#!/usr/bin/env python


import sys
import rtslib_fb
import json


class ISCSITarget(object):

    """
    Class used to define an iSCSI target using an json file to govern the
    target name, clients and luns defined.
    """

    def __init__(self):

        self.lio = rtslib_fb.root.RTSRoot()
        self.lun_list = []

        try:
            with open('lio/iscsi_conf.json') as conf:
                config = json.load(conf)
        except ValueError:
            print "invalid JSON"
            sys.exit(4)
        except IOError:
            print "Missing config file"
            sys.exit(4)

        self._create_target(config['target'])
        for lun in config["luns"]:
            self._add_lun(name=lun['name'])

        for client in config["clients"]:
            self._add_client(client_iqn=client["client_iqn"])

    def _create_target(self, iqn):
        iscsi = rtslib_fb.fabric.ISCSIFabricModule()
        self.iqn = iqn
        self.target = rtslib_fb.fabric.Target(iscsi, wwn=self.iqn)
        self.tpg = rtslib_fb.target.TPG(self.target)
        self.portal = rtslib_fb.target.NetworkPortal(self.tpg, '0.0.0.0')
        self.tpg.enable = True

    def _add_lun(self, name='ramdisk', disk_type='ramdisk', size=104857600):

        if disk_type == 'ramdisk':

            try:
                # create the disk (storage object)
                so = rtslib_fb.tcm.RDMCPStorageObject(name, size=size)

                # map to the iscsi tpg
                next_lun = len([disk for disk in self.lio.storage_objects]) - 1
                rtslib_fb.target.LUN(self.tpg, lun=next_lun,
                                     storage_object=so)
                self.lun_list.append(name)

            except rtslib_fb.RTSLibError:
                print "Failed to create LUN {}".format(name)
                sys.exit(4)

        else:
            raise ValueError("LUN type of {} has not been implemented")

    def _add_client(self, client_iqn=None):

        try:
            rtslib_fb.target.NodeACL(self.tpg, client_iqn)
        except rtslib_fb.RTSLibError:
            print "failed to create client {}".format(client_iqn)
            sys.exit(4)

    def _client_count(self):
        return len([client for client in self.lio.node_acls])

    def _lun_count(self):
        return len([lun for lun in self.lio.luns])

    def _tpg_count(self):
        return len([tpg for tpg in self.target.tpgs])

    def drop_test_config(self):

        # delete the test iscsi target definition
        # removing target/tpg/nodeacls/mapped luns
        self.target.delete()

        # delete disks created for the test
        for so in self.lio.storage_objects:
            so.delete()

    client_count = property(_client_count,
                            doc="get client count")

    lun_count = property(_lun_count,
                         doc='get number of luns')

    tpg_count = property(_tpg_count,
                         doc='get number of tpgs')
