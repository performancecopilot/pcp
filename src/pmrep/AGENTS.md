# pmrep Configuration Development

Guide for developing pmrep configuration files in `conf/`.

## Critical: ConfigParser Percent Sign Escaping

**pmrep config files are parsed by Python's ConfigParser, which treats `%` as a special interpolation character.**

### The Rule

Always escape percent signs in metric labels and values by doubling them: `%` → `%%`

### Examples

**Wrong (causes InterpolationSyntaxError):**
```ini
gpu.util = util%,,,,6
cpu_usage.label = cpu%
```

**Correct:**
```ini
gpu.util = util%%,,,,6
cpu_usage.label = cpu%%
```

### Common Error

```
configparser.InterpolationSyntaxError: '%' must be followed by '%' or '(', found: '%,,,,6'
```

**Fix:** Double all `%` characters that should appear literally in output.

## Unit Specifications

**PCP's unit conversion system only recognizes standard units. Custom units cause `PM_ERR_CONV` errors.**

### Supported Units

- **Storage**: KB, MB, GB, TB, PB
- **Time**: s, ms, us, ns
- **Count**: count, none (dimensionless)

### Unsupported Units

If the darwin PMDA defines a metric as dimensionless (`PMDA_PMUNITS(0,0,0,0,0,0)`), omit the unit specification in pmrep config:

**Wrong (causes PM_ERR_CONV):**
```ini
power.battery.temperature = temp,,dC,,5
power.battery.capacity.design = design,,mAh,,7
```

**Correct:**
```ini
power.battery.temperature = temp,,,,5      # Raw Celsius value
power.battery.capacity.design = design,,,,7  # Raw mAh value
```

Use comments and labels to indicate units - don't try to convert them.

## Configuration File Format

pmrep configs use INI format with specific semantics:

### Basic Metric Definition

```ini
[view-name]
header = yes
unitinfo = no
globals = no
timestamp = no
precision = 0
delimiter = " "
repeat_header = auto

# Direct metric mapping
metric.name = label,,unit,,width
```

### Derived Metrics with Formulas

```ini
# Derived metric (computed from other metrics)
derived_name = base.metric.name
derived_name.label = label
derived_name.formula = 100 * rate(metric.a) / hinv.ncpu
derived_name.unit = s
derived_name.width = 5
```

**Note:** If your formula includes division, ensure denominators can't be zero, or accept that pmrep may show "N/A" for those samples.

## Testing pmrep Configurations

### Syntax Validation

```bash
# Test that a view loads without errors
pmrep -t 1 -s 1 :view-name

# Test with actual metrics (requires pmcd running)
pmrep -t 1 -s 3 :view-name
```

### Integration Testing

pmrep configs are installed to `$(PCP_SYSCONF_DIR)/pmrep` during `make install`. Test changes in the installed location, not the source tree.

On macOS (Darwin PMDA development):
1. Commit changes to git (VM clones the repo)
2. Run `/macos-qa-test` skill to test in isolated VM with pmcd running

## Common Patterns

### Percentage Calculations

```ini
# CPU utilization percentage
usr = kernel.all.cpu.usrp
usr.label = us
usr.formula = 100 * (kernel.all.cpu.user + kernel.all.cpu.nice) / hinv.ncpu
usr.unit = s
usr.width = 3
```

### Aggregation Across Instances

```ini
# Sum across all network interfaces
net_total = network.total.bytes
net_total.label = total
net_total.formula = sum(network.interface.in.bytes)
net_total.unit = KB
net_total.width = 8
```

### Rate Calculations

```ini
# IOPS from counter metrics
iops = disk.rate
iops.formula = rate(disk.dev.total)
iops.width = 7
```

## File Organization

- `conf/*.conf` - View configurations organized by theme
- `conf/GNUmakefile` - Installs all `.conf` files to system location
- Pattern: `$(shell echo *.conf)` picks up new files automatically

## Platform-Specific Views

Views can reference platform-specific metrics. If a metric doesn't exist on a platform, pmrep will show "N/A" for that field rather than failing the entire view.

Example: `macstat.conf` uses Darwin-specific metrics like `mem.util.compressed` which don't exist on Linux.

## References

- pmrep(1) man page - Command-line options and behavior
- Python ConfigParser docs - Understanding interpolation syntax
- `conf/pmstat.conf` - Simple, well-commented example
- `conf/collectl.conf` - Complex example with many derived metrics
