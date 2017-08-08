#!/usr/bin/env python3


import subprocess
import unittest


SERVICE_PMCD = "pmcd.service"
SERVICE_PMLOGGER = "pmlogger.service"


PORTS_PMCD = [44321]
PORTS_PMLOGGER = [4330]


PROP_ACTIVESTATE = "ActiveState"
PROP_SUBSTATE = "SubState"
STATE_RUNNING = "running"
STATE_ACTIVE = "active"


def format_args(service, property):
    return ["systemctl", "show", "-p" + property, service]


def search_output(output, startsWith):
    for line in output.splitlines():
        if line.startswith(startsWith + "="):
            return line[(len(startsWith)+1):]


def check_port(port):
    import socket
    s = socket.socket()
    address = '127.0.0.1'
    try:
        s.connect((address, port))
    except:
        return False
    finally:
        s.close()
    return True



class TestDaemons(unittest.TestCase):
    def check_property(self, service, property, expected):
        output = subprocess.check_output(
            format_args(service, property)).decode("utf-8")
        self.assertEqual(search_output(output, property), expected)

    def test_pmcd(self):
        self.check_property(SERVICE_PMCD, PROP_ACTIVESTATE, STATE_ACTIVE)
        self.check_property(SERVICE_PMCD, PROP_SUBSTATE, STATE_RUNNING)

        for port in PORTS_PMCD:
            self.assertTrue(check_port(port))

    def test_pmlogger(self):
        self.check_property(SERVICE_PMLOGGER, PROP_ACTIVESTATE, STATE_ACTIVE)
        self.check_property(SERVICE_PMLOGGER, PROP_SUBSTATE, STATE_RUNNING)

        for port in PORTS_PMLOGGER:
            self.assertTrue(check_port(port))


if __name__ == "__main__":
    import sys
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout,
                                                     verbosity=2))
