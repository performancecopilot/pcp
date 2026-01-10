# Plan: Fast TDD Unit Testing for pmrep

**⚠️ SUPERSEDED**: This plan has been consolidated into `PLAN-pmrep-column-grouping.md`.

See the "Fast Unit Testing" section in PLAN-pmrep-column-grouping.md for:
- Current status (✅ COMPLETE - 146 tests in 0.002s)
- Testing workflow and infrastructure
- TDD success story and lessons learned

**Status as of 2026-01-11**: All phases complete, TDD methodology proven effective.

---

## Goal

Refactor pmrep.py to be unit-testable, then use true TDD for the column grouping feature. Unit tests run in seconds locally; QA integration tests run in GitHub CI.

---

## Testing Strategy

### Local Development (Fast Feedback)
```bash
# Run unit tests locally - completes in <5 seconds
cd src/pmrep/test && make test
```

### GitHub CI (Integration Tests)
- QA tests (`qa/check -g pmrep`) run only in GitHub CI
- Do NOT attempt to run QA tests locally
- Rely on CI for full integration coverage

### Commit Policy
- **Each phase = one git commit**
- **Only commit when unit tests pass**
- Do NOT wait for full QA cycle before committing
- CI will validate integration tests after push

---

## Current State Analysis

### Existing Infrastructure (We Can Leverage)

- **Framework**: `unittest` with `unittest.mock` (already used by mpstat, pidstat, ps)
- **GNUmakefile pattern**: `pmpython -m unittest discover -s . -p '*_test.py'`
- **29+ existing test files** demonstrating PCP mocking patterns

### Why mpstat/pidstat Are Testable

```python
# Dependency Injection pattern - mock-friendly
class CpuUtil:
    def __init__(self, delta_time, metric_repository):  # DI!
        self.__metric_repository = metric_repository
```

### Why pmrep Is Not Testable

```python
# Tight coupling - cannot mock
class PMReporter:
    def __init__(self):
        self.pmconfig = pmconfig.pmConfig(self)  # Circular reference!
        # 40+ attributes, config parsing in __init__
```

---

## Refactoring Strategy: Preserve Behavior, Enable Testing

### Principle: Characterization Tests First

Before refactoring, we capture current behavior:
1. Create test fixtures with sample configs and expected outputs
2. Tests verify refactored code produces identical output
3. Any behavior change is intentional and visible

---

## Phase 1: Test Infrastructure Setup [COMPLETED]

**Commit message**: `pmrep: add unit test infrastructure`

**Create directory structure**:
```
src/pmrep/
├── pmrep.py           (will be refactored)
├── GNUmakefile        (add test subdirectory)
└── test/
    ├── GNUmakefile
    ├── __init__.py
    └── test_smoke.py  (minimal smoke test to verify setup)
```

**Files to create**:

`src/pmrep/test/GNUmakefile`:
```makefile
TOPDIR = ../../..
include $(TOPDIR)/src/include/builddefs

SCRIPT = pmrep.py
MODULE = pcp_pmrep.py

LDIRT = $(MODULE) __pycache__

default default_pcp build-me install install_pcp:

include $(BUILDRULES)

ifeq "$(HAVE_PYTHON)" "true"
check test:
	@rm -f $(MODULE)
	$(LN_S) ../$(SCRIPT) $(MODULE)
	pmpython -m unittest discover -s . -p 'test_*.py' -v
endif
```

`src/pmrep/test/__init__.py`: (empty file)

`src/pmrep/test/test_smoke.py`:
```python
#!/usr/bin/env pmpython
import unittest

class TestSmoke(unittest.TestCase):
    def test_import(self):
        """Verify pmrep module can be imported"""
        import pcp_pmrep
        self.assertTrue(hasattr(pcp_pmrep, 'PMReporter'))

if __name__ == '__main__':
    unittest.main()
```

**Modify** `src/pmrep/GNUmakefile` to add SUBDIRS:
```makefile
SUBDIRS = test

check:: $(SUBDIRS)
	$(SUBDIRS_MAKERULE)
```

**Verification**:
```bash
cd src/pmrep/test && make test
# Should show: test_import ... ok
```

**Commit when**: Smoke test passes locally

---

## Phase 2: Extract Pure Functions [COMPLETED]

**Commit message**: `pmrep: extract pure formatting functions with tests`

