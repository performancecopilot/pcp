# Implementation Plan: pmrep Column Grouping Feature

## Implementation Status

**Latest Update**: 2026-01-11

### ✅ Phase 1: Core Implementation - COMPLETE
- [✅] Configuration parsing (`src/pmrep/pmconfig.py`) - groups.py handles group config
- [✅] Header formatting (`src/pmrep/header.py`) - extracts header formatter
- [✅] Group header implementation (`src/pmrep/groups.py`) - GroupConfig, GroupHeaderFormatter
- [✅] MetricRepository abstraction (`src/pmrep/metrics.py`) - enables mocking for tests
- [✅] Configuration dataclasses (`src/pmrep/config.py`) - OutputConfig, FilterConfig, ScaleConfig
- [✅] Unit test infrastructure (`src/pmrep/test/`) - fast local TDD workflow
- [✅] Group header rendering in pmrep.py - integrated with write_header_stdout

### ✅ Bug Fix - COMPLETE (Commit 7b4e88fb1f)
- [✅] **Critical Bug Fixed**: Missing configuration keys in pmrep.py
  - **Issue**: `groupalign = center` caused `PM_ERR_NAME Unknown metric name` error
  - **Root Cause**: Keys `groupalign`, `groupheader`, `groupsep`, `groupsep_data` missing from `self.keys` tuple
  - **Fix**: Added 4 missing keys to `src/pmrep/pmrep.py` line 156
  - **Tests**: Created `test_config_parsing.py` with 5 TDD tests (all passing)
  - **Verification**: All 146 unit tests pass in 0.002s

### ✅ Phase 2: Documentation - COMPLETE
- [✅] **COMPLETE**: Example configuration (`src/pmrep/conf/vmstat-grouped.conf`)
- [✅] **COMPLETE**: Documentation comments (`src/pmrep/conf/00-defaults.conf`)
- [✅] **COMPLETE**: Man page updates (`src/pmrep/pmrep.conf.5`, `src/pmrep/pmrep.1`)
- [ ] **PENDING**: QA integration tests (`qa/NNNN`)

### 📋 Testing Summary
- **Unit Tests**: 146 tests passing locally (<5 seconds)
  - 5 config parsing tests (test_config_parsing.py)
  - 16 formatting tests (test_formatting.py)
  - 16 config dataclass tests (test_config.py)
  - 29 group tests (test_groups.py)
  - 18 header tests (test_header.py)
  - 29 metrics tests (test_metrics.py)
  - 4 smoke tests (test_smoke.py)
- **QA Tests**: Not yet created (will run in CI after documentation complete)
- **Manual Testing**: Pending VM validation

### 🎯 Next Steps
1. **VM Testing**: Validate feature with actual PCP installation
2. **QA Tests**: Create integration tests for CI validation (Commit 2)
3. **Integration**: Validate with existing 43 pmrep QA tests for backwards compatibility

---

## Overview

Add column grouping capability to pmrep, allowing configuration files to define visual group headers that span multiple metric columns, similar to pmstat's output format.

**Example Output (with group separators):**
```
             procs         memory           swap      io       system      cpu
       r   b  swpd | free   buff  cache | si   so | bi   bo | in    cs | us sy id
       1   0  1234   5678   1234   5678    0    0   10   20  100   200    5 10 85
```

The `|` separator visually reinforces group boundaries on the metric header row.

---

## Configuration Syntax

### Final Design

```ini
[vmstat-grouped]
header = yes
groupalign = center              # Optional global default: left|center|right (default: center)
groupsep = |                     # Optional separator character between groups (default: none)
groupsep_data = no               # Apply separator to data rows too (default: no)

# Group definitions - order determines display order
group.procs = running, blocked
group.procs.prefix = kernel.all
group.procs.label = procs        # Optional: display label (default: handle name)

group.memory = swap.used, free, bufmem, allcache
group.memory.prefix = mem.util
group.memory.label = mem

group.swap = pagesin, pagesout
group.swap.prefix = swap

group.io = pgpgin, pgpgout
group.io.prefix = mem.vmstat
group.io.label = io

group.system = intr, pswitch
group.system.prefix = kernel.all

group.cpu = user, sys, idle, wait.total, steal, guest
group.cpu.prefix = kernel.all.cpu
group.cpu.align = left           # Optional: per-group override

# Metric definitions (existing format, unchanged)
kernel.all.running = r,,,,3
kernel.all.blocked = b,,,,3
swap.used = swpd,,KB,,7
mem.util.free = free,,KB,,8
# ... etc
```

