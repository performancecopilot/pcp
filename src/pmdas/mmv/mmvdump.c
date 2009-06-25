/*
 * Copyright (C) 2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (C) 2009 Aconex.  All Rights Reserved.
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

#include <pcp/mmv_dev.h>
#include <pcp/impl.h>
#include <sys/stat.h>

void
dump_metrics(void *addr, int idx, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_string_t * string;
    mmv_disk_metric_t * m = (mmv_disk_metric_t *)
				((char *)addr + offset);

    printf("\nTOC[%d]: offset %lld, metrics section (%d entries)\n",
		idx, (long long)offset, count);

    for (i = 0; i < count; i++) {
	__uint64_t off = offset + i * sizeof(mmv_disk_metric_t);
	printf("  [%u/%lld] %s\n", m[i].item, (long long)off, m[i].name);
	printf("       type=0x%x, sem=0x%x, pad=0x%x\n",
		m[i].type, m[i].semantics, m[i].padding);
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
dump_values(void *addr, int idx, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_value_t * vals = (mmv_disk_value_t *)
			((char *)addr + offset);

    printf("\nTOC[%d]: offset %lld, values section (%d entries)\n",
		idx, (long long)offset, count);

    for (i = 0; i < count; i++) {
	mmv_disk_string_t * string;
	mmv_disk_metric_t * m = (mmv_disk_metric_t *)
				((char *)addr + vals[i].metric);
	__uint64_t off = offset + i * sizeof(mmv_disk_value_t);

	printf("  [%u/%lld] %s", m->item, (long long)off, m->name);
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
	    printf(" = %lld", (long long)vals[i].value.ll);
	    break;
	case MMV_TYPE_U64:
	    printf(" = %llu", (unsigned long long)vals[i].value.ull);
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
	    long long t;

	    gettimeofday(&tv, NULL);
	    t = vals[i].value.ll +
		vals[i].extra + (tv.tv_sec*1e6 + tv.tv_usec);
	    printf(" = %lld", t);
	    break;
	}
	default:
	    printf("Unknown type %d", m->type);
	}
	putchar('\n');
    }
}

void
dump_indoms(void *addr, int idx, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_string_t * string;
    mmv_disk_indom_t * indom = (mmv_disk_indom_t *)
			((char *)addr + offset);

    printf("\nTOC[%d]: offset %lld, indoms section (%d entries)\n",
		idx, (long long)offset, count);

    for (i = 0; i < count; i++) {
	__uint64_t off = offset + i * sizeof(mmv_disk_indom_t);
	printf("  [%u/%lld] %d instances, starting at offset %lld\n",
		indom[i].serial, (long long)off,
		indom[i].count, (long long)indom[i].offset);
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
dump_instances(void *addr, int idx, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_instance_t * inst = (mmv_disk_instance_t *)
			((char *)addr + offset);

    printf("\nTOC[%d]: offset %lld, instances section (%d entries)\n",
		idx, (long long)offset, count);

    for (i = 0; i < count; i++)
	printf("  [%u/%lld] indom offset %lld - %d/%s\n",
		i, (long long)offset + i * sizeof(mmv_disk_instance_t),
		(long long)inst[i].indom,
		inst[i].internal, inst[i].external);
}

void
dump_string(void *addr, int idx, __uint64_t offset, __int32_t count)
{
    int i;
    mmv_disk_string_t * string = (mmv_disk_string_t *)
			((char *)addr + offset);

    printf("\nTOC[%d]: offset %lld, string section (%d entries)\n",
		idx, (long long)offset, count);

    for (i = 0; i < count; i++)
	printf("  [%u/%lld] %s\n",
		i, (long long)offset + i * sizeof(mmv_disk_string_t), 
		string->payload);
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
    printf("Generated  = %s", ctime((time_t *)&hdr->g1));
    if (hdr->g1 != hdr->g2) {
	printf("Generated2 = %s", ctime((time_t *)&hdr->g2));
	printf("Mismatched generation numbers\n");
	return 1;
    }
    printf("TOC count  = %u\n", hdr->tocs);
    printf("Cluster    = %u\n", hdr->cluster);
    printf("Process    = %u\n", hdr->process);
    printf("Flags      = 0x%x\n", hdr->flags);

    for (i = 0; i < hdr->tocs; i++) {
	switch (toc[i].type) {
	case MMV_TOC_VALUES:
	    dump_values(addr, i, toc[i].offset, toc[i].count);
	    break;
	case MMV_TOC_METRICS:
	    dump_metrics(addr, i, toc[i].offset, toc[i].count);
	    break;
	case MMV_TOC_INDOMS:
	    dump_indoms(addr, i, toc[i].offset, toc[i].count);
	    break;
	case MMV_TOC_INSTANCES:
	    dump_instances(addr, i, toc[i].offset, toc[i].count);
	    break;
	case MMV_TOC_STRING:
	    dump_string(addr, i, toc[i].offset, toc[i].count);
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

    if ((fd = open(file, O_RDONLY)) < 0)
	perror(file);
    else {
	struct stat s;
	void * addr;

	fstat(fd, &s);
	if ((addr = __pmMemoryMap(fd, s.st_size, 0)) != NULL)
	    return dump(file, addr);
    }
    return 1;
}
