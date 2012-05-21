import unittest
import pmapi
import time
import sys
import argparse
from pcp import *
from ctypes import *

# Utilities

def dump_seq (name_p, seq_p):
    print (name_p)
    for t in seq_p:
        if type(t) == type(int()) or type(t) == type(long()):
            print (hex(t))
        else:
            print (t)
    print ()

def dump_array_ptrs (name_p, arr_p):
    print (name_p)
    for i in xrange(len(arr_p)):
        print (" ") if (i > 0) else "", arr_p[i].contents

def dump_array (name_p, arr_p):
    print (name_p)
    for i in xrange(len(arr_p)):
        print (" ") if (i > 0) else "", hex(arr_p[i])

archive = ""                    # For testing either localhost or archive

traverse_callback_count = 0     # callback for pmTraversePMNS

def traverse_callback (arg):
    global traverse_callback_count
    traverse_callback_count += 1

def test_pcp(self, context = 'local', path = ''):

    if (archive == ""):
        print 'Running as localhost'
        pm = pmContext(pmapi.PM_CONTEXT_HOST,"localhost")
        self.local_type = True
    else:
        print 'Running as archive ' + archive
        pm = pmContext(pmapi.PM_CONTEXT_ARCHIVE, archive)
        self.archive_type = True
    # pmGetContextHostName
    self.assertTrue(len(pm.pmGetContextHostName()) >= 0)
    # pmParseMetricSpec
    (code, rsltp, errmsg) = pm.pmParseMetricSpec("kernel.all.load", 0, "1 minute")
    # XXX check inst
    self.assertTrue(rsltp.contents.source == "1 minute")

    # pmLookupName Get number cpus
    print "XXX"
    (code, self.ncpu_id) = pm.pmLookupName(("hinv.ncpu","kernel.all.load"))
    print "YYY"
    self.assertTrue(code >= 0)
    # pmIDStr
    self.assertTrue(pm.pmIDStr(self.ncpu_id[0]).count(".") >= 1)
    # pmLookupDesc
    (code, descs) = pm.pmLookupDesc(self.ncpu_id)
    self.assertTrue(code >= 0)
    dump_array_ptrs("pmLookupDesc", descs)
    # pmFetch
    (code, results) = pm.pmFetch(self.ncpu_id)
    self.assertTrue(code >= 0)

    # pmExtractValue
    (code, atom) = pm.pmExtractValue(results.contents.get_valfmt(0),
                                     results.contents.get_vlist(0, 0),
                                     descs[0].contents.type,
                                     pmapi.PM_TYPE_U32)
    self.assertTrue(code >= 0)
    self.assertTrue(atom.ul > 0)
    print ("#cpus=",atom.ul)
    ncpu = atom.ul

    # pmGetChildren
    gc = pm.pmGetChildren("kernel")
    self.assertTrue(len(gc) >=2)
    # pmGetChildrenStatus
    gc = pm.pmGetChildrenStatus("kernel")
    self.assertTrue(len(gc[0]) == len(gc[1]))

    # pmGetPMNSLocation
    i = pm.pmGetPMNSLocation()
    self.assertTrue(any((i==PMNS_ARCHIVE,i==PMNS_LOCAL,i==PMNS_REMOTE)))

    # pmTraversePMNS
    global traverse_callback_count
    traverse_callback_count = 0
    i = pm.pmTraversePMNS("kernel", traverse_callback)
    self.assertTrue(traverse_callback_count > 0)

