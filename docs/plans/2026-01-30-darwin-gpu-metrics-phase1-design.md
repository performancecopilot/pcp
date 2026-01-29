# Darwin PMDA GPU Metrics - Phase 1 Design

**Date:** 2026-01-30
**Author:** Claude Code + psmith
**Status:** Approved

## Overview

Add GPU monitoring capabilities to the Darwin PMDA, exposing GPU utilization and memory metrics for macOS systems (both Apple Silicon and Intel). This Phase 1 implementation provides foundational metrics with a per-GPU instance domain, following existing PCP patterns.

## Scope

### Phase 1 Metrics (This Design)
- `darwin.hinv.ngpu` - GPU count
- `darwin.gpu.util` - GPU utilization percentage (per-GPU instance)
- `darwin.gpu.memory.used` - VRAM used in bytes (per-GPU instance)
- `darwin.gpu.memory.free` - VRAM free in bytes (per-GPU instance)

### Phase 2 (Future)
- `darwin.hinv.gpu.model` - GPU model string
- `darwin.hinv.gpu.vram` - Total VRAM capacity
- `darwin.gpu.activity` - GPU activity percentage

## Architecture

### File Structure

**New Files:**
- `src/pmdas/darwin/gpu.h` - Data structures and function declarations
- `src/pmdas/darwin/gpu.c` - PCP/PMDA integration layer
- `src/pmdas/darwin/gpu_iokit.c` - IOKit/CoreFoundation GPU enumeration logic
- `build/mac/test/integration/test-gpu-metrics.sh` - Integration test

**Modified Files:**
- `src/pmdas/darwin/darwin.h` - Add GPU_INDOM and CLUSTER_GPU enums
- `src/pmdas/darwin/pmda.c` - Add init_gpu() and refresh_gpus() calls
- `src/pmdas/darwin/metrics.c` - Add GPU metric definitions
- `src/pmdas/darwin/pmns` - Add GPU namespace entries
- `src/pmdas/darwin/help` - Add metric help text
- `src/pmdas/darwin/GNUmakefile` - Add new source files to build
- `build/mac/test/integration/run-integration-tests.sh` - Add GPU test

### Data Flow

1. **Initialization** (`init_gpu()` in gpu.c):
   - One-time setup at PMDA startup
   - Call `gpu_iokit_enumerate()` to discover GPUs
   - Allocate fixed-size array based on GPU count
   - Setup GPU instance domain with "gpu0", "gpu1", etc.

2. **Refresh** (`refresh_gpus()` in gpu.c):
   - Called from `darwin_fetch()` in pmda.c before each metric fetch
   - Calls `gpu_iokit_enumerate()` to update current GPU statistics
   - Updates global `mach_gpu` structure with cached data
   - No dynamic reallocation (GPU count fixed at init)

3. **Fetch** (metric-specific helpers in gpu.c):
   - Return cached values from `mach_gpu` structure
   - Return `PM_ERR_AGAIN` if data unavailable (silent failure)

### Integration Points

**darwin.h additions:**
```c
enum {
    // ... existing indoms ...
    GPU_INDOM,              // 6 - set of all GPUs
    NUM_INDOMS
};

enum {
    // ... existing clusters ...
    CLUSTER_GPU = 19,       // gpu statistics
    NUM_CLUSTERS
};
```

**pmda.c additions:**
```c
#include "gpu.h"

int mach_gpu_error = 0;
struct gpustats mach_gpu = { 0 };

// In main():
init_gpu();

// In darwin_fetch():
refresh_gpus(&mach_gpu);
```

## Data Structures

### Core Structures (gpu.h)

```c
struct gpustat {
    char name[32];              // "gpu0", "gpu1", etc.
    int utilization;            // Device Utilization % (0-100)
    uint64_t memory_used;       // VRAM used in bytes
    uint64_t memory_total;      // Total VRAM in bytes
};

struct gpustats {
    int count;                  // Number of GPUs (fixed at init)
    struct gpustat *gpus;       // Allocated once at init, freed at shutdown
};
```