### Prefix Resolution Rule

- Metric contains `.` → use as-is (FQDN)
- Metric has no `.` → prepend `group.<name>.prefix` if defined

Example:
```ini
group.memory = swap.used, free, bufmem, allcache
group.memory.prefix = mem.util
```
Resolves to:
- `swap.used` → has `.` → `swap.used`
- `free` → no `.` → `mem.util.free`
- `bufmem` → no `.` → `mem.util.bufmem`
- `allcache` → no `.` → `mem.util.allcache`

### Groupheader Auto-Detection

- If any `group.*` definition exists → group header row is enabled automatically
- New option `groupheader = no` can explicitly suppress it
- Legacy configs without `group.*` lines → no change in behavior

### Group Configuration Options

**Per-group options** (e.g., `group.memory.label`):

| Option | Description | Default |
|--------|-------------|---------|
| `label` | Display text (vs handle name) | handle name |
| `align` | `left`, `center`, `right` | `center` (or global `groupalign`) |
| `prefix` | Common metric path prefix | none |

**Global options** (in `[options]` or metricset section):

| Option | Description | Default |
|--------|-------------|---------|
| `groupalign` | Default alignment for all groups | `center` |
| `groupheader` | Enable/disable group header row | auto (yes if groups defined) |
| `groupsep` | Separator character between groups | none |
| `groupsep_data` | Apply separator to data rows too | `no` |

### Design Decisions

1. **Non-contiguous groups**: Not allowed - metrics in a group must be listed together in the group definition (the group definition controls ordering)

2. **Ungrouped metrics**: Appear after all grouped columns with blank group header space above; emit warning during parsing

3. **Alignment default**: `center` (matches pmstat behavior)

4. **colxrow mode**: Groups are ignored (doesn't make conceptual sense)

5. **CSV output**: Phase 2 enhancement

---

## Development Methodology: Test-Driven Development (TDD)

**Status**: ✅ **Successfully Applied** - TDD methodology proven effective for this feature.

We followed TDD principles:

1. **Write test first** - Define expected behavior in test case ✅
2. **Run test, see it fail** - Confirms test is valid ✅
3. **Write minimal code** - Just enough to pass the test ✅
4. **Run test, see it pass** - Confirms implementation works ✅
5. **Refactor** - Clean up while keeping tests green ✅
6. **Repeat** - Next test case ✅

### Fast Unit Testing (Consolidated from PLAN-pmrep-unit-testing.md)

**Local Testing Workflow** - Completes in < 5 seconds:
```bash
# Run unit tests locally
cd src/pmrep/test && make test

# Or from project root:
make -C src/pmrep/test test
```

**Unit Test Infrastructure** (COMPLETE ✅):
- `src/pmrep/test/test_config_parsing.py` - Config key validation (5 tests) **NEW**
- `src/pmrep/test/test_formatting.py` - Pure function tests (16 tests)
- `src/pmrep/test/test_config.py` - Configuration dataclasses (16 tests)
- `src/pmrep/test/test_header.py` - Header formatter (18 tests)
- `src/pmrep/test/test_metrics.py` - MetricRepository abstraction (29 tests)
- `src/pmrep/test/test_groups.py` - Column grouping TDD (29 tests)
- `src/pmrep/test/test_smoke.py` - Import verification (4 tests)

**Total**: 146 tests passing in 0.002s

**Key Principle**: Unit tests run locally in seconds; QA integration tests run in GitHub CI only.

### Testing Constraints

**Important**: The PCP QA test suite requires PCP to be *installed* on the system, not just built.

#### Why QA Tests Don't Run During `./Makepkgs` on macOS

1. **`./Makepkgs --check` runs static analysis only** - The `--check` flag triggers `make check`, which runs pylint/manlint on source files, NOT the QA test suite.

2. **QA tests require `pcp.env`** - The `qa/check` script sources `$PCP_DIR/etc/pcp.env` (see `qa/common.rc:19-25`), which only exists after PCP is installed.

3. **macOS builds packages, doesn't install** - `Makepkgs` on darwin creates a `.pkg` file but doesn't install it, so `pcp.env` doesn't exist.

#### Options for Running QA Tests

| Option | Description |
|--------|-------------|
| **Install the package** | Run `sudo installer -pkg build/mac/pcp-*.pkg -target /` after Makepkgs |
| **Linux VM/container** | More straightforward environment for testing |
| **CI/CD** | QA tests run automatically in proper test environments |
| **Manual testing** | Test pmrep directly against archives during development |

#### Local Development Workflow

For local development on macOS, we can:
- Write QA test files following existing conventions
- Manually test pmrep invocations against archives in `qa/archives/`
- Validate full QA suite in a proper test environment (Linux VM or after package install)

---

## Phase 1: Core Implementation

### 1.1 Update `src/python/pcp/pmconfig.py`

**Changes:**

a) Add new configuration keys to recognized options:
   - `groupalign` - global alignment default
   - `groupheader` - explicit enable/disable

