/* C language writer - using low-level-disk-format structs */
/* Build via: cc -g -Wall -lpcp -o mmv_ondisk mmv_ondisk.c */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>
#include <pcp/mmv_dev.h>
#include <sys/mman.h>

size_t string_offset;
size_t string_next;

mmv_disk_header_t *
create_header(void *mapping, int tocs)
{
    mmv_disk_header_t *header = (mmv_disk_header_t *)mapping;

    memset(header, 0, sizeof(mmv_disk_header_t));
    memcpy(header->magic, "MMV", 4);
    header->version = MMV_VERSION3;
    header->cluster = 1234;
    header->process = 5678;
    header->tocs = tocs;
    header->g1 = header->g2 = 1;
    return header;
}

mmv_disk_toc_t *
create_toc_entry(void *mapping, int index, int type, int count, size_t offset)
{
    mmv_disk_toc_t *toc = (mmv_disk_toc_t *)((char *)mapping + sizeof(mmv_disk_header_t));
    toc += index;	/* note: mmv_disk_toc_t-sized increments */

    memset(toc, 0, sizeof(mmv_disk_toc_t));
    toc->type = type;
    toc->count = count;
    toc->offset = offset;
    return toc;
}

__uint64_t
set_string(void *mapping, char *src)
{
    char	*dst = (char *)((char *)mapping + string_next);
    __uint64_t	ret = string_next;

    strcpy(dst, src);
    string_next += strlen(src) + 1;

    return ret;
}

void
create_metric(void *mapping, size_t offset)
{
    mmv_disk_metric2_t *metric = (mmv_disk_metric2_t *)((char *)mapping + offset);
    pmUnits dimension = MMV_UNITS(0,0,1,0,0,PM_COUNT_ONE);

    memset(metric, 0, sizeof(mmv_disk_metric2_t));
    metric->name = set_string(mapping, "counter");
    metric->item = 1;
    metric->type = MMV_TYPE_U32;
    metric->indom = 1;
    metric->semantics = MMV_SEM_COUNTER;
    metric->dimension = dimension;
    metric->shorttext = set_string(mapping, "A simple-minded counter");
    metric->helptext = set_string(mapping, "\
We have but one metric here, a simple minded\n\
counter");

    return;
}

void
create_indom(void *mapping, size_t indom_offset, size_t instance_offset)
{
    mmv_disk_indom_t *indom = (mmv_disk_indom_t *)((char *)mapping + indom_offset);
    mmv_disk_instance2_t *instance = (mmv_disk_instance2_t *)((char *)mapping + instance_offset);

    memset(indom, 0, sizeof(mmv_disk_indom_t));
    indom->serial = 1;
    indom->count = 1;
    indom->offset = instance_offset;
    indom->shorttext = set_string(mapping, "lucky thirteen indom");
    indom->helptext = set_string(mapping, "foobar");

    memset(instance, 0, sizeof(mmv_disk_instance2_t));
    instance->indom = indom_offset;
    instance->internal = 88;
    instance->external = set_string(mapping, "golden dragon");

    return;
}

void
create_value(void *mapping, size_t offset, size_t metric_offset, size_t instance_offset)
{
    mmv_disk_value_t *value = (mmv_disk_value_t *)((char *)mapping + offset);

    memset(value, 0, sizeof(mmv_disk_value_t));
    value->value.ul = 42;
    value->metric = metric_offset;
    value->instance = instance_offset;
    return;
}

/*
 * toc[0] 1 x metric
 * toc[1] 1 x value
 * toc[2] 1 x indom
 * toc[3] 1 x instance
 * toc[4] strings (1024 bytes max)
 */
#define NUM_TOCS	5
#define STRING_MAX	1024

int 
main(int argc, char **argv)
{
    char path[MAXPATHLEN];
    char *file = (argc > 1) ? argv[1] : "ondisk";
    size_t size, value_offset, metric_offset, indom_offset, instance_offset;
    void *mapping;
    int fd;
    int idx = 0;

    size = sizeof(mmv_disk_header_t);
    size += NUM_TOCS * sizeof(mmv_disk_toc_t);
    size += sizeof(mmv_disk_metric2_t);
    size += sizeof(mmv_disk_value_t);
    size += sizeof(mmv_disk_indom_t);
    size += sizeof(mmv_disk_instance2_t);
    size += STRING_MAX;

    pmsprintf(path, sizeof(path), "%s/mmv/%s", pmGetConfig("PCP_TMP_DIR"), file);
    if ((fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644)) < 0) {
	perror(path);
	exit(1);
    }
    if (ftruncate(fd, size) < 0) {
	perror("ftruncate");
	close(fd);
	exit(1);
    }
    mapping = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
	perror("mmap");
	close(fd);
	exit(1);
    }
    close(fd);

    create_header(mapping, NUM_TOCS);
    metric_offset = sizeof(mmv_disk_header_t) + NUM_TOCS * sizeof(mmv_disk_toc_t);
    create_toc_entry(mapping, idx++, MMV_TOC_METRICS, 1, metric_offset);
    value_offset = metric_offset + sizeof(mmv_disk_metric2_t);
    create_toc_entry(mapping, idx++, MMV_TOC_VALUES, 1, value_offset);
    indom_offset = value_offset + sizeof(mmv_disk_value_t);
    create_toc_entry(mapping, idx++, MMV_TOC_INDOMS, 1, indom_offset);
    instance_offset = indom_offset + sizeof(mmv_disk_indom_t);
    create_toc_entry(mapping, idx++, MMV_TOC_INSTANCES, 1, instance_offset);
    string_next = string_offset = instance_offset + sizeof(mmv_disk_instance2_t);
    create_toc_entry(mapping, idx++, MMV_TOC_STRINGS, 1, string_offset);

    create_metric(mapping, metric_offset);
    create_indom(mapping, indom_offset, instance_offset);
    create_value(mapping, value_offset, metric_offset, instance_offset);

    munmap(mapping, size);
    return 0;
}
