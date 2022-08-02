/*
 * Copyright (C) 2013,2016 Red Hat.
 * Copyright (C) 2009 Aconex.  All Rights Reserved.
 * Copyright (C) 2001 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>
#include <pcp/mmv_dev.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <strings.h>

int
dump_indoms(void *addr, size_t size, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    char buf[MMV_STRINGMAX];
    mmv_disk_string_t *string;
    mmv_disk_indom_t *indom = (mmv_disk_indom_t *)((char *)addr + offset);

    printf("\nTOC[%d]: offset %ld, indoms offset %" PRIu64 " (%d entries)\n",
		idx, base, offset, count);

    for (i = 0; i < count; i++) {
	__uint64_t off = offset + (i * sizeof(mmv_disk_indom_t));

	if (size < off + sizeof(mmv_disk_indom_t)) {
	    printf("Bad file size: too small for toc[%d] indom[%d]\n", idx, i);
	    return 1;
	}

	printf("  [%u/%"PRIi64"] %d instances, starting at offset %"PRIi64"\n",
		indom[i].serial, off, indom[i].count, indom[i].offset);
	off = indom[i].shorttext;
	if (off != 0) {
	    if (size < off + sizeof(mmv_disk_string_t)) {
		printf("Bad file size: too small for toc[%d] indom[%d] oneline\n", idx, i);
		return 1;
	    }
	    string = (mmv_disk_string_t *)((char *)addr + off);
	    memcpy(buf, string->payload, sizeof(buf));
	    buf[sizeof(buf)-1] = '\0';
	    printf("       shorttext=%s\n", buf);
	}
	else
	    printf("       (no shorttext)\n");
	off = indom[i].helptext;
	if (off != 0) {
	    if (size < off + sizeof(mmv_disk_string_t)) {
		printf("Bad file size: too small for toc[%d] indom[%d] help\n", idx, i);
		return 1;
	    }
	    string = (mmv_disk_string_t *)((char *)addr + off);
	    memcpy(buf, string->payload, sizeof(buf));
	    buf[sizeof(buf)-1] = '\0';
	    printf("       helptext=%s\n", buf);
	}
	else
	    printf("       (no helptext)\n");
    }
    return 0;
}

int
dump_insts1(void *addr, size_t size, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_indom_t *indom;
    mmv_disk_instance_t *inst = (mmv_disk_instance_t *)((char *)addr + offset);

    for (i = 0; i < count; i++) {
	__uint64_t off = offset + (i * sizeof(mmv_disk_instance_t));

	if (size < off + sizeof(mmv_disk_instance_t)) {
	    printf("\nBad file size: too small for toc[%d] inst[%d]\n", idx, i);
	    return 1;
	}
	off = inst[i].indom;
	indom = (mmv_disk_indom_t *)((char *)addr + off);
	if (size < off + sizeof(mmv_disk_indom_t)) {
	    printf("\nBad file size: too small for toc[%d] indom[%d]\n", idx, i);
	    return 1;
	}
	printf("  [%u/%"PRIi64"] instance = [%d or \"%s\"]\n",
		indom->serial,
		(offset + i * sizeof(mmv_disk_instance_t)),
		inst[i].internal, inst[i].external);
    }
    return 0;
}

int
dump_insts2(void *addr, size_t size, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    char buf[MMV_STRINGMAX];
    mmv_disk_indom_t *indom;
    mmv_disk_string_t *string;
    mmv_disk_instance2_t *inst = (mmv_disk_instance2_t *)((char *)addr + offset);

    for (i = 0; i < count; i++) {
	__uint64_t off = offset + (i * sizeof(mmv_disk_instance2_t));

	if (size < off + sizeof(mmv_disk_instance2_t)) {
	    printf("\nBad file size: too small for toc[%d] inst[%d]\n", idx, i);
	    return 1;
	}
	off = inst[i].indom;
	indom = (mmv_disk_indom_t *)((char *)addr + off);
	if (size < off + sizeof(mmv_disk_indom_t)) {
	    printf("\nBad file size: too small for toc[%d] inst[%d] indom\n", idx, i);
	    return 1;
	}
	off = inst[i].external;
	if (size < off + sizeof(mmv_disk_string_t)) {
	    printf("\nBad file size: too small for toc[%d] inst[%d] name\n", idx, i);
	    return 1;
	} else if (off == 0) {
	    printf("\nBad file: invalid offset for toc[%d] inst[%d] name\n", idx, i);
	    return 1;
	}
	string = (mmv_disk_string_t *)((char *)addr + off);
	memcpy(buf, string->payload, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';

	printf("  [%u/%"PRIi64"] instance = [%d or \"%s\"]\n",
		indom->serial,
		(offset + i * sizeof(mmv_disk_instance_t)),
		inst[i].internal, buf);
    }
    return 0;
}

int
dump_insts(void *addr, size_t size, int idx, long base, __uint64_t offset, __int32_t count, int version)
{
    printf("\nTOC[%d]: offset %ld, instances offset %"PRIi64" (%d entries)\n",
		idx, base, offset, count);
    if (version == MMV_VERSION1)
	return dump_insts1(addr, size, idx, base, offset, count);
    return dump_insts2(addr, size, idx, base, offset, count);
}

static char *
metrictype(int mtype)
{
    char *type;

    switch (mtype) {
    case MMV_TYPE_I32:
	type = "32-bit int";
	break;
    case MMV_TYPE_U32:
	type = "32-bit unsigned int";
	break;
    case MMV_TYPE_I64:
	type = "64-bit int";
	break;
    case MMV_TYPE_U64:
	type = "64-bit unsigned int";
	break;
    case MMV_TYPE_FLOAT:
	type = "float";
	break;
    case MMV_TYPE_DOUBLE:
	type = "double";
	break;
    case MMV_TYPE_STRING:
	type = "string";
	break;
    case MMV_TYPE_ELAPSED:
	type = "elapsed";
	break;
    default:
	type = "?";
	break;
    }
    return type;
}

static char *
metricsem(int msem)
{
    char *sem;

    switch (msem) {
    case PM_SEM_COUNTER:
	sem = "counter";
	break;
    case PM_SEM_INSTANT:
	sem = "instant";
	break;
    case PM_SEM_DISCRETE:
	sem = "discrete";
	break;
    default:
	sem = "?";
	break;
    }
    return sem;
}

int
dump_metrics1(void *addr, size_t size, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    char buf[MMV_STRINGMAX];
    mmv_disk_string_t *string;
    mmv_disk_metric_t *m = (mmv_disk_metric_t *)((char *)addr + offset);

    for (i = 0; i < count; i++) {
	__uint64_t off = offset + i * sizeof(mmv_disk_metric_t);

	if (size < off + sizeof(mmv_disk_metric_t)) {
	    printf("Bad file size: too small for toc[%d] metric[%d]\n", idx, i);
	    return 1;
	}
	printf("  [%u/%"PRIi64"] %s\n", m[i].item, off, m[i].name);
	printf("       type=%s (0x%x), sem=%s (0x%x), pad=0x%x\n",
		metrictype(m[i].type), m[i].type,
		metricsem(m[i].semantics), m[i].semantics,
		m[i].padding);
	printf("       units=%s\n", pmUnitsStr(&m[i].dimension));
	if (m[i].indom != PM_INDOM_NULL && m[i].indom != 0)
	    printf("       indom=%d\n", m[i].indom);
	else
	    printf("       (no indom)\n");

	off = m[i].shorttext;
	if (off != 0) {
	    if (size < off + sizeof(mmv_disk_string_t)) {
		printf("Bad file size: too small for toc[%d] metric[%d] oneline\n", idx, i);
		return 1;
	    }
	    string = (mmv_disk_string_t *)((char *)addr + off);
	    memcpy(buf, string->payload, sizeof(buf));
	    buf[sizeof(buf)-1] = '\0';
	    printf("       shorttext=%s\n", buf);
	}
	else
	    printf("       (no shorttext)\n");

	off = m[i].helptext;
	if (off != 0) {
	    if (size < off + sizeof(mmv_disk_string_t)) {
		printf("Bad file size: too small for toc[%d] metric[%d] help\n", idx, i);
		return 1;
	    }
	    string = (mmv_disk_string_t *)((char *)addr + off);
	    memcpy(buf, string->payload, sizeof(buf));
	    buf[sizeof(buf)-1] = '\0';
	    printf("       helptext=%s\n", buf);
	}
	else
	    printf("       (no helptext)\n");
    }
    return 0;
}

int
dump_metrics2(void *addr, size_t size, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    char buf[MMV_STRINGMAX];
    mmv_disk_string_t *string;
    mmv_disk_metric2_t *m = (mmv_disk_metric2_t *)((char *)addr + offset);

    for (i = 0; i < count; i++) {
	__uint64_t name;
	__uint64_t off = offset + i * sizeof(mmv_disk_metric2_t);

	if (size < off + sizeof(mmv_disk_metric2_t)) {
	    printf("Bad file size: too small for toc[%d] metric[%d]\n", idx, i);
	    return 1;
	}
	name = m[i].name;
	if (size < name + sizeof(mmv_disk_string_t)) {
	    printf("Bad file size: too small for toc[%d] metric[%d] name\n", idx, i);
	    return 1;
	} else if (name == 0) {
	    printf("Bad file: invalid offset for toc[%d] metric[%d] name\n", idx, i);
	    return 1;
	}
	string = (mmv_disk_string_t *)((char *)addr + name);
	memcpy(buf, string->payload, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';

	printf("  [%u/%"PRIi64"] %s\n", m[i].item, off, buf);
	printf("       type=%s (0x%x), sem=%s (0x%x), pad=0x%x\n",
		metrictype(m[i].type), m[i].type,
		metricsem(m[i].semantics), m[i].semantics,
		m[i].padding);
	printf("       units=%s\n", pmUnitsStr(&m[i].dimension));
	if (m[i].indom != PM_INDOM_NULL && m[i].indom != 0)
	    printf("       indom=%d\n", m[i].indom);
	else
	    printf("       (no indom)\n");

	off = m[i].shorttext;
	if (off != 0) {
	    if (size < off + sizeof(mmv_disk_string_t)) {
		printf("Bad file size: too small for toc[%d] metric[%d] oneline\n", idx, i);
		return 1;
	    }
	    string = (mmv_disk_string_t *)((char *)addr + off);
	    memcpy(buf, string->payload, sizeof(buf));
	    buf[sizeof(buf)-1] = '\0';
	    printf("       shorttext=%s\n", buf);
	}
	else
	    printf("       (no shorttext)\n");

	off = m[i].helptext;
	if (off != 0) {
	    if (size < off + sizeof(mmv_disk_string_t)) {
		printf("Bad file size: too small for toc[%d] metric[%d] help\n", idx, i);
		return 1;
	    }
	    string = (mmv_disk_string_t *)((char *)addr + off);
	    memcpy(buf, string->payload, sizeof(buf));
	    buf[sizeof(buf)-1] = '\0';
	    printf("       helptext=%s\n", buf);
	}
	else
	    printf("       (no helptext)\n");
    }
    return 0;
}

int
dump_metrics(void *addr, size_t size, int idx, long base, __uint64_t offset, __int32_t count, int version)
{
    printf("\nTOC[%d]: toc offset %ld, metrics offset %"PRIi64" (%d entries)\n",
		idx, base, offset, count);
    if (version == MMV_VERSION1)
	return dump_metrics1(addr, size, idx, base, offset, count);
    return dump_metrics2(addr, size, idx, base, offset, count);
}

int
dump_value(void *addr, size_t size, mmv_disk_value_t *vals, int i, int toc, int type)
{
    mmv_disk_string_t *string;
    struct timeval tv;
    __int64_t t;

    switch (type) {
    case MMV_TYPE_I32:
	printf(" = %d", vals[i].value.l);
	break;
    case MMV_TYPE_U32:
	printf(" = %u", vals[i].value.ul);
	break;
    case MMV_TYPE_I64:
	printf(" = %" PRIi64, vals[i].value.ll);
	break;
    case MMV_TYPE_U64:
	printf(" = %" PRIu64, vals[i].value.ull);
	break;
    case MMV_TYPE_FLOAT:
	printf(" = %f", vals[i].value.f);
	break;
    case MMV_TYPE_DOUBLE:
	printf(" = %lf", vals[i].value.d);
	break;
    case MMV_TYPE_STRING:
	string = (mmv_disk_string_t *)((char *)addr + vals[i].extra);
	if (size < vals[i].extra + sizeof(mmv_disk_string_t)) {
	    printf(" = ?\n");
	    printf("Bad file size: toc[%d] str value[%d] extra\n", toc, i);
	    return 1;
	}
	printf(" = \"%s\"", string->payload);
	break;
    case MMV_TYPE_ELAPSED:
	pmtimevalNow(&tv);
	t = vals[i].value.ll;
	if (vals[i].extra < 0)
	    t += ((tv.tv_sec*1e6 + tv.tv_usec) + vals[i].extra);
	printf(" = %"PRIi64" (value=%"PRIi64"/extra=%"PRIi64")",
			t, vals[i].value.ll, vals[i].extra);
	if (vals[i].extra > 0) {
	    putchar('\n');
	    printf("Bad (positive) ELAPSED 'extra' value found!");
	}
	break;
    default:
	printf("Unknown type %d", type);
    }
    putchar('\n');
    return 0;
}

int
dump_values1(void *addr, size_t size, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    __uint64_t moff, off;
    mmv_disk_instance_t *instance;
    mmv_disk_metric_t *metric;
    mmv_disk_value_t *vals = (mmv_disk_value_t *)((char *)addr + offset);

    for (i = 0; i < count; i++) {
	off = offset + (i * sizeof(mmv_disk_value_t));
	if (size < off + sizeof(mmv_disk_value_t)) {
	    printf("Bad file size: too small for toc[%d] value[%d]\n", idx, i);
	    return 1;
	}
	moff = vals[i].metric;
	if (size < moff + sizeof(mmv_disk_metric_t)) {
	    printf("Bad file size: toc[%d] value[%d] metric offset\n", idx, i);
	    return 1;
	}
	metric = (mmv_disk_metric_t *)((char *)addr + moff);
	printf("  [%u/%"PRIu64"] %s", metric->item, off, metric->name);
	if (metric->indom && metric->indom != PM_IN_NULL) {
	    off = vals[i].instance;
	    if (size < off + sizeof(mmv_disk_instance_t)) {
		printf("\n");
		printf("Bad file size: toc[%d] value[%d] inst offset\n", idx, i);
		return 1;
	    }
	    instance = (mmv_disk_instance_t *)((char *)addr + off);
	    printf("[%d or \"%s\"]", instance->internal, instance->external);
	}
	dump_value(addr, size, vals, i, idx, metric->type);
    }
    return 0;
}

int
dump_values2(void *addr, size_t size, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    char buf[MMV_STRINGMAX];
    __uint64_t moff, off, name;
    mmv_disk_value_t *vals = (mmv_disk_value_t *)((char *)addr + offset);
    mmv_disk_string_t *string;
    mmv_disk_metric2_t *metric;
    mmv_disk_instance2_t *instance;

    for (i = 0; i < count; i++) {
	off = offset + (i * sizeof(mmv_disk_value_t));
	if (size < off + sizeof(mmv_disk_value_t)) {
	    printf("Bad file size: too small for toc[%d] value[%d]\n", idx, i);
	    return 1;
	}
	moff = vals[i].metric;
	if (size < moff + sizeof(mmv_disk_metric2_t)) {
	    printf("Bad file size: toc[%d] value[%d] metric offset\n", idx, i);
	    return 1;
	}
	metric = (mmv_disk_metric2_t *)((char *)addr + moff);
	name = metric->name;
	if (size < name + sizeof(mmv_disk_string_t)) {
	    printf("Bad file size: too small for toc[%d] value[%d] mname\n", idx, i);
	    return 1;
	} else if (name == 0) {
	    printf("Bad file: invalid offset for toc[%d] value[%d] mname\n", idx, i);
	    return 1;
	}
	string = (mmv_disk_string_t *)((char *)addr + name);
	memcpy(buf, string->payload, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	printf("  [%u/%"PRIu64"] %s", metric->item, off, buf);
	if (metric->indom && metric->indom != PM_IN_NULL) {
	    off = vals[i].instance;
	    if (size < off + sizeof(mmv_disk_instance2_t)) {
		printf("\n");
		printf("Bad file size: toc[%d] value[%d] inst offset\n", idx, i);
		return 1;
	    }
	    instance = (mmv_disk_instance2_t *)((char *)addr + off);
	    name = instance->external;
	    if (size < name + sizeof(mmv_disk_string_t)) {
		printf("Bad file size: too small for toc[%d] value[%d] iname\n", idx, i);
		return 1;
	    } else if (name == 0) {
		printf("Bad file: invalid offset for toc[%d] metric[%d] iname\n", idx, i);
		return 1;
	    }
	    string = (mmv_disk_string_t *)((char *)addr + name);
	    memcpy(buf, string->payload, sizeof(buf));
	    buf[sizeof(buf)-1] = '\0';
	    printf("[%d or \"%s\"]", instance->internal, buf);
	}
	dump_value(addr, size, vals, i, idx, metric->type);
    }
    return 0;
}

int
dump_values(void *addr, size_t size, int idx, long base, __uint64_t offset, __int32_t count, int version)
{
    printf("\nTOC[%d]: offset %ld, values offset %"PRIu64" (%d entries)\n",
		idx, base, offset, count);
    if (version == MMV_VERSION1)
	return dump_values1(addr, size, idx, base, offset, count);
    return dump_values2(addr, size, idx, base, offset, count);
}

int
dump_strings(void *addr, size_t size, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_string_t * string = (mmv_disk_string_t *)
			((char *)addr + offset);

    printf("\nTOC[%d]: offset %ld, string offset %"PRIu64" (%d entries)\n",
		idx, base, offset, count);

    for (i = 0; i < count; i++) {
	__uint64_t off = offset + i * sizeof(mmv_disk_value_t);

	if (size < off + sizeof(mmv_disk_string_t)) {
	    printf("Bad file size: toc[%d] string[%d]\n", idx, i);
	    return 1;
	}
	printf("  [%u/%"PRIu64"] %s\n",
		i+1, offset + i * sizeof(mmv_disk_string_t), 
		string[i].payload);
    }
    return 0;
}

int
dump_labels(void *addr, size_t size, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_label_t *lb = (mmv_disk_label_t *)((char *)addr + offset);

    printf("\nTOC[%d]: offset %ld, labels offset %"PRIi64" (%d entries)\n",
		idx, base, offset, count);

    for (i = 0; i < count; i++) {
	__uint64_t off = offset + i * sizeof(mmv_disk_label_t);

	if (size < off + sizeof(mmv_disk_label_t)) {
	    printf("Bad file size: too small for toc[%d] metric[%d]\n", idx, i);
	    return 1;
	}
	printf("  [%u/%"PRIu64"] %s\n",
		i+1, offset + i * sizeof(mmv_disk_label_t), 
		lb[i].payload);
	printf("        flags=0x%x, identity=0x%x\n",
		lb[i].flags, lb[i].identity);
	printf("        internal=0x%x\n",
		lb[i].internal);
    }
    
    return 0;
}

static char *
flagstr(int flags)
{
    static char buf[128];
    static char bits[32];
    char *ptr;

    if (flags == 0)
	strcat(buf, "none");

    if (flags & MMV_FLAG_NOPREFIX)
	strcat(buf, "noprefix, ");
    if (flags & MMV_FLAG_PROCESS)
	strcat(buf, "process, ");
    if (flags & MMV_FLAG_SENTINEL)
	strcat(buf, "sentinel, ");

    flags &= ~(MMV_FLAG_NOPREFIX | MMV_FLAG_PROCESS | MMV_FLAG_SENTINEL);

    /* unrecognised bits */
    if (flags) {
	pmsprintf(bits, sizeof(bits), "unknown=%x", flags);
	strcat(buf, bits);
    } else {
	/* remove any trailing comma-space */
	if ((ptr = rindex(buf, ',')) != NULL)
	    *ptr = '\0';
    }
    return buf;
}

