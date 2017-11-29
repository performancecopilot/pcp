/* C language writer - using low-level-disk-format structs */
/* Build via: cc -g -Wall -lpcp -o mmv_ondisk mmv_ondisk.c */

#include <pcp/pmapi.h>
#include <pcp/mmv_stats.h>
#include <pcp/mmv_dev.h>
#include <sys/mman.h>

mmv_disk_header_t *
create_header(void *mapping, int tocs)
{
    mmv_disk_header_t *header = (mmv_disk_header_t *)mapping;

    memset(header, 0, sizeof(mmv_disk_header_t));
    strncpy(header->magic, "MMV", 4);
    header->version = MMV_VERSION;
    header->cluster = 123;
    header->process = getpid();
    header->tocs = tocs;
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

mmv_disk_metric_t *
create_metric(void *mapping, size_t offset)
{
    mmv_disk_metric_t *metric = (mmv_disk_metric_t *)((char *)mapping + offset);
    pmUnits dimension = MMV_UNITS(0,0,1,0,0,PM_COUNT_ONE);

    memset(metric, 0, sizeof(mmv_disk_metric_t));
    strcpy(metric->name, "ondisk.counter");
    metric->item = 1;
    metric->type = MMV_TYPE_U32;
    metric->indom = PM_INDOM_NULL;
    metric->semantics = MMV_SEM_COUNTER;
    metric->dimension = dimension;
    return metric;
}

mmv_disk_value_t *
create_value(void *mapping, size_t offset, size_t metric_offset)
{
    mmv_disk_value_t *value = (mmv_disk_value_t *)((char *)mapping + offset);

    memset(value, 0, sizeof(mmv_disk_value_t));
    value->value.ul = 42;
    value->metric = metric_offset;
    return value;
}
int 
main(int argc, char **argv)
{
    char path[MAXPATHLEN];
    char *file = (argc > 1) ? argv[1] : "ondisk";
    size_t size, value_offset, metric_offset;
    void *mapping;
    int fd;

    size = sizeof(mmv_disk_header_t);
    size += sizeof(mmv_disk_toc_t);	/* one metric */
    size += sizeof(mmv_disk_toc_t);	/* one value */
    size += sizeof(mmv_disk_metric_t);
    size += sizeof(mmv_disk_value_t);

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

    /* the metric structure follows behind the preamble */
    metric_offset = sizeof(mmv_disk_header_t);
    metric_offset += sizeof(mmv_disk_toc_t) * 2;

    /* the value structure follows the metric structure */
    value_offset = sizeof(mmv_disk_header_t);
    value_offset += sizeof(mmv_disk_toc_t) * 2;
    value_offset += sizeof(mmv_disk_metric_t);

    create_header(mapping, 2);
    create_toc_entry(mapping, 0, MMV_TOC_METRICS, 1, metric_offset);
    create_toc_entry(mapping, 1, MMV_TOC_VALUES, 1, value_offset);
    create_metric(mapping, metric_offset);
    create_value(mapping, value_offset, metric_offset);

    munmap(mapping, size);
    return 0;
}
