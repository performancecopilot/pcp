/*
 * Copyright (C) 2013 Red Hat.
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
#include <pcp/impl.h>
#include <inttypes.h>
#include <sys/stat.h>

void
dump_indoms(void *addr, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_string_t * string;
    mmv_disk_indom_t * indom = (mmv_disk_indom_t *)
			((char *)addr + offset);

    printf("\nTOC[%d]: offset %ld, indoms offset %" PRIu64 " (%d entries)\n",
		idx, base, offset, count);

    for (i = 0; i < count; i++) {
	__uint64_t off = offset + i * sizeof(mmv_disk_indom_t);
	printf("  [%u/%"PRIi64"] %d instances, starting at offset %"PRIi64"\n",
		indom[i].serial, off, indom[i].count, indom[i].offset);
	if (indom[i].shorttext) {
	    string = (mmv_disk_string_t *)
			((char *)addr + indom[i].shorttext);
	    printf("       shorttext=%s\n", string->payload);
	}
	else
	    printf("       (no shorttext)\n");
	if (indom[i].helptext) {
	    string = (mmv_disk_string_t *)
			((char *)addr + indom[i].helptext);
	    printf("       helptext=%s\n", string->payload);
	}
	else
	    printf("       (no helptext)\n");
    }
}

void
dump_insts(void *addr, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_instance_t * inst = (mmv_disk_instance_t *)
			((char *)addr + offset);

    printf("\nTOC[%d]: offset %ld, instances offset %"PRIi64" (%d entries)\n",
		idx, base, offset, count);

    for (i = 0; i < count; i++) {
	mmv_disk_indom_t * indom = (mmv_disk_indom_t *)
			((char *)addr + inst[i].indom);
	printf("  [%u/%"PRIi64"] instance = [%d or \"%s\"]\n",
		indom->serial,
		(offset + i * sizeof(mmv_disk_instance_t)),
		inst[i].internal, inst[i].external);
    }
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

void
dump_metrics(void *addr, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_string_t * string;
    mmv_disk_metric_t * m = (mmv_disk_metric_t *)
				((char *)addr + offset);

    printf("\nTOC[%d]: toc offset %ld, metrics offset %"PRIi64" (%d entries)\n",
		idx, base, offset, count);

    for (i = 0; i < count; i++) {
	__uint64_t off = offset + i * sizeof(mmv_disk_metric_t);
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
	if (m[i].shorttext) {
	    string = (mmv_disk_string_t *)
			((char *)addr + m[i].shorttext);
	    printf("       shorttext=%s\n", string->payload);
	}
	else
	    printf("       (no shorttext)\n");
	if (m[i].helptext) {
	    string = (mmv_disk_string_t *)
			((char *)addr + m[i].helptext);
	    printf("       helptext=%s\n", string->payload);
	}
	else
	    printf("       (no helptext)\n");
    }
}

void
dump_values(void *addr, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_value_t * vals = (mmv_disk_value_t *)
			((char *)addr + offset);

    printf("\nTOC[%d]: offset %ld, values offset %"PRIu64" (%d entries)\n",
		idx, base, offset, count);

    for (i = 0; i < count; i++) {
	mmv_disk_string_t * string;
	mmv_disk_metric_t * m = (mmv_disk_metric_t *)
				((char *)addr + vals[i].metric);
	__uint64_t off = offset + i * sizeof(mmv_disk_value_t);

	printf("  [%u/%"PRIu64"] %s", m->item, off, m->name);
	if (m->indom && m->indom != PM_IN_NULL) {
	    mmv_disk_instance_t *indom = (mmv_disk_instance_t *)
				((char *)addr + vals[i].instance);
	    printf("[%d or \"%s\"]",
		    indom->internal, indom->external);
	}

	switch (m->type) {
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
	    printf(" = \"%s\"", string->payload);
	    break;
	case MMV_TYPE_ELAPSED: {
	    struct timeval tv;
	    __int64_t t;

	    __pmtimevalNow(&tv);
	    t = vals[i].value.ll;
	    if (vals[i].extra < 0)
		t += ((tv.tv_sec*1e6 + tv.tv_usec) + vals[i].extra);
	    printf(" = %"PRIi64" (value=%"PRIi64"/extra=%"PRIi64")",
			t, vals[i].value.ll, vals[i].extra);
	    if (vals[i].extra > 0)
		printf("Bad ELAPSED 'extra' value found!");
	    break;
	}
	default:
	    printf("Unknown type %d", m->type);
	}
	putchar('\n');
    }
}

void
dump_strings(void *addr, int idx, long base, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_string_t * string = (mmv_disk_string_t *)
			((char *)addr + offset);

    printf("\nTOC[%d]: offset %ld, string offset %"PRIu64" (%d entries)\n",
		idx, base, offset, count);

    for (i = 0; i < count; i++) {
	printf("  [%u/%"PRIu64"] %s\n",
		i+1, offset + i * sizeof(mmv_disk_string_t), 
		string[i].payload);
    }
}

int
dump(const char *file, void *addr)
{
    int i;
    mmv_disk_header_t * hdr = (mmv_disk_header_t *) addr;
    mmv_disk_toc_t * toc = (mmv_disk_toc_t *)
		((char *)addr + sizeof(mmv_disk_header_t));

    if (strcmp(hdr->magic, "MMV")) {
	printf("Bad magic: %c%c%c\n",
		hdr->magic[0], hdr->magic[1], hdr->magic[2]);
	return 1;
    }
    if (hdr->version != MMV_VERSION) {
	printf("version %d not supported\n", hdr->version);
	return 1;
    }

    printf("MMV file   = %s\n", file);
    printf("Version    = %d\n", hdr->version);
    printf("Generated  = %"PRIu64"\n", hdr->g1);
    if (hdr->g1 != hdr->g2) {
	printf("Generated2 = %"PRIu64"\n", hdr->g2);
	printf("Mismatched generation numbers\n");
	return 1;
    }
    printf("TOC count  = %u\n", hdr->tocs);
    printf("Cluster    = %u\n", hdr->cluster);
    printf("Process    = %d\n", hdr->process);
    printf("Flags      = 0x%x\n", hdr->flags);

    for (i = 0; i < hdr->tocs; i++) {
	__uint64_t base = ((char *)&toc[i] - (char *)addr);
	switch (toc[i].type) {
	case MMV_TOC_INDOMS:
	    dump_indoms(addr, i, base, toc[i].offset, toc[i].count);
	    break;
	case MMV_TOC_INSTANCES:
	    dump_insts(addr, i, base, toc[i].offset, toc[i].count);
	    break;
	case MMV_TOC_VALUES:
	    dump_values(addr, i, base, toc[i].offset, toc[i].count);
	    break;
	case MMV_TOC_METRICS:
	    dump_metrics(addr, i, base, toc[i].offset, toc[i].count);
	    break;
	case MMV_TOC_STRINGS:
	    dump_strings(addr, i, base, toc[i].offset, toc[i].count);
	    break;
	default:
	    printf("Unrecognised TOC[%d] type: 0x%x\n", i, toc[i].type);
	}
    }
    return 0;
}

int 
main(int argc, char * argv[])
{
    int fd;
    char file[MAXPATHLEN];

    if (argc > 2) {
	printf("USAGE: %s <filename>\n", argv[0]);
	exit(1);
    }
    if (argc > 1)
	strncpy(file, argv[1], MAXPATHLEN);
    else
	snprintf(file, MAXPATHLEN, "%s%cmmv%ctest",
		pmGetConfig("PCP_TMP_DIR"),
		__pmPathSeparator(), __pmPathSeparator());
    file[MAXPATHLEN-1] = '\0';

    if ((fd = open(file, O_RDONLY)) < 0)
	perror(file);
    else {
	struct stat s;
	void * addr;

	if (fstat(fd, &s) < 0)
	    perror(file);
	else if ((addr = __pmMemoryMap(fd, s.st_size, 0)) != NULL)
	    return dump(file, addr);
    }
    return 1;
}
