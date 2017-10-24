/*
 * Copyright (c) 2016 Red Hat.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include <pcp/pmapi.h>
#include <pcp/impl.h>

#include <pcp/mmv_stats.h>
#include <pcp/mmv_dev.h>

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,
    { "header", 0, 'h', 0, "exercise header section corruption" },
    { "contents", 0, 'c', 0, "exercise table-of-contents corruption" },
    { "indoms", 0, 'i', 0, "exercise indom+instance sections corruption" },
    { "metrics", 0, 'm', 0, "exercise metric+value sections corruption" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:hcim?",
    .long_options = longopts,
};

typedef int (*corruptor_t)(void);
corruptor_t corrupt;

static void *
create_mapping(const char *fname, int testcase, size_t size)
{
    char path[MAXPATHLEN];
    void *mapping = NULL;
    int fd, sep = __pmPathSeparator();

    pmsprintf(path, sizeof(path), "%s%c" "mmv" "%c%s-%d",
		pmGetConfig("PCP_TMP_DIR"), sep, sep, fname, testcase);

    if ((fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644)) < 0) {
	perror(path);
	exit(1);
    }
    if (ftruncate(fd, size) < 0) {
	perror("ftruncate");
	exit(1);
    }
    if ((mapping = __pmMemoryMap(fd, size, 1)) == NULL) {
	perror("memory map");
	exit(1);
    }
    close(fd);
    return mapping;
}

static void
finish_mapping(void *mapping, size_t size)
{
    __pmMemoryUnmap(mapping, size);
}

static mmv_disk_header_t *
create_header(void *mapping, int tocs)
{
    mmv_disk_header_t *header = (mmv_disk_header_t *)mapping;

    memset(mapping, 0, sizeof(*header));
    strncpy(header->magic, "MMV", 4);
    header->version = MMV_VERSION;
    header->tocs = tocs;
    return header;
}

static mmv_disk_toc_t *
create_toc_entry(void *mapping, int index, int type, int count, size_t offset)
{
    mmv_disk_toc_t *toc = (mmv_disk_toc_t *)((char *)mapping + sizeof(mmv_disk_header_t));

    toc += index;	/* mmv_disk_toc_t-sized increments */
    toc->type = type;
    toc->count = count;
    toc->offset = offset;
    return toc;
}

static int
corrupt_header(void)
{
    mmv_disk_header_t *header;
    const char section[] = "header";
    size_t length;
    void *mapping;
    int test = 1;

    /* Case #1 - file size smaller than header struct */
    length = sizeof(unsigned long long);
    mapping = create_mapping(section, test++, length);
    header = (mmv_disk_header_t *)mapping;
    strncpy(header->magic, "MMV", 4);
    header->version = MMV_VERSION;
    /* all other fields beyond EOF */
    finish_mapping(mapping, length);

    /* (setup a valid length for remaining test cases) */
    length = sizeof(mmv_disk_header_t);

    /* Case #2 - bad magic */
    mapping = create_mapping(section, test++, length);
    header = create_header(mapping, 2);
    strncpy(header->magic, "MMv", 4);
    finish_mapping(mapping, length);

    /* Case #3 - bad version */
    mapping = create_mapping(section, test++, length);
    header = create_header(mapping, 2);
    header->version = 9999;
    finish_mapping(mapping, length);

    /* Case #4 - invalid cluster */
    mapping = create_mapping(section, test++, length);
    header = create_header(mapping, 2);
    header->cluster = 1 << 26;
    finish_mapping(mapping, length);

    /* Case #5 - zero table-of-contents entry count */
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 0);
    finish_mapping(mapping, length);

    /* Case #6 - negative table-of-contents entry count */
    mapping = create_mapping(section, test++, length);
    create_header(mapping, -1);
    finish_mapping(mapping, length);

    return 0;
}

