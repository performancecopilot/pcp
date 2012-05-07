import unittest
import pmapi
import time
from pcp import *
from ctypes import *

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

    def test_pcp(self):
        check(self)
        # Get number cpus
        (code, self.ncpu_id) = self.pm.pmLookupName(("hinv.ncpu"))
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

        # Try a bad name lookup
        pmerr = pmErr()
        try:
            (code, badid) = self.pm.pmLookupName("A_BAD_METRIC")
            self.assertTrue(False)
        except  pmErr:
            print ("code=", self.pm.pmErrStr(code))
            self.assertTrue(True)

        # Get metrics
        (code, self.metric_ids) = self.pm.pmLookupName(("kernel.all.load", "kernel.percpu.cpu.user", "kernel.percpu.cpu.sys", "mem.freemem", "disk.all.total"))
        dump_array("pmLookupName", self.metric_ids)
        self.assertTrue(code >= 0)

        (code, descs) = self.pm.pmLookupDesc(self.metric_ids)
        dump_array_ptrs("pmLookupDesc", descs)
        self.assertTrue(code >= 0)

        # Set kernel.all.load
        code = self.pm.pmDelProfile(descs[0].contents, None);
        self.assertTrue(code >= 0)

        inst1 = self.pm.pmLookupInDom(descs[0].contents, "1 minute")
        self.assertTrue(code >= 0)
        
        code = self.pm.pmAddProfile(descs[0].contents, inst1);
        self.assertTrue(code >= 0)

        inst = 0
        try:
            inst = self.pm.pmLookupInDom(descs[0].contents, "gg minute")
            self.assertTrue(False)
        except  pmErr:
            print ("code=", self.pm.pmErrStr(inst))
            self.assertTrue(True)
        
        inst15 = self.pm.pmLookupInDom(descs[0].contents, "15 minute")
        self.assertTrue(code >= 0)
        
        code = self.pm.pmAddProfile(descs[0].contents, inst15);
        self.assertTrue(code >= 0)

        previous_cpu_user = [0,0,0,0]
        previous_cpu_sys = [0,0,0,0]
        previous_disk = 0

        (code, delta, errmsg) = self.pm.pmParseInterval("5 seconds")
        self.assertTrue(code >= 0)

        try:
            inst = self.pm.pmLoadNameSpace("NoSuchFile")
            self.assertTrue(False)
        except  pmErr:
            print ("code=", self.pm.pmErrStr(inst))
            self.assertTrue(True)

        n = 0
        while n < 10:
            cpu = 0
            while cpu < ncpu:
                # Fetch metrics
                (code, results) = self.pm.pmFetch(self.metric_ids)
                # kernel.percpu.cpu.user
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
                self.pm.pmFreeResult(results)

                # mem.freemem
                (code, tmpatom) = self.pm.pmExtractValue(results, descs, self.metric_ids[3], 0, pmapi.PM_TYPE_FLOAT)
                self.assertTrue(code >= 0)
                (code, atom) = self.pm.pmConvScale(pmapi.PM_TYPE_FLOAT, tmpatom, descs, 3, pmapi.PM_SPACE_MBYTE)
                self.assertTrue(code >= 0)
                print "freemem (Mbytes)=",atom.f

                # kernel.all.load
                for i in xrange((results.contents.vset[0].contents.numval)-1):
                    (code, atom) = self.pm.pmExtractValue(results, descs, self.metric_ids[0], 0, pmapi.PM_TYPE_FLOAT)
                    if results.contents.vset[0].contents.vlist[i].inst == inst1:
                        print "load average 1=",atom.f
                    elif results.contents.vset[0].contents.vlist[i].inst == inst15:
                        # XXX why no 15?
                        print "load average 15=",atom.f

                # disk.all.total
                (code, atom) = self.pm.pmExtractValue(results, descs, self.metric_ids[4], 0, pmapi.PM_TYPE_U32)
                dkiops = atom.ul;
                dkiops = dkiops - previous_disk
                previous_disk = atom.ul
                print "Disk IO=",dkiops
                
            code = self.pm.pmtimevalSleep(delta);
            n = n + 1

if __name__ == '__main__':
    unittest.main()