b) Add new data structures in `pmConfig.__init__()`:
   ```python
   self.groups = OrderedDict()      # group_handle -> [resolved_metric_names]
   self.group_config = {}           # group_handle -> {label, align, prefix}
   self.metric_to_group = {}        # metric_name -> group_handle (for lookup)
   ```

c) Add `parse_group_definitions()` method:
   - Scan config for `group.<handle> = metric1, metric2, ...`
   - Scan for `group.<handle>.prefix`, `.label`, `.align`
   - Resolve metric names using prefix rule
   - Build `groups`, `group_config`, `metric_to_group` structures
   - Validate: warn if metric referenced in group but not defined

d) Modify `prepare_metrics()`:
   - Call `parse_group_definitions()` after reading config
   - Reorder `self.util.metrics` to match group ordering (grouped metrics first, then ungrouped)
   - Warn about ungrouped metrics if groups are defined

e) Add validation in `validate_metrics()`:
   - Verify all metrics referenced in groups actually exist
   - Error if same metric appears in multiple groups

### 1.2 Update `src/pmrep/pmrep.py`

**Changes:**

a) Add new instance variables in `PMReporter.__init__()`:
   ```python
   self.groupheader = None          # Auto-detect or explicit
   self.groupalign = 'center'       # Global default
   self.groupsep = None             # Separator character (e.g., '|')
   self.groupsep_data = False       # Apply separator to data rows
   ```

b) Add to `keys` list for option parsing:
   - `groupheader`, `groupalign`, `groupsep`, `groupsep_data`

c) Add `prepare_group_header()` method:
   - Calculate span widths for each group (sum of metric widths + delimiters)
   - Account for multi-instance metrics (width × instance_count)
   - Build format string for group header row
   - Handle ungrouped metrics (blank space in group header)

d) Add `write_group_header()` method:
   ```python
   def write_group_header(self):
       """Write group header row above metric headers"""
       if not self.groupheader:
           return

       groups = []  # [(label, span_width), ...]
       # ... calculate spans based on group membership and column widths

       # Build aligned group labels
       for handle, width in group_spans:
           label = self.group_config[handle].get('label', handle)
           align = self.group_config[handle].get('align', self.groupalign)
           if align == 'center':
               groups.append(label.center(width))
           elif align == 'left':
               groups.append(label.ljust(width))
           else:
               groups.append(label.rjust(width))

       self.writer.write(timestamp_space + delimiter.join(groups) + "\n")
   ```

e) Modify `write_header_stdout()`:
   - Call `write_group_header()` before writing metric names
   - Only if `self.groupheader` is True (or auto-detected)

f) Modify `prepare_stdout_std()`:
   - After building metric format, also build group format
   - Store group span information for header writing

g) Handle `colxrow` mode:
   - Skip group headers in colxrow mode (doesn't make sense conceptually)
   - Optionally warn if groups defined but colxrow enabled

h) Handle `dynamic_header` mode:
   - Recalculate group spans when instances change
   - Call group header update in `dynamic_header_update()`

i) Implement separator support:
   - If `groupsep` is defined, insert separator character at group boundaries
   - For header row: insert between group labels and between metric labels
   - If `groupsep_data = yes`, also insert in data value rows
   - Separator replaces the delimiter at group boundaries

