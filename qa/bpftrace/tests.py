# pylint: disable=missing-docstring
import unittest
from bpftrace import BPFtrace


class BPFtraceTests(unittest.TestCase):

    def testTableRetainLines(self):
        bpftrace = BPFtrace(None, None, '// table-retain-lines: 3\n'
                                        'printf("test");')
        self.assertEqual(bpftrace.metadata.table_retain_lines, 3)

        def add_output(s):
            bpftrace.process_output_obj({"type": "printf", "msg": s})
        def assert_output(o):
            self.assertEqual(bpftrace.state().maps["@output"], o)

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