static int
corrupt_contents(void)
{
    mmv_disk_toc_t *toc;
    const char section[] = "contents";
    size_t length;
    void *mapping;
    int test = 1;

    /* Case #1 - file size smaller than TOC struct */
    length = sizeof(mmv_disk_header_t) + sizeof(unsigned long long);
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 2);
    toc = (mmv_disk_toc_t *)((char *)mapping + sizeof(mmv_disk_header_t));
    toc->type = MMV_TOC_METRICS;
    toc->count = 5;
    /* toc->offset is beyond EOF */
    finish_mapping(mapping, length);

    /* (setup a valid length for remaining test cases) */
    length = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    length += sizeof(mmv_disk_metric_t) + sizeof(mmv_disk_value_t);

    /* Case #2 - invalid type */
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 2);
    create_toc_entry(mapping, 0, 0x270f, 1, 0);
    create_toc_entry(mapping, 1, MMV_TOC_VALUES, 1, 0);
    finish_mapping(mapping, length);

    /* Case #3 - zero entry count */
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 2);
    create_toc_entry(mapping, 0, MMV_TOC_METRICS, 0, 0);
    create_toc_entry(mapping, 1, MMV_TOC_VALUES, 1, 0);
    finish_mapping(mapping, length);

    /* Case #4 - negative entry count */
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 2);
    create_toc_entry(mapping, 0, MMV_TOC_METRICS, -1, 0);
    create_toc_entry(mapping, 1, MMV_TOC_VALUES, 1, 0);
    finish_mapping(mapping, length);

    return 0;
}

static mmv_disk_indom_t *
create_indoms(void *mapping, size_t offset, int count, size_t instances)
{
    mmv_disk_indom_t *indom = (mmv_disk_indom_t *)((char *)mapping + offset);
    static int serial;

    memset(indom, 0, sizeof(*indom));
    indom->serial = ++serial;
    indom->count = count;
    indom->offset = instances;
    return indom;
}

static mmv_disk_instance_t *
create_instance(void *mapping, size_t offset, int id, char *name, size_t indom)
{
    mmv_disk_instance_t *insts = (mmv_disk_instance_t *)((char *)mapping + offset);

    memset(insts, 0, sizeof(*insts));
    insts->indom = indom;
    insts->internal = id;
    strcpy(insts->external, name);
    return insts;
}

static int
corrupt_indoms(void)
{
    mmv_disk_indom_t *indoms;
    const char section[] = "indoms";
    size_t length, indoms_offset, instances_offset, text_offset;
    void *mapping;
    int test = 1;

    indoms_offset = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    instances_offset = indoms_offset + sizeof(mmv_disk_indom_t);
    text_offset = instances_offset + sizeof(mmv_disk_instance_t);

    /* Case #1 - file ends within indoms structure */
    length = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    length += sizeof(char);
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 2);
    create_toc_entry(mapping, 0, MMV_TOC_INDOMS, 1, indoms_offset);
    create_toc_entry(mapping, 1, MMV_TOC_INSTANCES, 1, instances_offset);
    finish_mapping(mapping, length);

    /* Case #2 - file ends within instances structure */
    length = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    length += sizeof(mmv_disk_indom_t) + sizeof(char);
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 2);
    create_toc_entry(mapping, 0, MMV_TOC_INDOMS, 1, indoms_offset);
    create_toc_entry(mapping, 1, MMV_TOC_INSTANCES, 1, instances_offset);
    create_indoms(mapping, indoms_offset, 1, instances_offset);
    finish_mapping(mapping, length);

    /* (setup a valid length for next few test cases) */
    length = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    length += sizeof(mmv_disk_indom_t) + sizeof(mmv_disk_instance_t);

    /* Case #3 - zero indom entry count */
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 2);
    create_toc_entry(mapping, 0, MMV_TOC_INDOMS, 1, indoms_offset);
    create_toc_entry(mapping, 1, MMV_TOC_INSTANCES, 1, instances_offset);
    create_indoms(mapping, indoms_offset, 0, instances_offset);
    create_instance(mapping, instances_offset, 1, "ii", indoms_offset);
    finish_mapping(mapping, length);

    /* Case #4 - negative indom entry count */
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 2);
    create_toc_entry(mapping, 0, MMV_TOC_INDOMS, 1, indoms_offset);
    create_toc_entry(mapping, 1, MMV_TOC_INSTANCES, 1, instances_offset);
    create_indoms(mapping, indoms_offset, -1, instances_offset);
    create_instance(mapping, instances_offset, 1, "ii", indoms_offset);
    finish_mapping(mapping, length);

    /* (setup length+offsets for remaining test cases) */
    length = sizeof(mmv_disk_header_t) + 3 * sizeof(mmv_disk_toc_t);
    length += sizeof(mmv_disk_indom_t) + sizeof(mmv_disk_instance_t);
    indoms_offset = sizeof(mmv_disk_header_t) + 3 * sizeof(mmv_disk_toc_t);
    instances_offset = indoms_offset + sizeof(mmv_disk_indom_t);
    text_offset = instances_offset + sizeof(mmv_disk_instance_t);

    /* Case #5 - file ends within longform helptext */
    length = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    length += sizeof(mmv_disk_indom_t) + sizeof(mmv_disk_instance_t);
    length += sizeof(char);
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 3);
    create_toc_entry(mapping, 0, MMV_TOC_INDOMS, 1, indoms_offset);
    create_toc_entry(mapping, 1, MMV_TOC_INSTANCES, 1, instances_offset);
    create_toc_entry(mapping, 2, MMV_TOC_STRINGS, 1, text_offset);
    indoms = create_indoms(mapping, indoms_offset, 1, instances_offset);
    indoms->helptext = text_offset;
    create_instance(mapping, instances_offset, 1, "ii", indoms_offset);

    /* Case #6 - file ends within shortform helptext */
    length = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    length += sizeof(mmv_disk_indom_t) + sizeof(mmv_disk_instance_t);
    length += sizeof(char);
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 3);
    create_toc_entry(mapping, 0, MMV_TOC_INDOMS, 1, indoms_offset);
    create_toc_entry(mapping, 1, MMV_TOC_INSTANCES, 1, instances_offset);
    create_toc_entry(mapping, 2, MMV_TOC_STRINGS, 1, text_offset);
    indoms = create_indoms(mapping, indoms_offset, 1, instances_offset);
    indoms->shorttext = text_offset;
    create_instance(mapping, instances_offset, 1, "ii", indoms_offset);

    return 0;
}

