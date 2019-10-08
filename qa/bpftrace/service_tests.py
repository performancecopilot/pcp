import unittest
import time
import re
from multiprocessing import Manager
from bpftrace.parser import parse_code, process_bpftrace_output_obj
from bpftrace.models import PMDAConfig, Script, Logger
from bpftrace.service import BPFtraceService


class BPFtraceServiceTests(unittest.TestCase):
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
            "starting script", "started script", "stopping script", "stopped script", "removed script",
            "deregister: script .* not found"
        ]
        idx = [re.search(log_msg, full_output).span() for log_msg in log_msgs_order]
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
        self.setup()
        script = self.service.register_script(Script('profile:hz:9999 { @test1[kstack,ustack] = count(); }'))
        time.sleep(15)
        script = self.service.refresh_script(script.script_id)
        self.assertEqual(script.state.status, 'error')
        self.assertEqual(script.state.error, 'BPFtrace output exceeds limit of 102400 bytes per second')

    def testTooMuchOutput(self):
        self.setup()
        script = self.service.register_script(Script('profile:hz:9999 { printf("test"); }'))
        time.sleep(10)
        script = self.service.refresh_script(script.script_id)
        self.assertEqual(script.state.status, 'error')
        self.assertEqual(script.state.error, 'BPFtrace output exceeds limit of 102400 bytes per second')

    def testTableRetainLines(self):
        self.setup()
        self.config.bpftrace_version = (0, 9, 2)
        script = Script('// table-retain-lines: 3\n'
                        'printf("test");')
        script = parse_code(script)
        self.assertEqual(script.metadata.table_retain_lines, 3)

        def add_output(s):
            process_bpftrace_output_obj(self.config, script, {"type": "printf", "msg": s})

        def assert_output(o):
            self.assertEqual(script.state.data["@output"], o)

        add_output('head')
        assert_output('head')
        add_output('er\n')
        assert_output('header\n')
        add_output('line ')
        assert_output('header\nline ')
        add_output('1\n')
        assert_output('header\nline 1\n')
        add_output('line ')
        assert_output('header\nline 1\nline ')
        add_output('2\n')
        assert_output('header\nline 1\nline 2\n')
        add_output('line ')
        assert_output('header\nline 1\nline 2\nline ')
        add_output('3\n')
        assert_output('header\nline 1\nline 2\nline 3\n')
        add_output('line ')
        assert_output('header\nline 2\nline 3\nline ')
        add_output('4\n')
        assert_output('header\nline 2\nline 3\nline 4\n')
        add_output('line ')
        assert_output('header\nline 3\nline 4\nline ')
        add_output('5\n')
        assert_output('header\nline 3\nline 4\nline 5\n')
        add_output('line 10\nline 11\nline 12\n')
        assert_output('header\nline 10\nline 11\nline 12\n')


if __name__ == '__main__':
    unittest.main()
