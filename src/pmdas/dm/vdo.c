/*
 * Device Mapper PMDA - VDO (Virtual Data Optimizer) Stats
 *
 * Copyright (c) 2018-2019,2026 Red Hat.
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

#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "vdo.h"
#include "indom.h"

#ifdef HAVE_DM_VDO_STATS
#include <libdevmapper.h>
#endif

static char *dm_vdo_statspath;

#ifdef HAVE_DM_VDO_STATS
/*
 * Two collection backends are supported:
 *   VDO_BACKEND_SYSFS - legacy out-of-tree kvdo /sys/kvdo statistics tree
 *   VDO_BACKEND_DMMSG - in-tree dm-vdo (kernel >= 6.9) device-mapper "stats"
 *                       message, parsed with libdm dm_vdo_stats_parse()
 * The message backend is preferred when built against libdm >= 1.02.214, but
 * downgrades to sysfs at the first refresh if no vdo target is present and an
 * old /sys/kvdo tree exists (i.e. a new-libdm binary on an old kvdo kernel).
 */
static enum { VDO_BACKEND_SYSFS, VDO_BACKEND_DMMSG } vdo_backend = VDO_BACKEND_SYSFS;

/*
 * QA test hook (message backend): if DM_VDO_STATS_RESPONSE names a file, its
 * contents are parsed as a captured "stats" message response instead of issuing
 * a live device-mapper message, and a single synthetic instance (named by
 * DM_VDO_STATS_DEVICE, default "vdo0") is enumerated.  Mirrors the sysfs
 * DM_VDO_STATSPATH override; note it still requires libdm >= 1.02.214 linked in
 * for dm_vdo_stats_parse().
 */
static char *dm_vdo_response_path;
static char *dm_vdo_response_device;
#endif

/*
 * ------------------------------------------------------------------------
 * sysfs backend (legacy /sys/kvdo) - unchanged behaviour
 * ------------------------------------------------------------------------
 */

static char *
vdo_fetch_oneline(const char *path, const char *name)
{
    static char buffer[MAXPATHLEN];
    FILE *fp;

    pmsprintf(buffer, sizeof(buffer), "%s/%s/statistics/%s",
		   dm_vdo_statspath, name, path);

    if ((fp = fopen(buffer, "r")) != NULL) {
	int i = fscanf(fp, "%63s", buffer);
	fclose(fp);
	if (i == 1)
	    return buffer;
    }
    return NULL;
}

static int
vdo_fetch_string(const char *file, const char *name, char **v)
{
    if ((*v = vdo_fetch_oneline(file, name)) == NULL)
	return PM_ERR_APPVERSION;
    return PMDA_FETCH_STATIC;
}

static int
vdo_fetch_ull(const char *file, const char *name, __uint64_t *v)
{
    char *value = vdo_fetch_oneline(file, name);
    char *endnum = NULL;

    if (!value)
	return PM_ERR_APPVERSION;
    *v = strtoull(value, &endnum, 10);
    if (!endnum || *endnum != '\0')
	return PM_ERR_VALUE;
    return PMDA_FETCH_STATIC;
}

static int
vdo_fetch_ul(const char *file, const char *name, __uint32_t *v)
{
    char *value = vdo_fetch_oneline(file, name);
    char *endnum = NULL;

    if (!value)
	return PM_ERR_APPVERSION;
    *v = strtoul(value, &endnum, 10);
    if (!endnum || *endnum != '\0')
	return PM_ERR_VALUE;
    return PMDA_FETCH_STATIC;
}

static int
vdo_fetch_float(const char *file, const char *name, float *v)
{
    char *value = vdo_fetch_oneline(file, name);
    char *endnum = NULL;

    if (!value)
	return PM_ERR_APPVERSION;
    *v = strtof(value, &endnum);
    if (!endnum || *endnum != '\0')
	return PM_ERR_VALUE;
    return PMDA_FETCH_STATIC;
}

/*
 * Fetches the value for the given metric instance and assigns to pmAtomValue.
 */
