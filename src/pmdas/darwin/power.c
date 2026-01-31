/*
 * Copyright (c) 2025 Performance Co-Pilot contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "pmapi.h"
#include "pmda.h"
#include "darwin.h"
#include "power.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/IOKitLib.h>
#include <string.h>

static int
get_cf_int(CFDictionaryRef dict, CFStringRef key, int default_value)
{
    CFNumberRef num;
    int value = default_value;

    if (!dict)
        return default_value;

    num = CFDictionaryGetValue(dict, key);
    if (num && CFGetTypeID(num) == CFNumberGetTypeID())
        CFNumberGetValue(num, kCFNumberIntType, &value);

    return value;
}

static int
get_cf_bool(CFDictionaryRef dict, CFStringRef key, int default_value)
{
    CFBooleanRef val;

    if (!dict)
        return default_value;

    val = CFDictionaryGetValue(dict, key);
    if (val && CFGetTypeID(val) == CFBooleanGetTypeID())
        return CFBooleanGetValue(val) ? 1 : 0;

    return default_value;
}

static void
get_cf_string(CFDictionaryRef dict, CFStringRef key, char *buf, size_t buflen)
{
    CFStringRef str;

    if (!dict || !buf || buflen == 0)
        return;

    buf[0] = '\0';
    str = CFDictionaryGetValue(dict, key);
    if (str && CFGetTypeID(str) == CFStringGetTypeID())
        CFStringGetCString(str, buf, buflen, kCFStringEncodingUTF8);
}

static int
get_battery_details_from_iokit(powerstats_t *stats)
{
    io_service_t service;
    CFMutableDictionaryRef properties = NULL;
    kern_return_t kr;
    int got_data = 0;

    /* Find the AppleSmartBattery service for detailed metrics */
    service = IOServiceGetMatchingService(kIOMainPortDefault,
                                          IOServiceMatching("AppleSmartBattery"));
    if (!service)
        return 0;

    kr = IORegistryEntryCreateCFProperties(service, &properties,
                                           kCFAllocatorDefault, 0);
    if (kr == KERN_SUCCESS && properties) {
        /* Cycle count */
        stats->cycle_count = get_cf_int(properties, CFSTR("CycleCount"), 0);

        /* Temperature (in 0.01 Kelvin, convert to 0.01 Celsius) */
        int temp_kelvin_hundredths = get_cf_int(properties, CFSTR("Temperature"), 0);
        if (temp_kelvin_hundredths > 0) {
            /* Convert from 0.01K to 0.01C: subtract 273.15 * 100 = 27315 */
            stats->temperature = temp_kelvin_hundredths - 27315;
        } else {
            stats->temperature = 0;
        }

        /* Voltage (already in mV) */
        stats->voltage_mv = get_cf_int(properties, CFSTR("Voltage"), 0);

        /* Amperage (already in mA, negative when discharging) */
        stats->amperage_ma = get_cf_int(properties, CFSTR("Amperage"), 0);

        /* Design capacity (in mAh) */
        stats->design_capacity_mah = get_cf_int(properties, CFSTR("DesignCapacity"), 0);

        /* Max capacity (in mAh) */
        stats->max_capacity_mah = get_cf_int(properties, CFSTR("MaxCapacity"), 0);

        got_data = 1;
        CFRelease(properties);
    }

    IOObjectRelease(service);
    return got_data;
}

