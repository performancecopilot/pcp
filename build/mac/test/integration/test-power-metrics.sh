#!/bin/bash
#
# Power metrics validation test
# Ensures darwin.power.* metrics work correctly on macOS
#
# Note: VMs typically have no battery (battery_present=0, ac_connected=1)
#       Real laptops will show battery metrics
#

set -u

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "Testing Power metrics..."

checks_passed=0
checks_failed=0

# Test 1: Battery present flag (0 or 1)
battery_present_output=$(pminfo -f power.battery.present 2>&1)
battery_present_exit=$?

if [ $battery_present_exit -eq 0 ]; then
    battery_present=$(echo "$battery_present_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ "$battery_present" = "0" ] || [ "$battery_present" = "1" ]; then
        if [ "$battery_present" -eq 0 ]; then
            echo -e "${GREEN}✓ Battery present metric valid (no battery in VM)${NC}"
        else
            echo -e "${GREEN}✓ Battery present metric valid (battery detected)${NC}"
        fi
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Battery present invalid: $battery_present (expected 0 or 1)${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ Battery present metric missing${NC}"
    echo "$battery_present_output"
    checks_failed=$((checks_failed + 1))
    battery_present=0  # Assume no battery for remaining tests
fi

# Test 2: AC connected flag (0 or 1)
ac_output=$(pminfo -f power.ac.connected 2>&1)
if [ $? -eq 0 ]; then
    ac_connected=$(echo "$ac_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ "$ac_connected" = "0" ] || [ "$ac_connected" = "1" ]; then
        echo -e "${GREEN}✓ AC connected metric valid: $ac_connected${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ AC connected invalid: $ac_connected${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ AC connected metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 3: Power source string
source_output=$(pminfo -f power.source 2>&1)
if [ $? -eq 0 ]; then
    # Extract string value - format is: value "AC Power" or similar
    source_value=$(echo "$source_output" | grep -E '^\s+value' | sed 's/.*value "\(.*\)".*/\1/')

    case "$source_value" in
        "AC Power"|"Battery Power"|"Unknown")
            echo -e "${GREEN}✓ Power source valid: \"$source_value\"${NC}"
            checks_passed=$((checks_passed + 1))
            ;;
        *)
            echo -e "${RED}✗ Power source unexpected: \"$source_value\"${NC}"
            checks_failed=$((checks_failed + 1))
            ;;
    esac
else
    echo -e "${RED}✗ Power source metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 4: Battery charge percentage (0-100)
charge_output=$(pminfo -f power.battery.charge 2>&1)
if [ $? -eq 0 ]; then
    charge=$(echo "$charge_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$charge" ] && [ "$charge" -ge 0 ] && [ "$charge" -le 100 ]; then
        if [ "$battery_present" -eq 0 ] && [ "$charge" -eq 0 ]; then
            echo -e "${GREEN}✓ Battery charge valid (0 for no battery)${NC}"
        else
            echo -e "${GREEN}✓ Battery charge valid: $charge%${NC}"
        fi
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Battery charge out of range: $charge${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ Battery charge metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 5: Battery charging flag (0 or 1)
charging_output=$(pminfo -f power.battery.charging 2>&1)
if [ $? -eq 0 ]; then
    charging=$(echo "$charging_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ "$charging" = "0" ] || [ "$charging" = "1" ]; then
        echo -e "${GREEN}✓ Battery charging metric valid: $charging${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Battery charging invalid: $charging${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ Battery charging metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 6: Time remaining (minutes, -1 if not available)
time_output=$(pminfo -f power.battery.time_remaining 2>&1)
if [ $? -eq 0 ]; then
    # Handle negative values (signed)
    time_remaining=$(echo "$time_output" | grep -E '^\s+value' | grep -Eo '\-?[0-9]+')

    if [ -n "$time_remaining" ] && [ "$time_remaining" -ge -1 ]; then
        if [ "$time_remaining" -eq -1 ]; then
            echo -e "${GREEN}✓ Time remaining valid (not available)${NC}"
        else
            echo -e "${GREEN}✓ Time remaining valid: $time_remaining minutes${NC}"
        fi
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Time remaining invalid: $time_remaining${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ Time remaining metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 7: Battery health (0-100%)
health_output=$(pminfo -f power.battery.health 2>&1)
if [ $? -eq 0 ]; then
    health=$(echo "$health_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$health" ] && [ "$health" -ge 0 ] && [ "$health" -le 100 ]; then
        echo -e "${GREEN}✓ Battery health valid: $health%${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Battery health out of range: $health${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ Battery health metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 8: Cycle count (non-negative integer)
cycle_output=$(pminfo -f power.battery.cycle_count 2>&1)
if [ $? -eq 0 ]; then
    cycles=$(echo "$cycle_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$cycles" ] && [ "$cycles" -ge 0 ]; then
        echo -e "${GREEN}✓ Battery cycle count valid: $cycles${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Battery cycle count invalid: $cycles${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ Battery cycle count metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 9: Temperature (°C×100, can be 0 if no battery)
temp_output=$(pminfo -f power.battery.temperature 2>&1)
if [ $? -eq 0 ]; then
    temp=$(echo "$temp_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$temp" ] && [ "$temp" -ge 0 ]; then
        if [ "$temp" -eq 0 ]; then
            echo -e "${GREEN}✓ Battery temperature valid (0 - no battery or no sensor)${NC}"
        else
            # Convert to human-readable (divide by 100)
            temp_c=$((temp / 100))
            echo -e "${GREEN}✓ Battery temperature valid: ${temp_c}°C${NC}"
        fi
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Battery temperature invalid: $temp${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ Battery temperature metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 10: Voltage (mV, non-negative)
voltage_output=$(pminfo -f power.battery.voltage 2>&1)
if [ $? -eq 0 ]; then
    voltage=$(echo "$voltage_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$voltage" ] && [ "$voltage" -ge 0 ]; then
        echo -e "${GREEN}✓ Battery voltage valid: $voltage mV${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Battery voltage invalid: $voltage${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ Battery voltage metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 11: Amperage (mA, signed - negative when discharging)
amperage_output=$(pminfo -f power.battery.amperage 2>&1)
if [ $? -eq 0 ]; then
    # Handle negative values
    amperage=$(echo "$amperage_output" | grep -E '^\s+value' | grep -Eo '\-?[0-9]+')

    if [ -n "$amperage" ]; then
        echo -e "${GREEN}✓ Battery amperage valid: $amperage mA${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Battery amperage invalid: $amperage${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ Battery amperage metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 12: Design capacity (mAh, non-negative)
design_cap_output=$(pminfo -f power.battery.capacity.design 2>&1)
if [ $? -eq 0 ]; then
    design_cap=$(echo "$design_cap_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$design_cap" ] && [ "$design_cap" -ge 0 ]; then
        echo -e "${GREEN}✓ Battery design capacity valid: $design_cap mAh${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Battery design capacity invalid: $design_cap${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ Battery design capacity metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Test 13: Max capacity (mAh, non-negative)
max_cap_output=$(pminfo -f power.battery.capacity.max 2>&1)
if [ $? -eq 0 ]; then
    max_cap=$(echo "$max_cap_output" | grep -E '^\s+value' | grep -Eo '[0-9]+')

    if [ -n "$max_cap" ] && [ "$max_cap" -ge 0 ]; then
        echo -e "${GREEN}✓ Battery max capacity valid: $max_cap mAh${NC}"
        checks_passed=$((checks_passed + 1))
    else
        echo -e "${RED}✗ Battery max capacity invalid: $max_cap${NC}"
        checks_failed=$((checks_failed + 1))
    fi
else
    echo -e "${RED}✗ Battery max capacity metric missing${NC}"
    checks_failed=$((checks_failed + 1))
fi

# Logical consistency check
if [ -n "${battery_present:-}" ] && [ "$battery_present" -eq 0 ]; then
    echo -e "${YELLOW}Note: No battery present - all battery metrics should be at safe defaults${NC}"
fi

echo
echo "Checks passed: $checks_passed"
echo "Checks failed: $checks_failed"

if [ $checks_failed -eq 0 ]; then
    echo -e "${GREEN}✓ Power metrics validation passed${NC}"
    exit 0
else
    echo -e "${RED}✗ Power metrics validation failed${NC}"
    exit 1
fi