static int
vdo_sysfs_fetch(pmdaMetric *metric, const char *name, pmAtomValue *atom)
{
    char *file = (char *)metric->m_user;
    int sts;

    if (file) {
	unsigned int	type = metric->m_desc.type;

	switch (type) {
	case PM_TYPE_STRING:
	    return vdo_fetch_string(file, name, &atom->cp);
	case PM_TYPE_FLOAT:
	    return vdo_fetch_float(file, name, &atom->f);
	case PM_TYPE_U64:
	    return vdo_fetch_ull(file, name, &atom->ull);
	case PM_TYPE_U32:
	    return vdo_fetch_ul(file, name, &atom->ul);
	default:
	    break;
	}
	if (pmDebugOptions.libpmda)
	    fprintf(stderr, "Bad VDO type=%u f=%s dev=%s\n", type, file, name);
    } else {	/* derived metrics */
	__uint64_t v1, v2, v3;
	double calc;

	switch (pmID_item(metric->m_desc.pmid)) {
	case VDODEV_JOURNAL_BLOCKS_BATCHING:
	    if ((sts = vdo_fetch_ull("journal_blocks_started", name, &v1)) < 0)
		return sts;
	    if ((sts = vdo_fetch_ull("journal_blocks_written", name, &v2)) < 0)
		return sts;
	    atom->ull = v1 - v2;
	    return sts;
	case VDODEV_JOURNAL_BLOCKS_WRITING:
	    if ((sts = vdo_fetch_ull("journal_blocks_written", name, &v1)) < 0)
		return sts;
	    if ((sts = vdo_fetch_ull("journal_blocks_committed", name, &v2)) < 0)
		return sts;
	    atom->ull = v1 - v2;
	    return sts;
	case VDODEV_JOURNAL_ENTRIES_BATCHING:
	    if ((sts = vdo_fetch_ull("journal_entries_started", name, &v1)) < 0)
		return sts;
	    if ((sts = vdo_fetch_ull("journal_entries_written", name, &v2)) < 0)
		return sts;
	    atom->ull = v1 - v2;
	    return PMDA_FETCH_STATIC;
	case VDODEV_JOURNAL_ENTRIES_WRITING:
	    if ((sts = vdo_fetch_ull("journal_entries_written", name, &v1)) < 0)
		return sts;
	    if ((sts = vdo_fetch_ull("journal_entries_committed", name, &v2)) < 0)
		return sts;
	    atom->ull = v1 - v2;
	    return sts;
	case VDODEV_CAPACITY:
	    if ((sts = vdo_fetch_ull("physical_blocks", name, &v1)) < 0)
		return sts;
	    if ((sts = vdo_fetch_ull("block_size", name, &v2)) < 0)
		return sts;
	    atom->ull = v1 * v2 / 1024;
	    return sts;
	case VDODEV_USED:
	    if ((sts = vdo_fetch_ull("data_blocks_used", name, &v1)) < 0)
		return sts;
	    if ((sts = vdo_fetch_ull("overhead_blocks_used", name, &v2)) < 0)
		return sts;
	    if ((sts = vdo_fetch_ull("block_size", name, &v3)) < 0)
		return sts;
	    atom->ull = (v1 + v2) * v3 / 1024;
	    return sts;
	case VDODEV_AVAILABLE:
	    if ((sts = vdo_fetch_ull("physical_blocks", name, &v1)) < 0)
		return sts;
	    if ((sts = vdo_fetch_ull("data_blocks_used", name, &v2)) < 0)
		return sts;
	    if ((sts = vdo_fetch_ull("overhead_blocks_used", name, &v3)) < 0)
		return sts;
	    v1 = v1 - v2 - v3;
	    if ((sts = vdo_fetch_ull("block_size", name, &v2)) < 0)
		return sts;
	    atom->ull = v1 * v2 / 1024;
	    return sts;
	case VDODEV_USED_PERCENTAGE:
	    if ((sts = vdo_fetch_ull("physical_blocks", name, &v1)) < 0)
		return sts;
	    if ((sts = vdo_fetch_ull("data_blocks_used", name, &v2)) < 0)
		return sts;
	    if ((sts = vdo_fetch_ull("overhead_blocks_used", name, &v3)) < 0)
		return sts;
	    calc = (double)(v2 + v3);
	    if (v1 != 0)
		atom->f = 100.0 * (calc / (double)v1);
	    else
		atom->f = 0.0;
	    return sts;
	case VDODEV_SAVINGS_PERCENTAGE:
	    if ((sts = vdo_fetch_ull("logical_blocks_used", name, &v1)) < 0)
		return sts;
	    if ((sts = vdo_fetch_ull("data_blocks_used", name, &v2)) < 0)
		return sts;
	    calc = (double)(v1 - v2);
	    if (v1 != 0)
		atom->f = 100.0 * (calc / (double)v1);
	    else
		atom->f = 0.0;
	    return sts;
	default:
	    break;
	}
	if (pmDebugOptions.libpmda)
	    fprintf(stderr, "Bad metric item=%u dev=%s\n",
			    pmID_item(metric->m_desc.pmid), name);
    }
    return PMDA_FETCH_NOVALUES;
}

