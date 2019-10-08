import unittest
from cpmapi import PM_SEM_COUNTER
from bpftrace.parser import parse_code, process_bpftrace_output_obj
from bpftrace.models import Script, RuntimeInfo, MetricType


class ParserTests(unittest.TestCase):

    def testParseHist(self):
        script = Script("""
	if ($ns) {
		@usecs = hist((nsecs - $ns) / 1000);
	}
	delete(@qtime[args->next_pid]);
""")
        script = parse_code(script)
        self.assertEqual(script.variables['@usecs'].metrictype, MetricType.Histogram)
        self.assertEqual(script.variables['@usecs'].semantics, PM_SEM_COUNTER)

    def testParseStackProfiler(self):
        script = Script('profile:hz:99 { @[kstack] = count(); }')
        script = parse_code(script)
        self.assertEqual(script.variables['@'].metrictype, MetricType.Stacks)
        self.assertEqual(script.variables['@'].semantics, PM_SEM_COUNTER)

    def testParseLhist(self):
        script = Script("""
profile:hz:99
/pid/
{
	@cpu = lhist(cpu, 0, 1000, 1);
}
""")
        script = parse_code(script)
        self.assertEqual(script.variables['@cpu'].metrictype, MetricType.Histogram)
        self.assertEqual(script.variables['@cpu'].semantics, PM_SEM_COUNTER)

    def testTableRetainLines(self):
        runtime_info = RuntimeInfo()
        runtime_info.bpftrace_version = (0, 9, 2)

        script = Script('// table-retain-lines: 3\n'
                        'printf("test");')
        script = parse_code(script)
        self.assertEqual(script.metadata.table_retain_lines, 3)

        def add_output(s):
            process_bpftrace_output_obj(runtime_info, script, {"type": "printf", "msg": s})

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