Extract functions that have no side effects and can be tested immediately:

| Function | Lines | Description |
|----------|-------|-------------|
| `parse_non_number(value, width)` | 1165-1174 | Handle inf/-inf/NaN |
| `remove_delimiter(value, delimiter)` | 1176-1183 | Clean string values |
| `option_override(opt)` | 260-264 | Check option overrides |

**Refactoring approach**:
```python
# Before: method on class (in pmrep.py)
def parse_non_number(self, value, width=8):
    ...

# After: module-level function (still in pmrep.py, but no self dependency)
def parse_non_number(value, width=8):
    """Check and handle float inf, -inf, and NaN"""
    if math.isinf(value):
        if value > 0:
            return "inf" if width >= 3 else pmconfig.TRUNC
        else:
            return "-inf" if width >= 4 else pmconfig.TRUNC
    elif math.isnan(value):
        return "NaN" if width >= 3 else pmconfig.TRUNC
    return value

# Update method to call function
class PMReporter:
    def parse_non_number(self, value, width=8):
        return parse_non_number(value, width)
```

**Create** `src/pmrep/test/test_formatting.py`:
```python
#!/usr/bin/env pmpython
import unittest
import math
from pcp_pmrep import parse_non_number, remove_delimiter

class TestParseNonNumber(unittest.TestCase):
    def test_positive_infinity(self):
        self.assertEqual(parse_non_number(float('inf'), 8), 'inf')

    def test_positive_infinity_narrow_width(self):
        # Width too small for "inf" (3 chars)
        result = parse_non_number(float('inf'), 2)
        self.assertNotEqual(result, 'inf')  # Should be truncated

    def test_negative_infinity(self):
        self.assertEqual(parse_non_number(float('-inf'), 8), '-inf')

    def test_negative_infinity_narrow_width(self):
        # Width too small for "-inf" (4 chars)
        result = parse_non_number(float('-inf'), 3)
        self.assertNotEqual(result, '-inf')

    def test_nan(self):
        self.assertEqual(parse_non_number(float('nan'), 8), 'NaN')

    def test_nan_narrow_width(self):
        result = parse_non_number(float('nan'), 2)
        self.assertNotEqual(result, 'NaN')

    def test_regular_number_passthrough(self):
        self.assertEqual(parse_non_number(42.5, 8), 42.5)

    def test_integer_passthrough(self):
        self.assertEqual(parse_non_number(42, 8), 42)


class TestRemoveDelimiter(unittest.TestCase):
    def test_replaces_comma_with_underscore(self):
        result = remove_delimiter("foo,bar", ",")
        self.assertEqual(result, "foo_bar")

    def test_replaces_underscore_with_space(self):
        result = remove_delimiter("foo_bar", "_")
        self.assertEqual(result, "foo bar")

    def test_no_delimiter_in_string(self):
        result = remove_delimiter("foobar", ",")
        self.assertEqual(result, "foobar")

    def test_non_string_passthrough(self):
        result = remove_delimiter(42, ",")
        self.assertEqual(result, 42)


if __name__ == '__main__':
    unittest.main()
```

**Verification**:
```bash
cd src/pmrep/test && make test
# Should show all tests passing
```

**Commit when**: All formatting tests pass locally

---

## Phase 3: Extract Value Formatter [COMPLETED]

**Commit message**: `pmrep: extract format_stdout_value with tests`

The `format_stdout_value()` method (lines 1251-1284) is complex but nearly pure.

**Refactoring**: Extract to module-level function, keep method as wrapper.

**Add to** `src/pmrep/test/test_formatting.py`:
```python
from pcp_pmrep import format_stdout_value, TRUNC

class TestFormatStdoutValue(unittest.TestCase):
    def test_integer_fits(self):
        val, fmt = format_stdout_value(42, width=8, precision=3)
        self.assertEqual(val, 42)
        self.assertIn("8d", fmt)

    def test_integer_too_wide(self):
        val, fmt = format_stdout_value(123456789, width=5, precision=3)
        self.assertEqual(val, TRUNC)

    def test_float_with_precision(self):
        val, fmt = format_stdout_value(3.14159, width=8, precision=3)
        self.assertIsInstance(val, float)
        self.assertIn(".3f", fmt) or self.assertIn(".2f", fmt)

    def test_float_too_wide_becomes_int(self):
        # When float won't fit with decimals, should convert to int
        val, fmt = format_stdout_value(12345.67, width=6, precision=3)
        self.assertIsInstance(val, int)

    def test_string_newline_escaped(self):
        val, fmt = format_stdout_value("foo\nbar", width=10, precision=3)
        self.assertEqual(val, "foo\\nbar")

    def test_infinity_handled(self):
        val, fmt = format_stdout_value(float('inf'), width=8, precision=3)
        self.assertEqual(val, 'inf')

    def test_nan_handled(self):
        val, fmt = format_stdout_value(float('nan'), width=8, precision=3)
        self.assertEqual(val, 'NaN')
```