/*
 * Check whether 'name' in 'vdosysdir' is a VDO volume.
 */
static int
dm_vdodev_isvdovolumedir(const char *dir, const char *name)
{
    static char buffer[MAXPATHLEN];

    pmsprintf(buffer, sizeof(buffer), "%s/%s/statistics", dir, name);
    /* This file should exist if this is a VDO volume */
    return (access(buffer, F_OK) != -1);
}

/*
 * Update the VDO device instance domain. This will change as
 * VDO volumes are created, activated and removed.
 *
 * Using the pmdaCache interfaces simplifies things and provides
 * guarantees needed around consistent instance numbering in all
 * of those interesting corner cases.
 */
static int
vdo_sysfs_instance_refresh(void)
{
    DIR *sysdir;
    char *sysdev;
    struct dirent *sysentry;
    pmInDom indom = dm_indom(DM_VDODEV_INDOM);

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    if ((sysdir = opendir(dm_vdo_statspath)) == NULL)
	return -oserror();

    while ((sysentry = readdir(sysdir)) != NULL) {
	sysdev = sysentry->d_name;
	if (sysdev[0] == '.')
	    continue;
	if (!dm_vdodev_isvdovolumedir(dm_vdo_statspath, sysdev))
	    continue;
	if (pmDebugOptions.libpmda)
	    fprintf(stderr, "vdo_sysfs_instance_refresh: added %s", sysdev);
	pmdaCacheStore(indom, PMDA_CACHE_ADD, sysdev, NULL);
    }
    closedir(sysdir);
    return 0;
}

/*
 * ------------------------------------------------------------------------
 * device-mapper "stats" message backend (in-tree dm-vdo, libdm >= 1.02.214)
 * ------------------------------------------------------------------------
 */

#ifdef HAVE_DM_VDO_STATS

/* Per-device private data cached against the instance domain entry. */
struct vdo_inst {
    struct dm_vdo_stats_full	*full;	/* last parsed stats (dm_vdo_stats_parse) */
};

/*
 * Translate a PCP sysfs-style metric filename (e.g. "block_size") into the
 * device-mapper stats message label (e.g. "block size").  Most labels are a
 * plain '_' -> ' ' substitution; a handful of camelCase fields need an
 * explicit override.  We deliberately do NOT apply libdm's vdostats display
 * "fixups" - those are legacy presentation only; we key off the raw labels.
 */
static void
vdo_label_for_file(const char *file, char *out, size_t outlen)
{
    static const struct {
	const char	*file;
	const char	*label;
    } overrides[] = {
	{ "currentVIOs_in_progress",       "current VIOs in progress" },
	{ "maxVIOs",                       "max VIOs" },
	{ "errors_invalid_advicePBNCount", "errors invalid advice PBN count" },
    };
    size_t i;

    for (i = 0; i < sizeof(overrides) / sizeof(overrides[0]); i++) {
	if (strcmp(file, overrides[i].file) == 0) {
	    pmsprintf(out, outlen, "%s", overrides[i].label);
	    return;
	}
    }
    for (i = 0; file[i] != '\0' && i < outlen - 1; i++)
	out[i] = (file[i] == '_') ? ' ' : file[i];
    out[i] = '\0';
}

/* Linear scan for a parsed label; returns the value string or NULL. */
static const char *
vdo_dmmsg_field(const struct dm_vdo_stats_full *full, const char *label)
{
    unsigned int i;

    for (i = 0; i < full->field_count; i++) {
	if (strcmp(full->fields[i].label, label) == 0)
	    return full->fields[i].value;
    }
    return NULL;
}

/* Fetch a parsed label as an unsigned 64-bit value; -1 if absent/invalid. */
static int
vdo_dmmsg_field_u64(const struct dm_vdo_stats_full *full, const char *label,
		__uint64_t *v)
{
    const char *value = vdo_dmmsg_field(full, label);
    char *endnum = NULL;

    if (value == NULL)
	return -1;
    *v = strtoull(value, &endnum, 10);
    if (!endnum || *endnum != '\0')
	return -1;
    return 0;
}

