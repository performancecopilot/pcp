#!/usr/bin/env python

# Pre Reqs
# 1. rtslib_fb


from pcp import pmapi
import cpmapi as c_api
import logging

import unittest
from iscsi_target import ISCSITarget

summary_metrics = ["lio.summary.total_luns",
                   "lio.summary.total_clients",
                   "lio.summary.tpgs"]


class PMInfo(object):
    pass

class ISCSITests(unittest.TestCase):

    target = None
    pminfo = PMInfo()

    @classmethod
    def setUpClass(cls):
        print "\nCreating test iSCSI target using iscsi_config.json\n"
        ISCSITests.target = ISCSITarget()

        pminfo = PMInfo()
        pminfo.lun = {}

        ctx = pmapi.pmContext()
        #
        # First look at the some sample summary stats
        pmids = ctx.pmLookupName(summary_metrics)

        descs = ctx.pmLookupDescs(pmids)
        results = ctx.pmFetch(pmids)

        for i in range(results.contents.numpmid):
            atom = ctx.pmExtractValue(results.contents.get_valfmt(0),
                                      results.contents.get_vlist(i, 0),
                                      descs[i].contents.type,
                                      c_api.PM_TYPE_U32)

            field_name = summary_metrics[i].split('.')[-1]
            setattr(pminfo, field_name, atom.ul)

        #
        # Now look at the lun stats
        pmids = ctx.pmLookupName("lio.lun.iops")
        descs = ctx.pmLookupDescs(pmids)
        results = ctx.pmFetch(pmids)
        devices = ctx.pmGetInDom(descs[0])[1]
        for i in range(results.contents.get_numval(0)):

                dev_name = devices[i]
                iops = ctx.pmExtractValue(results.contents.get_valfmt(0),
                                          results.contents.get_vlist(0, i),
                                          descs[0].contents.type,
                                          c_api.PM_TYPE_U32)
                pminfo.lun[dev_name] = iops.ul

        pminfo.lun_list = pminfo.lun.keys()

        ISCSITests.pminfo = pminfo


    @classmethod
    def tearDownClass(cls):
        print "\nDeleting test iSCSI target"
        ISCSITests.target.drop_test_config()

    def test_LUN_count(self):
        self.assertEqual(ISCSITests.target.lun_count,
                         ISCSITests.pminfo.total_luns)

    def test_TPG_count(self):
        self.assertEqual(ISCSITests.target.tpg_count,
                         ISCSITests.pminfo.tpgs)

    def test_client_count(self):
        self.assertEqual(ISCSITests.target.client_count,
                         ISCSITests.pminfo.total_clients)

    def test_check_LUN_instances(self):
        self.assertListEqual(sorted(ISCSITests.target.lun_list),
                             sorted(ISCSITests.pminfo.lun_list))

    def test_check_LUN_IOPS(self):
        # all instances should have a 0 value
        for dev_name in ISCSITests.target.lun_list:
            lun_iops = ISCSITests.pminfo.lun[dev_name]
            self.assertEqual(lun_iops, 0)


if __name__ == '__main__':

    suite = unittest.TestLoader().loadTestsFromTestCase(ISCSITests)
    unittest.TextTestRunner(verbosity=2).run(suite)