**Verification**:
```bash
cd src/pmrep/test && make test
```

**Commit when**: All value formatter tests pass locally

---

## Phase 4: Configuration Dataclasses [COMPLETED]

**Commit message**: `pmrep: add configuration dataclasses with tests`

Replaces 40+ scattered attributes with structured, immutable configuration objects.

**Create** `src/pmrep/config.py`:
```python
"""Configuration structures for pmrep"""
from dataclasses import dataclass, field
from typing import Optional, List

@dataclass
class OutputConfig:
    """Output-related configuration"""
    output: str = "stdout"
    outfile: Optional[str] = None
    delimiter: str = "  "
    # ... (full definition in original plan)

@dataclass
class FilterConfig:
    """Filtering and ranking configuration"""
    rank: int = 0
    # ... (full definition in original plan)
```

**Create** `src/pmrep/test/test_config.py`:
```python
#!/usr/bin/env pmpython
import unittest
from config import OutputConfig, FilterConfig

class TestOutputConfig(unittest.TestCase):
    def test_defaults(self):
        config = OutputConfig()
        self.assertEqual(config.output, "stdout")
        self.assertEqual(config.delimiter, "  ")
        self.assertTrue(config.header)

    def test_custom_values(self):
        config = OutputConfig(output="csv", delimiter=",")
        self.assertEqual(config.output, "csv")
        self.assertEqual(config.delimiter, ",")
```

**Commit when**: Config tests pass locally

---

## Phase 5: Extract Header Formatter [COMPLETED]

**Commit message**: `pmrep: extract header formatter with tests`

Header generation (lines 892-1008) is complex but has clear inputs/outputs.

**Create** `src/pmrep/header.py` (see full implementation in original plan)

**Create** `src/pmrep/test/test_header.py`:
```python
#!/usr/bin/env pmpython
import unittest
from collections import OrderedDict
from header import HeaderFormatter

class TestHeaderFormatter(unittest.TestCase):
    def test_build_format_string_single_metric(self):
        formatter = HeaderFormatter(delimiter="  ", timestamp_width=8)
        metrics = OrderedDict([('cpu.user', ['usr', None, ['%'], None, 8])])
        instances = [([0], ['cpu0'])]

        fmt = formatter.build_format_string(metrics, instances, with_timestamp=True)

        self.assertIn("{0:<8}", fmt)  # timestamp
        self.assertIn(":>8.8}", fmt)  # metric width

    def test_build_format_string_no_timestamp(self):
        formatter = HeaderFormatter(delimiter=" ", timestamp_width=0)
        metrics = OrderedDict([('cpu', ['cpu', None, ['%'], None, 5])])
        instances = [([0], ['cpu0'])]

        fmt = formatter.build_format_string(metrics, instances, with_timestamp=False)

        self.assertTrue(fmt.startswith("{0:}{1}"))

    def test_format_header_row(self):
        formatter = HeaderFormatter(delimiter=" ", timestamp_width=8)
        metrics = OrderedDict([('cpu', ['usr', None, ['%'], None, 5])])
        instances = [([0], ['cpu0'])]
        fmt = "{0:}{1}{2:>5.5}"

        names, insts, units = formatter.format_header_row(
            fmt, metrics, instances, show_instances=True
        )

        self.assertIn('usr', names)
        self.assertIn('cpu0', insts)
        self.assertIn('%', units)


if __name__ == '__main__':
    unittest.main()
```

**Commit when**: Header formatter tests pass locally

---

## Phase 6: Extract Metric Repository [COMPLETED]

**Commit message**: `pmrep: add MetricRepository abstraction for testability`

This is the key refactor - creates a mockable interface following mpstat/pidstat pattern.