/* Convert a parsed label value string to a pmAtomValue by metric type. */
static int
vdo_dmmsg_convert(const char *value, unsigned int type, pmAtomValue *atom)
{
    char *endnum = NULL;

    switch (type) {
    case PM_TYPE_STRING:
	/* points into the cached parse, valid until the next refresh */
	atom->cp = (char *)value;
	return PMDA_FETCH_STATIC;
    case PM_TYPE_FLOAT:
	atom->f = strtof(value, &endnum);
	break;
    case PM_TYPE_U64:
	atom->ull = strtoull(value, &endnum, 10);
	break;
    case PM_TYPE_U32:
	atom->ul = strtoul(value, &endnum, 10);
	break;
    default:
	return PMDA_FETCH_NOVALUES;
    }
    if (!endnum || *endnum != '\0')
	return PM_ERR_VALUE;
    return PMDA_FETCH_STATIC;
}

/*
 * Derived metrics computed from the typed dm_vdo_stats struct, carrying the
 * LVM2 (dmvdostats) guards: propagate "no value" for capacity accounting when
 * the volume is not operating normally, and avoid u64 underflow / negative
 * percentages.  Sizes are reported in KiB to match the sysfs backend.
 */
static int
vdo_dmmsg_derived(pmdaMetric *metric, const struct vdo_inst *vi, pmAtomValue *atom)
{
    const struct dm_vdo_stats *s = vi->full->stats;
    __uint64_t bpb = s->bytes_per_physical_block;
    __uint64_t data_overhead = s->data_blocks_used + s->overhead_blocks_used;
    __uint64_t v1, v2;

    switch (pmID_item(metric->m_desc.pmid)) {
    case VDODEV_JOURNAL_BLOCKS_BATCHING:
	if (vdo_dmmsg_field_u64(vi->full, "journal blocks started", &v1) < 0 ||
	    vdo_dmmsg_field_u64(vi->full, "journal blocks written", &v2) < 0)
	    return PMDA_FETCH_NOVALUES;
	atom->ull = (v1 >= v2) ? v1 - v2 : 0;
	return PMDA_FETCH_STATIC;
    case VDODEV_JOURNAL_BLOCKS_WRITING:
	if (vdo_dmmsg_field_u64(vi->full, "journal blocks written", &v1) < 0 ||
	    vdo_dmmsg_field_u64(vi->full, "journal blocks committed", &v2) < 0)
	    return PMDA_FETCH_NOVALUES;
	atom->ull = (v1 >= v2) ? v1 - v2 : 0;
	return PMDA_FETCH_STATIC;
    case VDODEV_JOURNAL_ENTRIES_BATCHING:
	if (vdo_dmmsg_field_u64(vi->full, "journal entries started", &v1) < 0 ||
	    vdo_dmmsg_field_u64(vi->full, "journal entries written", &v2) < 0)
	    return PMDA_FETCH_NOVALUES;
	atom->ull = (v1 >= v2) ? v1 - v2 : 0;
	return PMDA_FETCH_STATIC;
    case VDODEV_JOURNAL_ENTRIES_WRITING:
	if (vdo_dmmsg_field_u64(vi->full, "journal entries written", &v1) < 0 ||
	    vdo_dmmsg_field_u64(vi->full, "journal entries committed", &v2) < 0)
	    return PMDA_FETCH_NOVALUES;
	atom->ull = (v1 >= v2) ? v1 - v2 : 0;
	return PMDA_FETCH_STATIC;
    case VDODEV_CAPACITY:
	atom->ull = s->physical_blocks * bpb / 1024;
	return PMDA_FETCH_STATIC;
    case VDODEV_USED:
	atom->ull = data_overhead * bpb / 1024;
	return PMDA_FETCH_STATIC;
    case VDODEV_AVAILABLE:
	if (s->operating_mode != DM_VDO_MODE_NORMAL ||
	    data_overhead > s->physical_blocks)
	    return PMDA_FETCH_NOVALUES;	/* N/A - would underflow */
	atom->ull = (s->physical_blocks - data_overhead) * bpb / 1024;
	return PMDA_FETCH_STATIC;
    case VDODEV_USED_PERCENTAGE:
	if (s->operating_mode != DM_VDO_MODE_NORMAL || s->physical_blocks == 0)
	    return PMDA_FETCH_NOVALUES;	/* N/A */
	atom->f = 100.0 * ((double)data_overhead / (double)s->physical_blocks);
	return PMDA_FETCH_STATIC;
    case VDODEV_SAVINGS_PERCENTAGE:
	v1 = s->logical_blocks_used;
	v2 = s->data_blocks_used;
	if (v1 == 0 || v2 > v1)
	    return PMDA_FETCH_NOVALUES;	/* N/A - avoid negative saving */
	atom->f = 100.0 * ((double)(v1 - v2) / (double)v1);
	return PMDA_FETCH_STATIC;
    case VDODEV_WRITE_AMPLIFICATION:
	atom->f = (s->bios_in > 0) ?
	    (double)(s->bios_out + s->bios_meta) / (double)s->bios_in : 0.0;
	return PMDA_FETCH_STATIC;
    case VDODEV_EMULATION_512:
	atom->ul = (s->bytes_per_logical_block == 512) ? 1 : 0;
	return PMDA_FETCH_STATIC;
    default:
	break;
    }
    if (pmDebugOptions.libpmda)
	fprintf(stderr, "Bad VDO derived item=%u\n",
			pmID_item(metric->m_desc.pmid));
    return PMDA_FETCH_NOVALUES;
}