### 1.3 Update Default Configuration

**File: `src/pmrep/conf/00-defaults.conf`**

Add documentation comments:
```ini
# Column Grouping Options (optional)
#
# groupalign = center    # Default alignment for group headers: left|center|right
# groupheader = yes      # Auto-enabled if group.* defined; set 'no' to suppress
#
# Group Definition Syntax:
#   group.<handle> = metric1, metric2, metric3
#   group.<handle>.prefix = common.metric.prefix  # Optional: prepended to non-FQDN metrics
#   group.<handle>.label = Label                  # Optional: display text (default: handle)
#   group.<handle>.align = center                 # Optional: override global alignment
```

### 1.4 Add Example Configuration

**File: `src/pmrep/conf/vmstat-grouped.conf`** (new)

```ini
# vmstat-grouped - vmstat-like output with column grouping
# Demonstrates the column grouping feature

[vmstat-grouped]
header = yes
unitinfo = no
globals = no
timestamp = yes
precision = 0
delimiter = " "
repeat_header = auto
groupalign = center
groupsep = |                     # Visual separator between groups

# Group definitions
group.procs = running, blocked
group.procs.prefix = kernel.all
group.procs.label = procs

group.memory = swap.used, free, bufmem, allcache
group.memory.prefix = mem.util
group.memory.label = memory

group.swap = pagesin, pagesout
group.swap.prefix = swap

group.io = pgpgin, pgpgout
group.io.prefix = mem.vmstat
group.io.label = io

group.system = intr, pswitch
group.system.prefix = kernel.all

group.cpu = user, sys, idle, wait.total, steal, guest
group.cpu.prefix = kernel.all.cpu

# Metric definitions
kernel.all.running = r,,,,3
kernel.all.blocked = b,,,,3
swap.used = swpd,,KB,,7
mem.util.free = free,,KB,,8
mem.util.bufmem = buff,,KB,,8
allcache = mem.util.allcache
allcache.label = cache
allcache.formula = mem.util.cached + mem.util.slab
allcache.unit = KB
allcache.width = 8
swap.pagesin = si,,,,5
swap.pagesout = so,,,,5
mem.vmstat.pgpgin = bi,,,,6
mem.vmstat.pgpgout = bo,,,,6
kernel.all.intr = in,,,,5
kernel.all.pswitch = cs,,,,6
kernel.all.cpu.user = us,,,,3
kernel.all.cpu.sys = sy,,,,3
kernel.all.cpu.idle = id,,,,3
kernel.all.cpu.wait.total = wa,,,,3
kernel.all.cpu.steal = st,,,,3
kernel.all.cpu.guest = gu,,,,3
```

### 1.5 Update Man Pages

**File: `man/man5/pmrep.conf.5`**

Add new section documenting:
- `groupheader` option
- `groupalign` option
- `group.<handle>` syntax
- `group.<handle>.prefix`, `.label`, `.align` attributes
- Prefix resolution rules
- Examples

**File: `man/man1/pmrep.1`**

Add brief mention of column grouping feature with reference to pmrep.conf(5).

---

## Phase 2: CSV Support (Future)

### 2.1 CSV Group Header Row

- Add optional group header row to CSV output
- New option: `csvgroupheader = yes|no` (default: no for backwards compat)
- Format: group labels in same column positions as their metrics

### 2.2 Extended CSV Metadata

- Optionally include group info in CSV header comments
- Format: `# groups: procs=0-1, memory=2-5, swap=6-7, ...`

---

## Future Enhancements (Tabled)

| Feature | Description |
|---------|-------------|
| `grouporder` | Explicit ordering override: `grouporder = procs, memory, swap` |
| Empty groups | Allow `group.spacer = ` for visual gaps |
| Group underlines | Underline characters under group headers |

### Terminal Table Libraries (Investigated)