**Create** `src/pmrep/metrics.py`:
```python
"""Metric access abstraction for pmrep"""
from typing import Dict, List, Tuple, Any, Optional

class MetricRepository:
    """
    Abstraction layer for metric access.

    In production: delegates to pmconfig
    In tests: can be mocked to return predetermined values
    """

    def __init__(self, pmconfig, pmfg_ts_callable):
        self._pmconfig = pmconfig
        self._pmfg_ts = pmfg_ts_callable

    def get_ranked_results(self, valid_only: bool = True) -> Dict[str, List[Tuple]]:
        return self._pmconfig.get_ranked_results(valid_only=valid_only)

    def fetch(self) -> int:
        return self._pmconfig.fetch()

    def pause(self):
        self._pmconfig.pause()

    def timestamp(self):
        return self._pmfg_ts()

    @property
    def insts(self):
        return self._pmconfig.insts

    @property
    def descs(self):
        return self._pmconfig.descs
```

**Create** `src/pmrep/test/test_metrics.py`:
```python
#!/usr/bin/env pmpython
import unittest
from unittest.mock import Mock
from metrics import MetricRepository

class TestMetricRepository(unittest.TestCase):
    def setUp(self):
        self.mock_pmconfig = Mock()
        self.mock_pmfg_ts = Mock(return_value="12:00:00")
        self.repo = MetricRepository(self.mock_pmconfig, self.mock_pmfg_ts)

    def test_get_ranked_results_delegates(self):
        expected = {'metric': [(0, 'inst', 42)]}
        self.mock_pmconfig.get_ranked_results.return_value = expected

        result = self.repo.get_ranked_results()

        self.assertEqual(result, expected)
        self.mock_pmconfig.get_ranked_results.assert_called_once_with(valid_only=True)

    def test_fetch_delegates(self):
        self.mock_pmconfig.fetch.return_value = 0

        result = self.repo.fetch()

        self.assertEqual(result, 0)
        self.mock_pmconfig.fetch.assert_called_once()

    def test_timestamp_calls_callable(self):
        result = self.repo.timestamp()

        self.assertEqual(result, "12:00:00")
        self.mock_pmfg_ts.assert_called_once()

    def test_insts_property(self):
        self.mock_pmconfig.insts = [([0, 1], ['cpu0', 'cpu1'])]

        result = self.repo.insts

        self.assertEqual(result, [([0, 1], ['cpu0', 'cpu1'])])


if __name__ == '__main__':
    unittest.main()
```

**Update** `src/pmrep/pmrep.py`:
```python
class PMReporter:
    def __init__(self, metric_repo=None):
        # ... existing init ...
        self._metric_repo = metric_repo  # Injected for testing

    def connect(self):
        # ... existing connection logic ...
        if self._metric_repo is None:
            from metrics import MetricRepository
            self._metric_repo = MetricRepository(self.pmconfig, self.pmfg_ts)
```

**Commit when**: MetricRepository tests pass locally

---

## Phase 7: Group Header Feature (TDD) [COMPLETED]

**Commit message**: `pmrep: add column grouping feature (TDD)`

With infrastructure in place, implement column grouping with true TDD.

**TDD Workflow**:
1. Write test for `GroupConfig` class
2. Run test, see it fail
3. Implement `GroupConfig`
4. Run test, see it pass
5. Write test for `GroupHeaderFormatter.calculate_spans()`
6. Run test, see it fail
7. Implement `calculate_spans()`
8. Continue...

