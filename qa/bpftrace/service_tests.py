import unittest
import time
import re
from multiprocessing import Manager
from bpftrace.models import PMDAConfig, Script, Logger
from bpftrace.service import BPFtraceService


class ServiceTests(unittest.TestCase):
    config: PMDAConfig

    def setup(self, config=None, logger=None):
        self.config = config or PMDAConfig()
        self.logger = logger or Logger(lambda x: print("Info: " + x), lambda x: print("Error: " + x))
        self.service = BPFtraceService(self.config, self.logger)
        self.service.start_daemon()

    def tearDown(self):
        self.service.stop_daemon()

    def waitForData(self, script_id):
        for i in range(10):
            script = self.service.refresh_script(script_id)
            if script.state.data:
                return script
            time.sleep(0.5)
        raise Exception('Timeout waiting for bpftrace data')

    def testStart(self):
        self.setup()
        script = self.service.register_script(Script('kretprobe:vfs_read { @bytes = hist(retval); }'))
        self.assertEqual(script.state.status, 'starting')

        script = self.waitForData(script.script_id)
        self.assertTrue(script.state.data)
        self.service.stop_script(script.script_id)

    def testDeregister(self):
        manager = Manager()
        output = manager.list()  # will be transferred to a different process

        logger = Logger(output.append, output.append)
        self.setup(logger=logger)
        script = self.service.register_script(Script('kretprobe:vfs_read { @bytes = hist(retval); }'))
        self.assertEqual(script.state.status, 'starting')

        script = self.waitForData(script.script_id)
        self.assertTrue(script.state.data)
        self.service.deregister_script(script.script_id)
        self.service.stop_daemon()

        # verify if events happen in the correct order by checking the log messages
        full_output = "\n".join(output)
        print(f"testDeregister output: {full_output}")
        log_msgs_order = [
            "starting script", "started script", "stopping script", "stopped script", "deregistered script",
            "deregister: script .* not found"
        ]
        idx = []
        for log_msg in log_msgs_order:
            m = re.search(log_msg, full_output)
            if not m:
                raise Exception(f"cannot find '{log_msg}' in:\n{full_output}")
            idx.append(m.span()[0])
        self.assertEqual(len(idx), len(log_msgs_order))
        self.assertEqual(idx, sorted(idx))

    def testExpiry(self):
        config = PMDAConfig()
        config.script_expiry_time = 2
        self.setup(config)

        script = self.service.register_script(Script('kretprobe:vfs_read { @bytes = hist(retval); }'))
        time.sleep(4)
        script = self.service.refresh_script(script.script_id)
        self.assertIsNone(script)

    def testTooManyKeys(self):
        config = PMDAConfig()
        config.max_throughput = 64 * 1024
        self.setup(config)

        script = self.service.register_script(Script('profile:hz:9999 { @test1[kstack,ustack] = count(); }'))
        time.sleep(15)
        script = self.service.refresh_script(script.script_id)
        self.assertEqual(script.state.status, 'error')
        self.assertRegex(script.state.error, 'BPFtrace output exceeds limit of .+ bytes per second')

    def testTooMuchOutput(self):
        config = PMDAConfig()
        config.max_throughput = 64 * 1024
        self.setup(config)

        script = self.service.register_script(Script('profile:hz:9999 { printf("test"); }'))
        time.sleep(15)
        script = self.service.refresh_script(script.script_id)
        self.assertEqual(script.state.status, 'error')
        self.assertRegex(script.state.error, 'BPFtrace output exceeds limit of .+ bytes per second')


if __name__ == '__main__':
    unittest.main()