static int
vdo_dmmsg_fetch(pmdaMetric *metric, const char *name, pmAtomValue *atom)
{
    pmInDom indom = dm_indom(DM_VDODEV_INDOM);
    struct vdo_inst *vi;
    char *file = (char *)metric->m_user;
    char label[128];
    const char *value;
    int sts;

    sts = pmdaCacheLookupName(indom, name, NULL, (void **)&vi);
    if (sts < 0 || vi == NULL || vi->full == NULL)
	return PMDA_FETCH_NOVALUES;

    if (file == NULL)
	return vdo_dmmsg_derived(metric, vi, atom);

    vdo_label_for_file(file, label, sizeof(label));
    if ((value = vdo_dmmsg_field(vi->full, label)) == NULL)
	return PM_ERR_APPVERSION;	/* absent on this kernel (e.g. legacy) */
    return vdo_dmmsg_convert(value, metric->m_desc.type, atom);
}

/*
 * Parse a captured "stats" message response read from a file (QA test hook).
 * Caller owns the result (dm_free).
 */
static struct dm_vdo_stats_full *
vdo_dmmsg_capture_from_file(const char *path)
{
    struct dm_vdo_stats_full *full = NULL;
    char *response;
    long size;
    FILE *fp;

    if ((fp = fopen(path, "r")) == NULL)
	return NULL;
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0) {
	fclose(fp);
	return NULL;
    }
    rewind(fp);
    if ((response = malloc(size + 1)) != NULL) {
	size_t got = fread(response, 1, size, fp);
	response[got] = '\0';
	full = dm_vdo_stats_parse(NULL, response, DM_VDO_STATS_FULL);
	free(response);
    }
    fclose(fp);
    return full;
}

/*
 * Issue the "stats" device-mapper message to a VDO target device and parse
 * the response into a dm_vdo_stats_full.  Caller owns the result (dm_free).
 */
static struct dm_vdo_stats_full *
vdo_dmmsg_capture(const char *name)
{
    struct dm_task *dmt;
    const char *response;
    struct dm_vdo_stats_full *full = NULL;

    if (dm_vdo_response_path != NULL)	/* QA fixture, no live device */
	return vdo_dmmsg_capture_from_file(dm_vdo_response_path);

    if ((dmt = dm_task_create(DM_DEVICE_TARGET_MSG)) == NULL)
	return NULL;
    if (!dm_task_set_name(dmt, name))
	goto out;
    if (!dm_task_set_sector(dmt, 0))
	goto out;
    if (!dm_task_set_message(dmt, "stats"))
	goto out;
    if (!dm_task_run(dmt))
	goto out;
    if ((response = dm_task_get_message_response(dmt)) != NULL)
	full = dm_vdo_stats_parse(NULL, response, DM_VDO_STATS_FULL);
out:
    dm_task_destroy(dmt);
    return full;
}