**Create** `src/pmrep/test/test_groups.py`:
```python
#!/usr/bin/env pmpython
import unittest
from groups import GroupConfig, GroupHeaderFormatter

class TestGroupConfig(unittest.TestCase):
    def test_defaults(self):
        group = GroupConfig('memory', ['free', 'buff'])
        self.assertEqual(group.handle, 'memory')
        self.assertEqual(group.label, 'memory')  # Default to handle
        self.assertEqual(group.align, 'center')
        self.assertIsNone(group.prefix)

    def test_custom_label(self):
        group = GroupConfig('memory', ['free'], label='mem')
        self.assertEqual(group.label, 'mem')


class TestGroupHeaderFormatter(unittest.TestCase):
    def test_calculate_spans_single_group(self):
        groups = [GroupConfig('memory', ['free', 'buff'], label='mem')]
        formatter = GroupHeaderFormatter(groups, delimiter='  ')

        spans = formatter.calculate_spans({'free': 8, 'buff': 8})

        self.assertEqual(len(spans), 1)
        self.assertEqual(spans[0][0], 'mem')
        self.assertEqual(spans[0][1], 18)  # 8 + 2 + 8

    def test_calculate_spans_multiple_groups(self):
        groups = [
            GroupConfig('procs', ['r', 'b'], label='procs'),
            GroupConfig('memory', ['free', 'buff', 'cache'], label='memory')
        ]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        spans = formatter.calculate_spans({
            'r': 3, 'b': 3,
            'free': 8, 'buff': 8, 'cache': 8
        })

        self.assertEqual(len(spans), 2)
        self.assertEqual(spans[0][1], 7)   # 3 + 1 + 3
        self.assertEqual(spans[1][1], 26)  # 8 + 1 + 8 + 1 + 8

    def test_format_header_center_aligned(self):
        groups = [GroupConfig('mem', ['a', 'b'], align='center')]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        header = formatter.format_header([('mem', 10, 'center')])

        self.assertEqual(len(header), 10)
        self.assertIn('mem', header)

    def test_format_header_left_aligned(self):
        groups = [GroupConfig('mem', ['a'], align='left')]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        header = formatter.format_header([('mem', 10, 'left')])

        self.assertTrue(header.startswith('mem'))

    def test_format_header_right_aligned(self):
        groups = [GroupConfig('mem', ['a'], align='right')]
        formatter = GroupHeaderFormatter(groups, delimiter=' ')

        header = formatter.format_header([('mem', 10, 'right')])

        self.assertTrue(header.endswith('mem'))

    def test_format_header_with_separator(self):
        groups = [
            GroupConfig('a', ['x'], label='A'),
            GroupConfig('b', ['y'], label='B')
        ]
        formatter = GroupHeaderFormatter(groups, delimiter=' ', groupsep='|')

        header = formatter.format_header([('A', 5, 'center'), ('B', 5, 'center')])

        self.assertIn('|', header)


if __name__ == '__main__':
    unittest.main()
```

**Create** `src/pmrep/groups.py` (implement to make tests pass)

**Commit when**: All group header tests pass locally

---

## Files Summary

| File | Action | Phase | Description |
|------|--------|-------|-------------|
| `src/pmrep/test/GNUmakefile` | Create | 1 | Test runner |
| `src/pmrep/test/__init__.py` | Create | 1 | Package marker |
| `src/pmrep/test/test_smoke.py` | Create | 1 | Verify setup works |
| `src/pmrep/GNUmakefile` | Modify | 1 | Add SUBDIRS = test |
| `src/pmrep/test/test_formatting.py` | Create | 2-3 | Formatting function tests |
| `src/pmrep/pmrep.py` | Modify | 2-3 | Extract pure functions |
| `src/pmrep/config.py` | Create | 4 | Config dataclasses (optional) |
| `src/pmrep/test/test_config.py` | Create | 4 | Config tests (optional) |
| `src/pmrep/header.py` | Create | 5 | Header formatter |
| `src/pmrep/test/test_header.py` | Create | 5 | Header tests |
| `src/pmrep/metrics.py` | Create | 6 | MetricRepository |
| `src/pmrep/test/test_metrics.py` | Create | 6 | Repository tests |
| `src/pmrep/groups.py` | Create | 7 | Group header feature |
| `src/pmrep/test/test_groups.py` | Create | 7 | Group tests (TDD) |

---

## Verification Commands

```bash
# Run unit tests locally (should complete in <5 seconds)
cd src/pmrep/test && make test

# Or from project root:
make -C src/pmrep/test test

# Verify all tests pass before each commit
cd src/pmrep/test && make test && echo "Ready to commit!"
```

---

## Commit Checklist

For each phase:
1. [ ] Implement the changes
2. [ ] Run `cd src/pmrep/test && make test`
3. [ ] All tests pass
4. [ ] Create commit with phase-specific message
5. [ ] Push to trigger CI (QA tests run in GitHub)

---

## Risk Mitigation

1. **Backward Compatibility**: All refactoring preserves public behavior
2. **Incremental**: Each phase is independently committable
3. **Rollback**: Each commit can be reverted if CI fails
4. **Local Validation**: Unit tests catch issues before pushing
5. **CI Integration**: QA tests validate no regressions after push