static mmv_disk_metric_t *
create_metric(void *mapping, char *name, int indom, size_t offset)
{
    mmv_disk_metric_t *metric = (mmv_disk_metric_t *)((char *)mapping + offset);
    static int item;

    memset(metric, 0, sizeof(*metric));
    strcpy(metric->name, name);
    metric->item = ++item;
    metric->type = MMV_TYPE_U32;
    metric->semantics = MMV_SEM_INSTANT;
    metric->indom = indom;
    return metric;
}

static mmv_disk_value_t *
create_value(void *mapping, size_t offset, size_t metric_offset, size_t inst_offset)
{
    mmv_disk_value_t *value = (mmv_disk_value_t *)((char *)mapping + offset);

    memset(value, 0, sizeof(*value));
    value->metric = metric_offset;
    value->instance = inst_offset;
    return value;
}

static int
corrupt_metrics(void)
{
    mmv_disk_value_t *value;
    mmv_disk_metric_t *metric;
    const char section[] = "metrics";
    size_t length, values_offset, metric_offset, text_offset;
    void *mapping;
    int test = 1;

    metric_offset = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    values_offset = metric_offset + sizeof(mmv_disk_metric_t);
    text_offset = values_offset + sizeof(mmv_disk_value_t);

    /* Case #1 - file ends within metrics structure */
    length = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    length += sizeof(char);
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 2);
    create_toc_entry(mapping, 0, MMV_TOC_METRICS, 1, metric_offset);
    create_toc_entry(mapping, 1, MMV_TOC_VALUES, 1, values_offset);
    finish_mapping(mapping, length);

    /* Case #2 - file ends within values structure */
    length = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    length += sizeof(mmv_disk_metric_t) + sizeof(char);
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 2);
    create_toc_entry(mapping, 0, MMV_TOC_METRICS, 1, metric_offset);
    create_toc_entry(mapping, 1, MMV_TOC_VALUES, 1, values_offset);
    create_metric(mapping, "mm.vv", PM_INDOM_NULL, metric_offset);
    finish_mapping(mapping, length);

    /* Case #3 - bad metric back pointer */
    length = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    length += sizeof(mmv_disk_metric_t) + sizeof(mmv_disk_value_t);
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 2);
    create_toc_entry(mapping, 0, MMV_TOC_METRICS, 1, metric_offset);
    create_toc_entry(mapping, 1, MMV_TOC_VALUES, 1, values_offset);
    metric = create_metric(mapping, "mm.vv", PM_INDOM_NULL, metric_offset);
    value = create_value(mapping, values_offset, metric_offset, 0);
    value->metric = length + sizeof(char);
    finish_mapping(mapping, length);

    /* (setup offsets for remaining test cases) */
    metric_offset = sizeof(mmv_disk_header_t) + 3 * sizeof(mmv_disk_toc_t);
    values_offset = metric_offset + sizeof(mmv_disk_metric_t);
    text_offset = values_offset + sizeof(mmv_disk_value_t);

    /* Case #4 - file ends within a string value */
    length = sizeof(mmv_disk_header_t) + 3 * sizeof(mmv_disk_toc_t);
    length += sizeof(mmv_disk_metric_t) + sizeof(mmv_disk_value_t);
    length += sizeof(char);
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 3);
    create_toc_entry(mapping, 0, MMV_TOC_METRICS, 1, metric_offset);
    create_toc_entry(mapping, 1, MMV_TOC_VALUES, 1, values_offset);
    create_toc_entry(mapping, 2, MMV_TOC_STRINGS, 1, text_offset);
    metric = create_metric(mapping, "mm.vv", PM_INDOM_NULL, metric_offset);
    metric->type = MMV_TYPE_STRING;
    value = create_value(mapping, values_offset, metric_offset, 0);
    value->extra = text_offset;
    finish_mapping(mapping, length);

    /* (setup a valid length for remaining test cases) */
    length = sizeof(mmv_disk_header_t) + 3 * sizeof(mmv_disk_toc_t);
    length += sizeof(mmv_disk_metric_t) + sizeof(mmv_disk_value_t);

    /* Case #5 - file ends within longform helptext */
    length = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    length += sizeof(mmv_disk_metric_t) + sizeof(mmv_disk_value_t);
    length += sizeof(char);
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 3);
    create_toc_entry(mapping, 0, MMV_TOC_METRICS, 1, metric_offset);
    create_toc_entry(mapping, 1, MMV_TOC_VALUES, 1, values_offset);
    create_toc_entry(mapping, 2, MMV_TOC_STRINGS, 1, text_offset);
    metric = create_metric(mapping, "mm.vv", PM_INDOM_NULL, metric_offset);
    metric->helptext = text_offset;
    create_value(mapping, values_offset, metric_offset, 0);
    finish_mapping(mapping, length);

    /* Case #6 - file ends within shortform helptext */
    length = sizeof(mmv_disk_header_t) + 2 * sizeof(mmv_disk_toc_t);
    length += sizeof(mmv_disk_metric_t) + sizeof(mmv_disk_value_t);
    length += sizeof(char);
    mapping = create_mapping(section, test++, length);
    create_header(mapping, 3);
    create_toc_entry(mapping, 0, MMV_TOC_METRICS, 1, metric_offset);
    create_toc_entry(mapping, 1, MMV_TOC_VALUES, 1, values_offset);
    create_toc_entry(mapping, 2, MMV_TOC_STRINGS, 1, text_offset);
    metric = create_metric(mapping, "mm.vv", PM_INDOM_NULL, metric_offset);
    metric->shorttext = text_offset;
    create_value(mapping, values_offset, metric_offset, 0);

    return 0;
}

int
main(int argc, char **argv)
{
    int	c, sts;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'D':
	    if ((sts = pmSetDebug(opts.optarg)) < 0) {
		pmprintf("%s: unrecognized debug options specification (%s)\n",
			pmProgname, opts.optarg);
		opts.errors++;
	    }
	    break;

	case 'h':
	    corrupt = corrupt_header;
	    break;

	case 'c':
	    corrupt = corrupt_contents;
	    break;

	case 'i':
	    corrupt = corrupt_indoms;
	    break;

	case 'm':
	    corrupt = corrupt_metrics;
	    break;
	}
    }

    if (opts.errors || !corrupt) {
	pmUsageMessage(&opts);
	return 1;
    }
    return corrupt();
}