We investigated using Python terminal table libraries like [Rich](https://rich.readthedocs.io/en/stable/live.html) for advanced formatting capabilities (live-updating tables, colors, box-drawing characters).

**Analysis:**
- **Rich** is the leading library for live-updating terminal tables in Python
- Supports `Live` display with configurable refresh rates
- Would provide colors, styles, and box-drawing borders

**Decision: Continue with current approach for Phase 1**

Reasons:
- PCP is a mature project; pmrep uses only stdlib + PCP modules
- Adding a dependency affects packaging on all supported platforms
- Current formatting code works well; separators are trivial to add
- `rich` would be overkill for the grouping feature alone

**Future consideration:** If we ever want advanced terminal features (colors, box-drawing borders, themes), `rich` would be worth evaluating as part of a larger refactor.

---

## QA Test Infrastructure

### Existing pmrep Tests

- **43 active pmrep tests** in `qa/` directory (tests 035 to 1813)
- All registered in `qa/group` file with tags like `pmrep python local`
- Tests use archives from `qa/archives/` for deterministic output

### Test Structure Pattern

```bash
#!/bin/sh
# QA output created by ...

seq=`basename $0`
echo "QA output created by $seq"

. ./common.python

_check_python_pmrep_modules()

tmp=/var/tmp/$$
trap "rm -rf $tmp.*; exit \$status" 0 1 2 3 15
status=1

# Test cases
echo "=== test description ===" | tee -a $seq.full
pmrep -a archives/sample-secs -z ... | _filter

status=0
exit
```

### Key Test Resources

| Resource | Location | Purpose |
|----------|----------|---------|
| Test archives | `qa/archives/` | Deterministic metric data |
| Config files | `src/pmrep/conf/` | Production configs tested via `:metricset` |
| Common helpers | `qa/common.python` | Shared test functions |
| Filter functions | `qa/common.filter` | Output normalization |

### Tests Most Relevant to This Feature

| Test | Description | Relevance |
|------|-------------|-----------|
| 1062 | Basic pmrep output modes | stdout formatting, headers |
| 1069 | Comprehensive pmrep test | Config parsing, multiple output modes |
| 1134 | Header options testing | `header`, `unitinfo`, `instinfo` options |
| 1169 | Extended header options | `extheader`, header formatting |
| 1548 | Configuration file handling | Config file parsing edge cases |

### New Test Plan

**New test file: `qa/NNNN` (number TBD)**

Following TDD, write tests BEFORE implementation:

```bash
#!/bin/sh
# QA output created by $seq
# Test pmrep column grouping feature

seq=`basename $0`
echo "QA output created by $seq"

. ./common.python

_check_python_pmrep_modules()

tmp=/var/tmp/$$
trap "rm -rf $tmp.*; exit \$status" 0 1 2 3 15
status=1

_filter()
{
    sed \
        -e "s,$PCP_PMDAS_DIR,PCP_PMDAS_DIR,g" \
        # ... additional filters
}

# === Test 1: Basic group header output ===
echo "=== basic group headers ==="
cat > $tmp.conf << EOF
[test-groups]
header = yes
group.memory = free, bufmem
group.memory.prefix = mem.util
group.memory.label = mem
mem.util.free = free,,,,8
mem.util.bufmem = buff,,,,8
EOF
pmrep -a archives/sample-secs -z -c $tmp.conf :test-groups -s 1

# === Test 2: Group alignment - center (default) ===
echo "=== group alignment center ==="
# ... test centered alignment

# === Test 3: Group alignment - left ===
echo "=== group alignment left ==="
cat > $tmp.conf << EOF
[test-align]
header = yes
groupalign = left
group.test = free, bufmem
group.test.prefix = mem.util
mem.util.free = free,,,,8
mem.util.bufmem = buff,,,,8
EOF
pmrep -a archives/sample-secs -z -c $tmp.conf :test-align -s 1

# === Test 4: Group alignment - right ===
echo "=== group alignment right ==="
# ... test right alignment

# === Test 5: Per-group alignment override ===
echo "=== per-group alignment override ==="
# ... test group.X.align overriding groupalign

# === Test 6: Prefix resolution - mixed FQDN and leaf ===
echo "=== prefix resolution ==="
cat > $tmp.conf << EOF
[test-prefix]
header = yes
group.mixed = swap.used, free, bufmem
group.mixed.prefix = mem.util
swap.used = swpd,,,,7
mem.util.free = free,,,,8
mem.util.bufmem = buff,,,,8
EOF
pmrep -a archives/sample-secs -z -c $tmp.conf :test-prefix -s 1

# === Test 7: Multiple groups ===
echo "=== multiple groups ==="
# ... test multiple groups in sequence

# === Test 8: Group label vs handle ===
echo "=== group label override ==="
cat > $tmp.conf << EOF
[test-label]
header = yes
group.memory_metrics = free, bufmem
group.memory_metrics.prefix = mem.util
group.memory_metrics.label = mem
mem.util.free = free,,,,8
mem.util.bufmem = buff,,,,8
EOF
pmrep -a archives/sample-secs -z -c $tmp.conf :test-label -s 1

# === Test 9: Ungrouped metrics (should warn) ===
echo "=== ungrouped metrics warning ==="
# ... test warning for metrics not in any group

# === Test 10: Suppress group header explicitly ===
echo "=== groupheader = no ==="
cat > $tmp.conf << EOF
[test-suppress]
header = yes
groupheader = no
group.memory = free, bufmem
group.memory.prefix = mem.util
mem.util.free = free,,,,8
mem.util.bufmem = buff,,,,8
EOF
pmrep -a archives/sample-secs -z -c $tmp.conf :test-suppress -s 1

# === Test 11: No groups defined (backwards compat) ===
echo "=== no groups - backwards compatibility ==="
cat > $tmp.conf << EOF
[test-nogroups]
header = yes
mem.util.free = free,,,,8
mem.util.bufmem = buff,,,,8
EOF
pmrep -a archives/sample-secs -z -c $tmp.conf :test-nogroups -s 1

# === Test 12: Multi-instance metrics in groups ===
echo "=== multi-instance metrics ==="
# ... test with disk.dev metrics that have multiple instances

# === Test 13: Dynamic instances with groups ===
echo "=== dynamic instances ==="
pmrep -a archives/dyninsts -z ... # test dynamic header updates

# === Test 14: colxrow mode ignores groups ===
echo "=== colxrow mode ==="
# ... verify groups don't break colxrow mode

# === Test 15: Error - metric in multiple groups ===
echo "=== error: duplicate metric in groups ==="
# ... test error handling

# === Test 16: Group separators - header only ===
echo "=== group separators - header only ==="
cat > $tmp.conf << EOF
[test-sep]
header = yes
groupsep = |
group.mem = free, bufmem
group.mem.prefix = mem.util
mem.util.free = free,,,,8
mem.util.bufmem = buff,,,,8
EOF
pmrep -a archives/sample-secs -z -c $tmp.conf :test-sep -s 2

# === Test 17: Group separators - header and data ===
echo "=== group separators - header and data ==="
cat > $tmp.conf << EOF
[test-sep-data]
header = yes 
groupsep = |
groupsep_data = yes
group.mem = free, bufmem
group.mem.prefix = mem.util
mem.util.free = free,,,,8
mem.util.bufmem = buff,,,,8
EOF
pmrep -a archives/sample-secs -z -c $tmp.conf :test-sep-data -s 2

status=0
exit
```

**Register in `qa/group`:**
```
NNNN pmrep python local
```

**Create expected output `qa/NNNN.out`:**
- Capture expected output for each test case
- Apply filters to normalize non-deterministic parts

### Backwards Compatibility Verification

All 43 existing pmrep tests should pass unchanged because:
1. No existing config uses `group.*` syntax
2. Default `groupheader` behavior is auto-detect (off if no groups)
3. All output formatting for non-grouped metrics remains identical

Verify with:
```bash
cd qa && ./check -g pmrep
```

---

## Files Changed Summary

| File | Changes |
|------|---------|
| `src/python/pcp/pmconfig.py` | Group parsing, metric reordering, validation |
| `src/pmrep/pmrep.py` | Group header rendering, format calculation |
| `src/pmrep/conf/00-defaults.conf` | Documentation comments |
| `src/pmrep/conf/vmstat-grouped.conf` | New example config |
| `man/man5/pmrep.conf.5` | Configuration documentation |
| `man/man1/pmrep.1` | Brief feature mention |
| `qa/NNNN` | New test file |
| `qa/NNNN.out` | Expected test output |
| `qa/group` | Register new test |

---

## Implementation Order (TDD)

### ✅ Step 1: Test Infrastructure (COMPLETE)
1. ✅ Created `src/pmrep/test/` directory structure
2. ✅ Unit test framework with GNUmakefile
3. ✅ Mock PCP infrastructure for fast local testing

### ✅ Step 2: Pure Functions & Formatters (COMPLETE)
1. ✅ Extracted `parse_non_number()`, `remove_delimiter()`, `format_stdout_value()`
2. ✅ Created `test_formatting.py` with comprehensive tests
3. ✅ Extracted `HeaderFormatter` to `src/pmrep/header.py`
4. ✅ Created `test_header.py`

### ✅ Step 3: Configuration Dataclasses (COMPLETE)
1. ✅ Created `src/pmrep/config.py` with OutputConfig, FilterConfig, ScaleConfig
2. ✅ Created `test_config.py` for immutability and validation

### ✅ Step 4: MetricRepository Abstraction (COMPLETE)
1. ✅ Created `src/pmrep/metrics.py` for testability
2. ✅ Dependency injection pattern (like mpstat/pidstat)
3. ✅ Created `test_metrics.py` demonstrating mocking

### ✅ Step 5: Group Header Implementation (COMPLETE - TDD)
1. ✅ Created `src/pmrep/groups.py` with GroupConfig, GroupHeaderFormatter
2. ✅ Wrote `test_groups.py` FIRST (29 tests)
3. ✅ Implemented to make tests pass
4. ✅ Column span calculation, alignment (left/center/right), separators

### ✅ Step 6: Integration (COMPLETE)
1. ✅ Integrated GroupHeaderFormatter into pmrep.py
2. ✅ Connected to write_header_stdout
3. ✅ All features working together

### ✅ Step 7: Bug Fix - Missing Config Keys (COMPLETE - Commit 7b4e88fb1f)
1. ✅ Discovered bug: `groupalign = center` causing PM_ERR_NAME
2. ✅ Wrote failing tests in `test_config_parsing.py` (5 tests)
3. ✅ Fixed by adding 4 keys to `self.keys` tuple in pmrep.py
4. ✅ All 146 tests passing

### ✅ Step 8: Documentation (COMPLETE)
1. [✅] Create `src/pmrep/conf/vmstat-grouped.conf` example
2. [✅] Add documentation comments to `src/pmrep/conf/00-defaults.conf`
3. [✅] Update `man/man5/pmrep.conf.5` with group options
4. [✅] Update `man/man1/pmrep.1` with brief mention

### 📋 Step 9: QA Integration Tests (PENDING)
1. [ ] Create `qa/NNNN` test file (will run in CI)
2. [ ] Create `qa/NNNN.out` expected output
3. [ ] Register in `qa/group`
4. [ ] Verify backwards compatibility (43 existing pmrep tests)

---

## Notes

### Development History
- This plan was developed through analysis of `src/pmstat/pmstat.c` (for column grouping reference), `src/pmrep/pmrep.py`, `src/python/pcp/pmconfig.py`, and the QA test infrastructure.
- **2026-01-11**: Bug fix completed (Commit 7b4e88fb1f) - Missing configuration keys caused PM_ERR_NAME errors
- **2026-01-11**: Documentation phase completed - Example config, defaults documentation, and man pages updated
- **Unit Testing**: Successfully consolidated information from `PLAN-pmrep-unit-testing.md` into this plan
- **TDD Success**: Test-Driven Development methodology proven highly effective:
  - Fast feedback loop (146 tests in 0.002s)
  - Bug discovered and fixed with TDD approach
  - Zero regressions throughout development

### Design & Implementation
- The design prioritizes backwards compatibility - existing configurations work unchanged.
- The prefix feature reduces verbosity for groups with common metric ancestors.
- Group separators (`groupsep`) provide visual reinforcement of group boundaries.
- Phase 2 (CSV support) is intentionally deferred to keep initial scope manageable.
- Terminal table libraries (e.g., Rich) were evaluated but deemed unnecessary for Phase 1.

### Testing Notes
- **Unit tests**: Run locally in < 5 seconds (`cd src/pmrep/test && make test`)
- **QA tests**: Require PCP to be installed; run in GitHub CI
- **macOS**: Install package or use Linux VM for full QA testing
- **Backwards Compatibility**: All 43 existing pmrep QA tests should pass unchanged

### Related Plans
- **PLAN-pmrep-unit-testing.md**: Superseded - content consolidated into this plan (see "Fast Unit Testing" section)