### IOKit Interface (gpu.h)

```c
// Initialize GPU monitoring (called once at startup)
int init_gpu(void);

// Enumerate GPUs and update statistics
// Returns: 0 on success, -errno on failure
int gpu_iokit_enumerate(struct gpustats *stats);

// Refresh GPU statistics (called before each fetch)
int refresh_gpus(struct gpustats *stats);
```

### Lifecycle

- **init_gpu()**: Enumerate GPUs, allocate `stats->gpus` array, setup instance domain
- **refresh_gpus()**: Update stats for fixed set of GPUs
- **Shutdown**: Free `stats->gpus` array

No dynamic resizing - GPU count is constant for PMDA lifetime.

## IOKit Implementation

### IOKit Pattern (gpu_iokit.c)

Based on community-documented IOAccelerator interface (reference: https://github.com/andersrennermalm/gpuinfo).

**Algorithm:**
1. Use `IOServiceGetMatchingServices()` with "IOAccelerator" class matcher
2. Iterate through matching services with `IOIteratorNext()`
3. For each service:
   - Get properties via `IORegistryEntryCreateCFProperties()`
   - Extract "PerformanceStatistics" CFDictionary
   - Read "Device Utilization %" (or "GPU Activity(%)" as fallback)
   - Extract memory statistics if available
   - Populate `gpustat` entry

4. Return 0 on success, -errno on failure

**Key IOKit APIs:**
- `IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOAccelerator"), &iterator)`
- `IORegistryEntryCreateCFProperties(service, &properties, kCFAllocatorDefault, 0)`
- `CFDictionaryGetValue(props, CFSTR("PerformanceStatistics"))`
- `CFNumberGetValue(number, kCFNumberIntType, &value)`

**Memory Management:**
- `IOObjectRelease()` on iterator and services
- `CFRelease()` on CoreFoundation dictionaries/numbers
- Follow create/copy rule: if function has "Create" or "Copy", caller must release

**Error Handling:**
- Return `PM_ERR_AGAIN` if no GPUs found or IOKit access fails
- No logging (silent failure pattern per disk.c)
- Graceful degradation on older hardware

### Platform Compatibility

- **Apple Silicon**: Uses AGXAccelerator via IOAccelerator class - fully supported
- **Intel Macs**: Uses IOAccelerator class - fully supported
- Both platforms use identical IOKit interface

## PCP Integration

### Metrics Definition

**Namespace (pmns):**
```
hinv {
    ngpu            DARWIN:4:XX    # GPU count (CLUSTER_HINV)
}

gpu {                              # New top-level namespace
    util            DARWIN:19:0    # Per-GPU (CLUSTER_GPU, GPU_INDOM)
    memory
}

gpu.memory {
    used            DARWIN:19:1    # Per-GPU
    free            DARWIN:19:2    # Per-GPU
}
```

**Metric Specifications:**

| Metric | Type | Indom | Semantics | Units | Description |
|--------|------|-------|-----------|-------|-------------|
| darwin.hinv.ngpu | PM_TYPE_U32 | PM_INDOM_NULL | PM_SEM_DISCRETE | count | Number of GPUs |
| darwin.gpu.util | PM_TYPE_U32 | GPU_INDOM | PM_SEM_INSTANT | percent | GPU utilization % |
| darwin.gpu.memory.used | PM_TYPE_U64 | GPU_INDOM | PM_SEM_INSTANT | byte | VRAM used |
| darwin.gpu.memory.free | PM_TYPE_U64 | GPU_INDOM | PM_SEM_INSTANT | byte | VRAM free |

**Cluster Assignment:**
- `hinv.ngpu` → CLUSTER_HINV (4) - existing hardware inventory cluster
- `gpu.*` → CLUSTER_GPU (19) - new GPU statistics cluster

**Instance Domain:**
- GPU_INDOM (index 6 in NUM_INDOMS)
- Instances: "gpu0", "gpu1", "gpu2", etc.
- Follows disk.c pattern (DISK_INDOM, disk0/disk1)

### Fetch Callbacks (metrics.c)

```c
// darwin.hinv.ngpu
atom->ul = mach_gpu.count;

// darwin.gpu.util (per-GPU instance)
atom->ul = mach_gpu.gpus[inst].utilization;

// darwin.gpu.memory.used (per-GPU instance)
atom->ull = mach_gpu.gpus[inst].memory_used;

// darwin.gpu.memory.free (per-GPU instance)
atom->ull = mach_gpu.gpus[inst].memory_total - mach_gpu.gpus[inst].memory_used;
```

## Testing Strategy (TDD)

### Test Location
`build/mac/test/integration/test-gpu-metrics.sh`

### Test Pattern
Following existing `test-pmstat.sh` pattern with colored output and check counters.

### Test Coverage

1. **GPU count validation:**
   - `pminfo -f darwin.hinv.ngpu` returns valid count (0-4 expected range)
   - Value is non-negative integer

2. **Metric existence:**
   - `pminfo darwin.gpu.util` succeeds
   - `pminfo darwin.gpu.memory.used` succeeds
   - `pminfo darwin.gpu.memory.free` succeeds

3. **Instance domain validation:**
   - `pminfo -f darwin.gpu.util` shows instances (gpu0, gpu1, etc.)
   - Instance count matches `hinv.ngpu`

4. **Value validation:**
   - Utilization: 0-100 range
   - Memory used: Non-negative integer
   - Memory free: Non-negative integer
   - Memory free ≤ memory used is not required (different contexts)

### Test Execution

**Manual run:**
```bash
cd build/mac/test/integration
./test-gpu-metrics.sh
```

**Automated via QA agent:**
```bash
# macos-darwin-pmda-qa agent handles:
# - VM provisioning
# - Build in isolation
# - Test execution
# - Results reporting
```

### Expected Output

```bash
Testing GPU metrics...
✓ GPU count metric present (darwin.hinv.ngpu = 1)
✓ GPU utilization metric exists
✓ GPU memory.used metric exists
✓ GPU memory.free metric exists
✓ Instance domain shows gpu0
✓ Utilization value in valid range (0-100)
✓ Memory values are non-negative

Checks passed: 7
Checks failed: 0
✓ GPU metrics validation passed
```

## Implementation Workflow (TDD)

### Step 1: Write Failing Test
- Create `build/mac/test/integration/test-gpu-metrics.sh`
- Test that metrics exist and return valid data
- Add test to `run-integration-tests.sh`
- Run test - expect failure (metrics don't exist yet)

### Step 2: Implement Minimal Code
1. Create `gpu.h` with data structures and declarations
2. Create `gpu_iokit.c` with IOKit enumeration logic
3. Create `gpu.c` with PCP integration (init, refresh, fetch helpers)
4. Update `darwin.h` (add GPU_INDOM = 6, CLUSTER_GPU = 19)
5. Update `pmda.c` (add `#include "gpu.h"`, call init_gpu() and refresh_gpus())
6. Update `metrics.c` (add 4 metric definitions with fetch callbacks)
7. Update `pmns` (add hinv.ngpu and gpu.* namespace)
8. Update `help` (add metric descriptions)
9. Update `GNUmakefile` (add gpu.c and gpu_iokit.c to CFILES)

### Step 3: Run Test
- Execute `macos-darwin-pmda-qa` agent
- Test should pass in isolated VM environment
- Verify metrics work on Apple Silicon

### Step 4: Manual Verification
```bash
pminfo -f darwin.hinv.ngpu
pminfo -f darwin.gpu.util
pminfo -f darwin.gpu.memory.used
pminfo -f darwin.gpu.memory.free
```

### Step 5: Commit
Commit message format:
```
darwin: add basic GPU monitoring metrics

Expose GPU utilization and memory usage via IOKit IOAccelerator.
Enables monitoring of GPU workloads on both Apple Silicon and Intel.
```

## File Change Summary

**New Files (4):**
- `src/pmdas/darwin/gpu.h`
- `src/pmdas/darwin/gpu.c`
- `src/pmdas/darwin/gpu_iokit.c`
- `build/mac/test/integration/test-gpu-metrics.sh`

**Modified Files (7):**
- `src/pmdas/darwin/darwin.h`
- `src/pmdas/darwin/pmda.c`
- `src/pmdas/darwin/metrics.c`
- `src/pmdas/darwin/pmns`
- `src/pmdas/darwin/help`
- `src/pmdas/darwin/GNUmakefile`
- `build/mac/test/integration/run-integration-tests.sh`

## Error Handling

### Silent Failure Pattern
Following disk.c pattern:
- If IOKit enumeration fails: return `PM_ERR_AGAIN`
- If GPU data unavailable: return `PM_ERR_AGAIN`
- No logging to avoid spam
- Tools like `pminfo` show metric exists but has no current value

### Graceful Degradation
- Older hardware without GPU: `hinv.ngpu = 0`, no instances in GPU_INDOM
- IOKit access denied: Metrics registered but return no values
- No GPUs found: Not an error, just zero count

## Future Work (Phase 2)

Phase 2 will add in a separate commit:
- `darwin.hinv.gpu.model` - GPU model string (per-GPU instance)
- `darwin.hinv.gpu.vram` - Total VRAM capacity in bytes (per-GPU instance)
- `darwin.gpu.activity` - GPU activity percentage (per-GPU instance)

These require additional IORegistry property extraction but follow the same pattern.

## References

- IOKit GPU Pattern: https://github.com/andersrennermalm/gpuinfo
- PCP NVIDIA PMDA: https://www.man7.org/linux/man-pages/man1/pmdanvidia.1.html
- Darwin PMDA Phase 2 Research: `darwin-pmda-phase2-research.md`
- Apple IOKit Documentation: https://developer.apple.com/documentation/iokit

## Design Decisions

### Why split gpu.c and gpu_iokit.c?
Separates IOKit/CoreFoundation complexity from PCP integration logic. Makes code more testable and maintainable.

### Why fixed GPU count at init?
GPUs don't hot-swap like disks. Simpler than dynamic reallocation, no performance overhead.

### Why PM_ERR_AGAIN vs logging?
Follows established Darwin PMDA pattern (disk.c). Silent failure avoids log spam when metrics polled frequently.

### Why per-GPU instance domain?
Handles multi-GPU setups (eGPUs) properly. Matches PCP conventions (disk.dev, network.interface). Future-proof.

### Why Phase 1 vs full implementation?
Smaller commits easier to review. Foundational metrics (util, memory) provide immediate value. Model/VRAM can follow.

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| IOKit API changes | Use community-documented patterns with broad macOS version support |
| Permissions issues | Silent failure with PM_ERR_AGAIN, document in help text |
| eGPU disconnect | Fixed count at init means restart needed; acceptable trade-off |
| Memory leaks | Follow CoreFoundation create/copy ownership rules strictly |
| Test flakiness | Run in isolated VM via macos-darwin-pmda-qa agent |

## Success Criteria

- ✅ Test-driven: Failing test written first, passes after implementation
- ✅ All 4 metrics return valid data on Apple Silicon Mac
- ✅ Instance domain correctly shows gpu0 (or multiple GPUs if present)
- ✅ Integration test passes in isolated VM environment
- ✅ Code follows existing Darwin PMDA patterns (disk.c, network.c)
- ✅ No compiler warnings, no memory leaks
- ✅ Help text documents all metrics clearly
- ✅ PMNS follows darwin.* namespace conventions