# Try a bad name lookup

    try:
        # pmLookupName
        (code, badid) = pm.pmLookupName("A_BAD_METRIC")
        self.assertTrue(False)
    except  pmErr as e:
        print "pmLookupName bad metric: ", e
        self.assertTrue(True)

    # Get metrics
    metrics = ("kernel.all.load", "kernel.percpu.cpu.user", "kernel.percpu.cpu.sys", "mem.freemem", "disk.all.total")
    # pmLookupName
    (code, self.metric_ids) = pm.pmLookupName(metrics)
    dump_array("pmLookupName", self.metric_ids)
    self.assertTrue(code >= 0)

    for i in xrange(len(metrics)-1):
        # pmNameAll
        self.assertTrue(pm.pmNameAll(self.metric_ids[i])[0] == metrics[i])
        # pmNameID
        self.assertTrue(pm.pmNameID(self.metric_ids[i]) == metrics[i])
        # pmLookupDesc
        (code, descs) = pm.pmLookupDesc(self.metric_ids)
        dump_array_ptrs("pmLookupDesc", descs)
        self.assertTrue(code >= 0)

    if self.local_type:
        # pmGetInDom
        (inst, name) = pm.pmGetInDom(descs[1])
        self.assertTrue(all((len(inst) >= 2, len(name) >= 2)))
    else:
        # pmGetInDomArchive
        (inst, name) = pm.pmGetInDomArchive(descs[1])
        self.assertTrue(all((len(inst) >= 2, len(name) >= 2)))

    # pmInDomStr
    self.assertTrue(pm.pmInDomStr(descs[0]).count(".") >= 1)

    # pmDelProfile
    code = pm.pmDelProfile(descs[0], None);
    self.assertTrue(code >= 0)

    if self.local_type:
        # pmLookupInDom
        inst1 = pm.pmLookupInDom(descs[0], "1 minute")
    else:
        # pmLookupInDomArchive
        inst1 = pm.pmLookupInDomArchive(descs[0], "1 minute")
    self.assertTrue(inst1 >= 0)
        
    if self.local_type:
        # pmNameInDom
        instname = pm.pmNameInDom(descs[0], inst1)
    else:
        # pmNameInDomArchive
        instname = pm.pmNameInDomArchive(descs[0], inst1)
    self.assertTrue(instname == "1 minute")
        
    text = 0
    try:
        # pmLookupInDomText XXX there is no help info
        text = pm.pmLookupInDomText(descs[0])
        self.assertTrue(False)
    except pmErr as e:
        print "pmLookupInDomText no help info: ", e
        self.assertTrue(True)
        
    # pmAddProfile
    code = pm.pmAddProfile(descs[0], inst1);
    self.assertTrue(code >= 0)

    inst = 0
    try:
        # pmLookupInDom
        inst = pm.pmLookupInDom(descs[0], "gg minute")
        self.assertTrue(False)
    except  pmErr as e:
        print "pmLookupInDom invalid minute: ", e
        self.assertTrue(True)
        
    if self.local_type:
        # pmLookupInDom
        inst15 = pm.pmLookupInDom(descs[0], "15 minute")
    else:
        # pmLookupInDomArchive
        inst15 = pm.pmLookupInDomArchive(descs[0], "15 minute")
    self.assertTrue(inst15 >= 0)
        
    # pmAddProfile
    code = pm.pmAddProfile(descs[0], inst15);
    self.assertTrue(code >= 0)

    previous_cpu_user = [0,0,0,0]
    previous_cpu_sys = [0,0,0,0]
    previous_disk = 0

    # pmParseInterval
    (code, delta, errmsg) = pm.pmParseInterval("5 seconds")
    self.assertTrue(code >= 0)

    try:
        # pmLoadNameSpace XXX Test: a real use, pmLoadASCIINameSpace, pmUnLoadNameSpace
        inst = pm.pmLoadNameSpace("NoSuchFile")
        self.assertTrue(False)
    except  pmErr as e:
        print "pmLoadNameSpace no such file: ", e
        self.assertTrue(True)

    n = 0
    while n < 2:
        cpu = 0
        while cpu < ncpu:
            # pmFetch
            (code, results) = pm.pmFetch(self.metric_ids)
            self.assertTrue(code >= 0)

            # pmSortInstances
            pm.pmSortInstances(results)

            # pmStore
            try:
                # pmStore
                code = pm.pmStore(results)
                self.assertTrue(False)
            except pmErr as e:
                print "pmStore: ", e
                self.assertTrue(True)
                    
            self.assertTrue(code >= 0)

            # pmExtractValue kernel.percpu.cpu.user
            for i in xrange(results.contents.numpmid):
                if (results.contents.get_pmid(i) != self.metric_ids[1]):
                    continue
                (code, atom) = pm.pmExtractValue(results.contents.get_valfmt(i),
                                                 results.contents.get_vlist(i, 0),
                                                 descs[i].contents.type, pmapi.PM_TYPE_FLOAT)
            self.assertTrue(code >= 0)
            cpu_val = atom.f - previous_cpu_user[cpu]
            previous_cpu_user[cpu] = atom.f

            # pmExtractValue kernel.percpu.cpu.sys
            for i in xrange(results.contents.numpmid):
                if (results.contents.get_pmid(i) != self.metric_ids[2]):
                    continue
                (code, atom) = pm.pmExtractValue(results.contents.get_valfmt(i),
                                                 results.contents.get_vlist(i, 0),
                                                 descs[i].contents.type, pmapi.PM_TYPE_FLOAT)
            self.assertTrue(code >= 0)
            cpu_val = cpu_val + atom.f - previous_cpu_sys[cpu]
            print "cpu_val=", cpu_val
            self.assertTrue(cpu_val > 0)
            previous_cpu_sys[cpu] = atom.f
            cpu = cpu + 1

            # pmExtractValue mem.freemem
            for i in xrange(results.contents.numpmid):
                if (results.contents.get_pmid(i) != self.metric_ids[3]):
                    continue
                (code, tmpatom) = pm.pmExtractValue(results.contents.get_valfmt(i),
                                                    results.contents.get_vlist(i, 0),
                                                    descs[i].contents.type, pmapi.PM_TYPE_FLOAT)
            self.assertTrue(code >= 0)
            # pmConvScale
            (code, atom) = pm.pmConvScale(pmapi.PM_TYPE_FLOAT, tmpatom, descs, 3, pmapi.PM_SPACE_MBYTE)
            self.assertTrue(code >= 0)
            print "freemem (Mbytes)=",atom.f

            for i in xrange(results.contents.get_numval(0) - 1):
                # pmExtractValue kernel.all.load
                for i in xrange(results.contents.numpmid):
                    if (results.contents.get_pmid(i) != self.metric_ids[0]):
                        continue
                    (code, atom) = pm.pmExtractValue(results.contents.get_valfmt(i),
                                                     results.contents.get_vlist(i, 0),
                                                     descs[i].contents.type, pmapi.PM_TYPE_FLOAT)
                value = atom.f
                self.assertTrue(code >= 0)
                if results.contents.get_inst(0, i) == inst1:
                    # XXX pm.pmprintf ("load average 1=%f", value)
                    print "load average 1=",atom.f
                elif results.contents.get_inst(0, i) == inst15:
                    # XXX why no 15?
                    print "load average 15=%f",atom.f

            # pmExtractValue disk.all.total
            for i in xrange(results.contents.numpmid):
                if (results.contents.get_pmid(i) != self.metric_ids[4]):
                    continue
                (code, atom) = pm.pmExtractValue(results.contents.get_valfmt(i),
                                                 results.contents.get_vlist(i, 0),
                                                 descs[i].contents.type, pmapi.PM_TYPE_U32)
            dkiops = atom.ul;
            dkiops = dkiops - previous_disk
            previous_disk = atom.ul
            pm.pmprintf ("Disk IO=%d\n",dkiops)
            pm.pmflush()
            # pmAtomStr
            self.assertTrue(pm.pmAtomStr (atom, pmapi.PM_TYPE_U32) == str(previous_disk))

        # pmFreeResult
        pm.pmFreeResult(results)
        code = pm.pmtimevalSleep(delta);
        n = n + 1
        # pmDupContext
        context = pm.pmDupContext()
        self.assertTrue(context >= 0)
        # pmWhichContext
        code = pm.pmWhichContext ()
        self.assertTrue(code >= 0)

    # pmTypeStr
    self.assertTrue (pm.pmTypeStr (pmapi.PM_TYPE_FLOAT)  == "FLOAT")

    if self.archive_type:
        # pmSetMode
        code = pm.pmSetMode (pmapi.PM_MODE_INTERP, results.contents.timestamp, 0)
        self.assertTrue(code >= 0)

    # pmPrintValue XXX
    # pm.pmPrintValue(sys.__stdout__, results, descs[0], 0, 0, 8)

    if self.archive_type:
        # pmGetArchiveLabel
        (code, loglabel) = pm.pmGetArchiveLabel()
        self.assertTrue (all((code >= 0, loglabel.pid_t > 0)))

    # pmReconnectContext
    code = pm.pmReconnectContext ()
    self.assertTrue(code >= 0)
    del pm

class TestSequenceFunctions(unittest.TestCase):

    ncpu_id = []
    metric_ids = []
    archive_type = False
    local_type = False

    def test_context(self):
        test_pcp(self)


if __name__ == '__main__':

    HAVE_BITFIELDS_LTOR = False
    if (len(sys.argv) == 2):
        open(sys.argv[1] + '.index', mode='r')
        archive = sys.argv[1]
    elif (len(sys.argv) > 2):
        print "Usage: " + sys.argv[0] + " OptionalArchivePath"
        sys.exit()
    else:
        archive = ""
        
    sys.argv[1:] = ()

    unittest.main()
    sys.exit(main())

""" Not tested
pmNewContextZone
pmNewZone
pmUseZone
pmWhichZone
pmGetConfig
pmFetchArchive
pmGetArchiveEnd
pmSetMode
"""