int
dump(const char *file, void *addr, size_t size)
{
    int i, sts, type, version;
    __uint32_t count;
    __uint64_t offset;
    mmv_disk_toc_t *toc;
    mmv_disk_header_t *hdr;

    hdr = (mmv_disk_header_t *)addr;
    if (size < sizeof(mmv_disk_header_t)) {
	printf("Bad file size: too small for a header\n");
	return 1;
    }

    if (strncmp(hdr->magic, "MMV", 4)) {
	printf("Bad magic: %c%c%c\n",
		hdr->magic[0], hdr->magic[1], hdr->magic[2]);
	return 1;
    }
    version = hdr->version;
    if (version != MMV_VERSION1 && version != MMV_VERSION2 &&
	version != MMV_VERSION3)
    {
	printf("Version %d not supported\n", version);
	return 1;
    }

    printf("MMV file   = %s\n", file);
    printf("Version    = %d\n", version);
    printf("Generated  = %"PRIu64"\n", hdr->g1);
    if (hdr->g1 != hdr->g2) {
	printf("Generated2 = %"PRIu64"\n", hdr->g2);
	printf("Mismatched generation numbers (%"PRIu64"/%"PRIu64")\n",
		hdr->g1, hdr->g2);
	return 1;
    }
    printf("TOC count  = %u\n", hdr->tocs);
    if (hdr->tocs < 2) {
	printf("Bad tocs: invalid table of contents count (%d)\n", hdr->tocs);
	return 1;
    }
    printf("Cluster    = %u\n", hdr->cluster);
    if (hdr->cluster < 0 || hdr->cluster > (1<<12)-1) {
	printf("Bad cluster: %d is not a valid cluster ID\n", hdr->cluster);
	return 1;
    }
    printf("Process    = %d\n", hdr->process);
    printf("Flags      = 0x%x (%s)\n", hdr->flags, flagstr(hdr->flags));

    offset = hdr->tocs * sizeof(mmv_disk_toc_t);
    if (size < sizeof(mmv_disk_header_t) + offset) {
	printf("Bad file size: too small for %d TOC entries\n", hdr->tocs);
	return 1;
    }
    toc = (mmv_disk_toc_t *)((char *)addr + sizeof(mmv_disk_header_t));

    for (i = sts = 0; i < hdr->tocs; i++) {
	__uint64_t base = ((char *)&toc[i] - (char *)addr);

	type = toc[i].type;
	count = toc[i].count;
	offset = toc[i].offset;

	if (count < 1) {
	    printf("Bad TOC[%d]: invalid entry count %d\n", i, count);
	    continue;
	}

	switch (type) {
	case MMV_TOC_INDOMS:
	    if (dump_indoms(addr, size, i, base, offset, count))
		sts = 1;
	    break;
	case MMV_TOC_INSTANCES:
	    if (dump_insts(addr, size, i, base, offset, count, version))
		sts = 1;
	    break;
	case MMV_TOC_VALUES:
	    if (dump_values(addr, size, i, base, offset, count, version))
		sts = 1;
	    break;
	case MMV_TOC_METRICS:
	    if (dump_metrics(addr, size, i, base, offset, count, version))
		sts = 1;
	    break;
	case MMV_TOC_STRINGS:
	    if (dump_strings(addr, size, i, base, offset, count))
		sts = 1;
	    break;
	case MMV_TOC_LABELS:
	    if (dump_labels(addr, size, i, base, offset, count))
		sts = 1;
	    break;    
	default:
	    printf("Unrecognised TOC[%d] type: 0x%x\n", i, type);
	    sts = 1;
	}
    }
    return sts;
}

/* directly access libpcp internal mmap wrapper for portability */
extern void *__pmMemoryMap(int, size_t, int);

int
main(int argc, char **argv)
{
    int fd;
    char file[MAXPATHLEN];
    struct stat s;
    void *addr;

    if (argc > 2) {
	fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
	exit(1);
    }
    if (argc > 1)
	strncpy(file, argv[1], MAXPATHLEN);
    else
	pmsprintf(file, MAXPATHLEN, "%s%cmmv%ctest",
		pmGetConfig("PCP_TMP_DIR"),
		pmPathSeparator(), pmPathSeparator());
    file[MAXPATHLEN-1] = '\0';

    if ((fd = open(file, O_RDONLY)) < 0)
	perror(file);
    else if (fstat(fd, &s) < 0)
	perror(file);
    else if ((addr = __pmMemoryMap(fd, s.st_size, 0)) == NULL)
	perror(file);
    else
	return dump(file, addr, s.st_size);
    return 1;
}
