import unittest
import pmapi
import time
from pcp import *
from ctypes import *

traverse_callback_count = 0

# callback for pmTraversePMNS
def traverse_callback (arg):
    global traverse_callback_count
    traverse_callback_count += 1

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

def check(self):
    True

class TestSequenceFunctions(unittest.TestCase):

    pmc = None
    ncpu_id = []
    metric_ids = []

    def setUp(self):
        print ("setUp")
        self.pm = pmContext(pmapi.PM_CONTEXT_HOST,"localhost")
        self.assertTrue(len(self.pm.pmGetContextHostName()) >= 0)
        (code, rsltp, errmsg) = self.pm.pmParseMetricSpec("kernel.all.load", 0, "1 minute")
        # XXX check inst
        self.assertTrue(rsltp.contents.source == "1 minute")


    def test_pcp(self):
        check(self)
        # Get number cpus
        (code, self.ncpu_id) = self.pm.pmLookupName(("hinv.ncpu","kernel.all.load"))
        self.assertTrue(code >= 0)
        self.assertTrue(self.pm.pmIDStr(self.ncpu_id[0]).count(".") >= 1)
        (code, descs) = self.pm.pmLookupDesc(self.ncpu_id)
        self.assertTrue(code >= 0)
        dump_array_ptrs("pmLookupDesc", descs)
        (code, results) = self.pm.pmFetch(self.ncpu_id)
        self.assertTrue(code >= 0)
        (code, atom) = self.pm.pmExtractValue(results, descs, self.ncpu_id[0], 0, pmapi.PM_TYPE_U32)
        self.assertTrue(code >= 0)
        self.assertTrue(atom.ul > 0)
        print ("#cpus=",atom.ul)
        ncpu = atom.ul

        # pmGetChildren
        gc = self.pm.pmGetChildren("kernel")
        self.assertTrue(len(gc) >= 4)
        gc = self.pm.pmGetChildrenStatus("kernel")
        self.assertTrue(len(gc[0]) == len(gc[1]))

        # pmGetPMNSLocation
        i = self.pm.pmGetPMNSLocation()
        self.assertTrue(any((i==PMNS_ARCHIVE,i==PMNS_LOCAL,i==PMNS_REMOTE)))

        # pmTraversePMNS
        global traverse_callback_count
        traverse_callback_count = 0
        i = self.pm.pmTraversePMNS("kernel", traverse_callback)
        self.assertTrue(traverse_callback_count > 0)

        # Try a bad name lookup

        try:
            (code, badid) = self.pm.pmLookupName("A_BAD_METRIC")
            self.assertTrue(False)
        except  pmErr as e:
            print self.pm.pmErrStr(e.value)
            self.assertTrue(True)

        # Get metrics
        metrics = ("kernel.all.load", "kernel.percpu.cpu.user", "kernel.percpu.cpu.sys", "mem.freemem", "disk.all.total")
        (code, self.metric_ids) = self.pm.pmLookupName(metrics)
        dump_array("pmLookupName", self.metric_ids)
        self.assertTrue(code >= 0)

        for i in xrange(len(metrics)-1):
            self.assertTrue(self.pm.pmNameAll(self.metric_ids[i])[0] == metrics[i])
            self.assertTrue(self.pm.pmNameID(self.metric_ids[i]) == metrics[i])
        (code, descs) = self.pm.pmLookupDesc(self.metric_ids)
        dump_array_ptrs("pmLookupDesc", descs)
        self.assertTrue(code >= 0)

        # pmGetInDom
        (inst, name) = self.pm.pmGetInDom(descs[0])
        self.assertTrue(all((len(inst) == 3, len(name) == 3)))

        # pmInDomStr
        self.assertTrue(self.pm.pmInDomStr(descs[0]).count(".") >= 1)

        # Set kernel.all.load
        code = self.pm.pmDelProfile(descs[0], None);
        self.assertTrue(code >= 0)

        # pmLookupInDom
        inst1 = self.pm.pmLookupInDom(descs[0], "1 minute")
        self.assertTrue(inst1 >= 0)
        
        # pmNameInDom
        instname = self.pm.pmNameInDom(descs[0], inst1)
        self.assertTrue(instname == "1 minute")
        
        text = 0
        try:
            # XXX test with real help info, pmLookupText
            text = self.pm.pmLookupInDomText(descs[0])
            self.assertTrue(False)
        except pmErr as e:
            print self.pm.pmErrStr(e.value)
            self.assertTrue(True)
        
        code = self.pm.pmAddProfile(descs[0], inst1);
        self.assertTrue(code >= 0)

        inst = 0
        try:
            inst = self.pm.pmLookupInDom(descs[0], "gg minute")
            self.assertTrue(False)
        except  pmErr as e:
            print self.pm.pmErrStr(e.value)
            self.assertTrue(True)
        
        inst15 = self.pm.pmLookupInDom(descs[0], "15 minute")
        self.assertTrue(inst15 >= 0)
        
        code = self.pm.pmAddProfile(descs[0], inst15);
        self.assertTrue(code >= 0)

        previous_cpu_user = [0,0,0,0]
        previous_cpu_sys = [0,0,0,0]
        previous_disk = 0

        (code, delta, errmsg) = self.pm.pmParseInterval("5 seconds")
        self.assertTrue(code >= 0)

        try:
            # XXX Test a real use, also pmLoadASCIINameSpace, pmUnLoadNameSpace
            inst = self.pm.pmLoadNameSpace("NoSuchFile")
            self.assertTrue(False)
        except  pmErr as e:
            print self.pm.pmErrStr(e.value)
            self.assertTrue(True)

        n = 0
        while n < 2:
            cpu = 0
            while cpu < ncpu:
                # pmFetch
                (code, results) = self.pm.pmFetch(self.metric_ids)
                self.assertTrue(code >= 0)

                # pmSortInstances
                self.pm.pmSortInstances(results)

                # pmStore
                try:
                    code = self.pm.pmStore(results)
                    self.assertTrue(False)
                except pmErr as e:
                    print self.pm.pmErrStr(e.value)
                    self.assertTrue(True)
                    
                self.assertTrue(code >= 0)

                # pmExtractValue kernel.percpu.cpu.user
                (code, atom) = self.pm.pmExtractValue(results, descs, self.metric_ids[1], 0, pmapi.PM_TYPE_FLOAT)
                self.assertTrue(code >= 0)
                cpu_val = atom.f - previous_cpu_user[cpu]
                previous_cpu_user[cpu] = atom.f

                # kernel.percpu.cpu.sys
                (code, atom) = self.pm.pmExtractValue(results, descs, self.metric_ids[2], 0, pmapi.PM_TYPE_FLOAT)
                self.assertTrue(code >= 0)
                cpu_val = cpu_val + atom.f - previous_cpu_sys[cpu]
                print "cpu_val=", cpu_val
                self.assertTrue(cpu_val > 0)
                previous_cpu_sys[cpu] = atom.f
                cpu = cpu + 1

                # mem.freemem
                (code, tmpatom) = self.pm.pmExtractValue(results, descs, self.metric_ids[3], 0, pmapi.PM_TYPE_FLOAT)
                self.assertTrue(code >= 0)
                (code, atom) = self.pm.pmConvScale(pmapi.PM_TYPE_FLOAT, tmpatom, descs, 3, pmapi.PM_SPACE_MBYTE)
                self.assertTrue(code >= 0)
                print "freemem (Mbytes)=",atom.f

                # kernel.all.load
                for i in xrange(results.contents.get_vset_length(0) - 1):
                    (code, atom) = self.pm.pmExtractValue(results, descs, self.metric_ids[0], 0, pmapi.PM_TYPE_FLOAT)
                    value = atom.f
                    print 206,value
                    self.assertTrue(code >= 0)
                    if results.contents.get_inst(0, i) == inst1:
                        # XXX self.pm.pmprintf ("load average 1=%f", value)
                        print "load average 1=%f",atom.f
                    elif results.contents.get_inst(0, i) == inst15:
                        # XXX why no 15?
                        print "load average 15=%f",atom.f

                # disk.all.total
                (code, atom) = self.pm.pmExtractValue(results, descs, self.metric_ids[4], 0, pmapi.PM_TYPE_U32)
                dkiops = atom.ul;
                dkiops = dkiops - previous_disk
                previous_disk = atom.ul
                self.pm.pmprintf ("Disk IO=%d\n",dkiops)
                self.pm.pmflush()
                # pmAtomStr
                self.assertTrue(self.pm.pmAtomStr (atom, pmapi.PM_TYPE_U32) == str(previous_disk))

            self.pm.pmFreeResult(results)
            code = self.pm.pmtimevalSleep(delta);
            n = n + 1
            context = self.pm.pmDupContext()
            self.assertTrue(context >= 0)
            code = self.pm.pmWhichContext ()
            self.assertTrue(code >= 0)

        # pmTypeStr
        self.assertTrue (self.pm.pmTypeStr (pmapi.PM_TYPE_FLOAT)  == "FLOAT")

        # pmPrintValue XXX
        # self.pm.pmPrintValue(sys.__stdout__, results, descs[0], 0, 0, 8)

        # pmReconnectContext
        code = self.pm.pmReconnectContext ()
        self.assertTrue(code >= 0)

if __name__ == '__main__':
    unittest.main()

""" Not tested
pmNewContextZone
pmNewZone
pmUseZone
pmWhichZone
pmGetConfig
----------------
pmGetArchiveLabel
pmGetArchiveEnd
pmGetInDomArchive
pmLookupInDomArchive
pmNameInDomArchive
pmFetchArchive
pmSetMode

pmParseMetricSpec
"""