int
refresh_power(powerstats_t *stats)
{
    CFTypeRef blob;
    CFArrayRef list;
    CFDictionaryRef ps;
    CFStringRef source_type;
    int i, count;

    if (!stats)
        return -1;

    /* Initialize to safe defaults */
    memset(stats, 0, sizeof(*stats));
    stats->time_remaining = -1;
    strcpy(stats->power_source, "Unknown");

    /* Get power source info */
    blob = IOPSCopyPowerSourcesInfo();
    if (!blob)
        return -1;

    /* Get current power source type */
    source_type = IOPSGetProvidingPowerSourceType(blob);
    if (source_type) {
        get_cf_string(NULL, source_type, stats->power_source,
                      sizeof(stats->power_source));
        stats->ac_connected = (CFStringCompare(source_type, CFSTR("AC Power"),
                                               0) == kCFCompareEqualTo) ? 1 : 0;
    }

    /* Get list of power sources */
    list = IOPSCopyPowerSourcesList(blob);
    if (!list) {
        CFRelease(blob);
        return 0;
    }

    count = CFArrayGetCount(list);

    /* Iterate through power sources looking for battery */
    for (i = 0; i < count; i++) {
        CFStringRef ps_type;

        ps = IOPSGetPowerSourceDescription(blob, CFArrayGetValueAtIndex(list, i));
        if (!ps)
            continue;

        /* Check if this is a battery */
        ps_type = CFDictionaryGetValue(ps, CFSTR(kIOPSTypeKey));
        if (!ps_type || CFStringCompare(ps_type, CFSTR(kIOPSInternalBatteryType),
                                        0) != kCFCompareEqualTo)
            continue;

        /* Found battery */
        stats->battery_present = get_cf_bool(ps, CFSTR(kIOPSIsPresentKey), 0);
        if (!stats->battery_present)
            continue;

        /* Charging status */
        stats->charging = get_cf_bool(ps, CFSTR(kIOPSIsChargingKey), 0);

        /* Current charge percentage */
        stats->charge_percent = get_cf_int(ps, CFSTR(kIOPSCurrentCapacityKey), 0);

        /* Time remaining (minutes) */
        if (stats->charging) {
            stats->time_remaining = get_cf_int(ps, CFSTR(kIOPSTimeToFullChargeKey), -1);
        } else {
            stats->time_remaining = get_cf_int(ps, CFSTR(kIOPSTimeToEmptyKey), -1);
        }

        /* Battery health (percentage) */
        int max_cap = get_cf_int(ps, CFSTR(kIOPSMaxCapacityKey), 100);
        int design_cap = get_cf_int(ps, CFSTR(kIOPSDesignCapacityKey), 100);
        if (design_cap > 0) {
            stats->health_percent = (max_cap * 100) / design_cap;
            if (stats->health_percent > 100)
                stats->health_percent = 100;
        } else {
            stats->health_percent = 100;
        }

        /* Get detailed metrics from IOKit */
        get_battery_details_from_iokit(stats);

        break;  /* Only process first battery */
    }

    CFRelease(list);
    CFRelease(blob);

    return 0;
}

int
fetch_power(unsigned int item, pmAtomValue *atom)
{
    extern powerstats_t mach_power;

    switch (item) {
    case 0:  /* power.battery.present */
        atom->ul = mach_power.battery_present;
        return PMDA_FETCH_STATIC;
    case 1:  /* power.battery.charging */
        atom->ul = mach_power.charging;
        return PMDA_FETCH_STATIC;
    case 2:  /* power.battery.charge */
        atom->ul = mach_power.charge_percent;
        return PMDA_FETCH_STATIC;
    case 3:  /* power.battery.time_remaining */
        atom->l = mach_power.time_remaining;
        return PMDA_FETCH_STATIC;
    case 4:  /* power.battery.health */
        atom->ul = mach_power.health_percent;
        return PMDA_FETCH_STATIC;
    case 5:  /* power.battery.cycle_count */
        atom->ul = mach_power.cycle_count;
        return PMDA_FETCH_STATIC;
    case 6:  /* power.battery.temperature */
        atom->ul = mach_power.temperature;
        return PMDA_FETCH_STATIC;
    case 7:  /* power.battery.voltage */
        atom->ul = mach_power.voltage_mv;
        return PMDA_FETCH_STATIC;
    case 8:  /* power.battery.amperage */
        atom->l = mach_power.amperage_ma;
        return PMDA_FETCH_STATIC;
    case 9:  /* power.battery.capacity.design */
        atom->ul = mach_power.design_capacity_mah;
        return PMDA_FETCH_STATIC;
    case 10: /* power.battery.capacity.max */
        atom->ul = mach_power.max_capacity_mah;
        return PMDA_FETCH_STATIC;
    case 11: /* power.ac.connected */
        atom->ul = mach_power.ac_connected;
        return PMDA_FETCH_STATIC;
    case 12: /* power.source */
        atom->cp = mach_power.power_source;
        return PMDA_FETCH_STATIC;
    default:
        return PM_ERR_PMID;
    }
}
