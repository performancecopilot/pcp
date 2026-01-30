/*
 * GPU IOKit enumeration for Darwin PMDA
 *
 * Copyright (c) 2026 Paul Smith.
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

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <string.h>

#include "pmapi.h"
#include "pmda.h"

#include "darwin.h"
#include "gpu.h"

/*
 * Enumerate GPUs using IOKit IOAccelerator interface
 * Returns: 0 on success, PM_ERR_AGAIN on failure
 *
 * If stats->gpus is NULL, this function only counts GPUs and sets stats->count.
 * If stats->gpus is allocated, this function updates GPU statistics.
 */
int
gpu_iokit_enumerate(struct gpustats *stats)
{
    io_iterator_t iterator = 0;
    io_service_t service = 0;
    CFMutableDictionaryRef properties = NULL;
    CFDictionaryRef perf_stats = NULL;
    CFNumberRef number = NULL;
    int gpu_index = 0;
    int ret = PM_ERR_AGAIN;

    if (!stats)
        return PM_ERR_AGAIN;

    /* Get IOAccelerator services (works for both Apple Silicon and Intel GPUs) */
    kern_return_t kr = IOServiceGetMatchingServices(
        kIOMainPortDefault,
        IOServiceMatching("IOAccelerator"),
        &iterator
    );

    if (kr != KERN_SUCCESS || !iterator) {
        if (iterator)
            IOObjectRelease(iterator);
        stats->count = 0;
        return PM_ERR_AGAIN;
    }

    /* Iterate through all GPUs */
    while ((service = IOIteratorNext(iterator))) {
        /* If we're just counting, increment and continue */
        if (!stats->gpus) {
            gpu_index++;
            IOObjectRelease(service);
            continue;
        }

        /* If we're updating stats, ensure we don't overflow */
        if (gpu_index >= stats->count) {
            IOObjectRelease(service);
            continue;
        }

        /* Get service properties */
        kr = IORegistryEntryCreateCFProperties(
            service,
            &properties,
            kCFAllocatorDefault,
            0
        );

        if (kr == KERN_SUCCESS && properties) {
            /* Extract PerformanceStatistics dictionary */
            perf_stats = CFDictionaryGetValue(properties, CFSTR("PerformanceStatistics"));

            if (perf_stats && CFGetTypeID(perf_stats) == CFDictionaryGetTypeID()) {
                /* Try "Device Utilization %" first (primary key) */
                number = CFDictionaryGetValue(perf_stats, CFSTR("Device Utilization %"));
                if (!number) {
                    /* Fallback to "GPU Activity(%)" for older systems */
                    number = CFDictionaryGetValue(perf_stats, CFSTR("GPU Activity(%)"));
                }

                if (number && CFGetTypeID(number) == CFNumberGetTypeID()) {
                    int util = 0;
                    CFNumberGetValue(number, kCFNumberIntType, &util);
                    stats->gpus[gpu_index].utilization = util;
                }

                /* Extract memory statistics if available */
                number = CFDictionaryGetValue(perf_stats, CFSTR("vramUsedBytes"));
                if (number && CFGetTypeID(number) == CFNumberGetTypeID()) {
                    uint64_t mem_used = 0;
                    CFNumberGetValue(number, kCFNumberSInt64Type, &mem_used);
                    stats->gpus[gpu_index].memory_used = mem_used;
                }

                /* Extract total VRAM (may be in different locations depending on GPU type) */
                number = CFDictionaryGetValue(properties, CFSTR("VRAM,totalMB"));
                if (number && CFGetTypeID(number) == CFNumberGetTypeID()) {
                    uint64_t vram_mb = 0;
                    CFNumberGetValue(number, kCFNumberSInt64Type, &vram_mb);
                    stats->gpus[gpu_index].memory_total = vram_mb * 1024 * 1024;
                }

                ret = 0; /* At least one GPU successfully enumerated */
            }

            CFRelease(properties);
        }

        IOObjectRelease(service);
        gpu_index++;
    }

    IOObjectRelease(iterator);

    /* Update count if we were just counting */
    if (!stats->gpus) {
        stats->count = gpu_index;
        return (gpu_index > 0) ? 0 : PM_ERR_AGAIN;
    }

    return ret;
}