/* Return non-zero if the named DM device has a "vdo" target. */
static int
vdo_dmmsg_is_vdo(const char *name)
{
    struct dm_task *dmt;
    uint64_t start, length;
    char *ttype = NULL, *params = NULL;
    void *next = NULL;
    int is_vdo = 0;

    if ((dmt = dm_task_create(DM_DEVICE_TABLE)) == NULL)
	return 0;
    if (!dm_task_set_name(dmt, name))
	goto out;
    if (!dm_task_run(dmt))
	goto out;
    do {
	next = dm_get_next_target(dmt, next, &start, &length, &ttype, &params);
	if (ttype != NULL && strcmp(ttype, "vdo") == 0) {
	    is_vdo = 1;
	    break;
	}
    } while (next != NULL);
out:
    dm_task_destroy(dmt);
    return is_vdo;
}

/* Free a cached VDO instance record (parsed stats blob and its container). */
static void
vdo_dmmsg_free_inst(struct vdo_inst *vi)
{
    if (vi == NULL)
	return;
    if (vi->full != NULL)
	dm_free(vi->full);
    free(vi);
}

/*
 * Release the parsed-stats captures cached against the instance domain.  Only
 * active entries are reachable via the cache walk, so this must run while the
 * previous cycle's devices are still active (before PMDA_CACHE_INACTIVE): that
 * way a device about to disappear does not strand its (potentially large)
 * parsed blob on an inactive entry, bounding retained memory to the live set
 * across device churn.
 *
 * With free_container set the whole per-device record is freed and detached,
 * used when switching away from the message backend so the sysfs refresh -
 * which stores NULL private data - does not orphan our allocations.  Otherwise
 * only the parsed blob is released and the small container is kept for reuse
 * when the device is next seen (matching the sibling dm* families).
 */
static void
vdo_dmmsg_release_captures(pmInDom indom, int free_container)
{
    struct vdo_inst *vi;
    char *name;
    int inst;

    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((inst = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (pmdaCacheLookup(indom, inst, &name, (void **)&vi) < 0 || vi == NULL)
	    continue;
	if (free_container) {
	    vdo_dmmsg_free_inst(vi);
	    /* detach the freed pointer so nothing can reference it later */
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, name, NULL);
	}
	else if (vi->full != NULL) {
	    dm_free(vi->full);
	    vi->full = NULL;
	}
    }
}

/*
 * Add (or refresh) one VDO device instance: capture + parse its stats and
 * cache the result against the indom entry.  Returns 1 if a usable capture was
 * parsed, 0 if the device was added but no stats were available, or a negative
 * error.
 */
static int
vdo_dmmsg_add_instance(pmInDom indom, const char *name)
{
    struct vdo_inst *vi = NULL;
    int sts;

    sts = pmdaCacheLookupName(indom, name, NULL, (void **)&vi);
    if (sts < 0 && sts != PM_ERR_INST)
	return sts;
    if (vi == NULL) {
	if ((vi = calloc(1, sizeof(*vi))) == NULL)
	    return -oserror();
    }
    if (vi->full != NULL) {		/* discard the previous capture */
	dm_free(vi->full);
	vi->full = NULL;
    }
    vi->full = vdo_dmmsg_capture(name);

    if (pmDebugOptions.libpmda)
	fprintf(stderr, "vdo_dmmsg_instance_refresh: added %s", name);
    pmdaCacheStore(indom, PMDA_CACHE_ADD, name, (void *)vi);
    return (vi->full != NULL) ? 1 : 0;
}

/*
 * Enumerate all device-mapper devices, keep those with a vdo target, and
 * capture + parse their stats once per refresh (cached against the indom).
 * Returns the number of VDO devices with usable stats, or a negative error.
 */
