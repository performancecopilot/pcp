#!/usr/bin/env python3


import unittest
import subprocess


class TestCliTools(unittest.TestCase):
    def test_pmerr(self):
        subprocess.check_call(['pmerr', '-2']);

    def test_pminfo(self):
        subprocess.check_call(['pminfo', 'proc.nprocs'])

    def test_pcp(self):
        subprocess.check_call(['pcp'])

    def test_pmstat(self):
        subprocess.check_call(['pmstat', '-s', '1'])

    def test_pmdate(self):
        subprocess.check_call(['pmdate', '-7d', '%d%m%Y'])

    def test_pmprobe(self):
        subprocess.check_call(['pmprobe'])

    def test_pmclient(self):
        subprocess.check_call(['pmclient', '-s', '1'])

    def test_pmval(self):
        subprocess.check_call(['pmval', 'proc.nprocs', '-s', '1'])

    def test_pmcollectl(self):
        subprocess.check_call(['pmcollectl', '-c', '1'])

    def test_pmiostat(self):
        subprocess.check_call(['pmiostat', '-s', '2'])

if __name__ == "__main__":
    import sys
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout,
                                                     verbosity=2))