static int
vdo_dmmsg_instance_refresh(void)
{
    struct dm_task *dmt;
    struct dm_names *names;
    unsigned int next = 0;
    int count = 0, sts;
    pmInDom indom = dm_indom(DM_VDODEV_INDOM);

    /* free last cycle's parsed blobs before any device can go inactive */
    vdo_dmmsg_release_captures(indom, 0);

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    if (dm_vdo_response_path != NULL)	/* QA fixture: one synthetic device */
	return vdo_dmmsg_add_instance(indom, dm_vdo_response_device);

    /*
     * Mirror the access precheck the other device-mapper families use: without
     * read access to the control node we cannot drive the ioctls anyway, so do
     * not perform device-mapper enumeration or target-table inspection.  A zero
     * return lets the sysfs fallback (itself separately gated) have a turn.
     */
    if (access("/dev/mapper/control", R_OK) != 0)
	return 0;

    if ((dmt = dm_task_create(DM_DEVICE_LIST)) == NULL)
	return -oserror();
    if (!dm_task_enable_checks(dmt) || !dm_task_run(dmt) ||
	(names = dm_task_get_names(dmt)) == NULL) {
	dm_task_destroy(dmt);
	return -oserror();
    }
    if (names->dev == 0) {	/* empty list - no devices at all */
	dm_task_destroy(dmt);
	return 0;
    }

    do {
	names = (struct dm_names *)((char *)names + next);
	next = names->next;

	if (!vdo_dmmsg_is_vdo(names->name))
	    continue;

	if ((sts = vdo_dmmsg_add_instance(indom, names->name)) < 0) {
	    dm_task_destroy(dmt);
	    return sts;
	}
	count += sts;
    } while (next != 0);

    dm_task_destroy(dmt);
    return count;
}

#endif /* HAVE_DM_VDO_STATS */

/*
 * ------------------------------------------------------------------------
 * public entry points (backend dispatch)
 * ------------------------------------------------------------------------
 */

int
dm_vdodev_fetch(pmdaMetric *metric, const char *name, pmAtomValue *atom)
{
#ifdef HAVE_DM_VDO_STATS
    if (vdo_backend == VDO_BACKEND_DMMSG)
	return vdo_dmmsg_fetch(metric, name, atom);
#endif
    return vdo_sysfs_fetch(metric, name, atom);
}

int
dm_vdodev_instance_refresh(void)
{
#ifdef HAVE_DM_VDO_STATS
    if (vdo_backend == VDO_BACKEND_DMMSG) {
	int sts = vdo_dmmsg_instance_refresh();

	/*
	 * Runtime fallback: a binary built with new libdm may still run on
	 * an old kernel exposing only /sys/kvdo (out-of-tree kvdo).  If the
	 * message backend produced no usable stats but the sysfs tree exists,
	 * try it - but only commit to the sysfs backend permanently when it
	 * actually finds VDO volumes.  This way a transient message-backend
	 * failure (or a momentarily empty/unreadable /sys/kvdo) does not strand
	 * monitoring on a dead backend: the next refresh retries the message
	 * backend as usual.
	 */
	if (sts <= 0 && dm_vdo_response_path == NULL &&
	    access(dm_vdo_statspath, F_OK) == 0) {
	    pmInDom indom = dm_indom(DM_VDODEV_INDOM);
	    int fsts;

	    /* free message-backend records before the sysfs backend (which
	       stores NULL private data) takes over the same indom entries */
	    vdo_dmmsg_release_captures(indom, 1);

	    fsts = vdo_sysfs_instance_refresh();
	    if (fsts >= 0 && pmdaCacheOp(indom, PMDA_CACHE_SIZE_ACTIVE) > 0) {
		vdo_backend = VDO_BACKEND_SYSFS;
		return fsts;
	    }
	}
	return (sts < 0) ? sts : 0;
    }
#endif
    return vdo_sysfs_instance_refresh();
}

void
dm_vdo_setup(void)
{
    static char vdo_path[] = "/sys/kvdo";
    char *env_path;

    /* allow override at startup for QA testing (forces the sysfs backend) */
    if ((env_path = getenv("DM_VDO_STATSPATH")) != NULL)
	dm_vdo_statspath = env_path;
    else
	dm_vdo_statspath = vdo_path;

#ifdef HAVE_DM_VDO_STATS
    /*
     * A DM_VDO_STATSPATH sysfs fixture forces the sysfs backend; otherwise the
     * message backend is preferred.  A DM_VDO_STATS_RESPONSE fixture drives the
     * message backend from a captured response file (QA), naming its synthetic
     * device via DM_VDO_STATS_DEVICE (default "vdo0").
     */
    if (env_path != NULL) {
	vdo_backend = VDO_BACKEND_SYSFS;
    } else {
	vdo_backend = VDO_BACKEND_DMMSG;
	if ((dm_vdo_response_path = getenv("DM_VDO_STATS_RESPONSE")) != NULL) {
	    if ((dm_vdo_response_device = getenv("DM_VDO_STATS_DEVICE")) == NULL)
		dm_vdo_response_device = "vdo0";
	}
    }
#endif
}
