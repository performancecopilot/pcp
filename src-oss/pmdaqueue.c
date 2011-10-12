#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

/*
 * Make a dummy event, timestamped now
 */
void *make_event(size_t size, struct timeval *tv)
{
    void *buffer = malloc(size);
    if (buffer)
	memset(buffer, 0xfeedbeef, size);
    gettimeofday(tv, NULL);
    return buffer;
}

/*
 * Simple filter based on event size, also report whenever called
 */
int apply_filter(int context, void *data, void *event, int size)
{
    size_t filter = (size_t)data;

    fprintf(stderr, "=> apply filter(%d,%d) -> %d", context, filter, size >= filter);
    return (size >= filter);
}

/*
 * Client context ended, ensure release method is called
 */
void release_filter(int context, void *data)
{
    size_t filter = (size_t)data;
    fprintf(stderr, "=> release filter(%d,%d)", context, filter);
}


int
main(int argc, char **argv)
{
    int	sts;
    int errflag = 0;
    int context, size = 0;
    struct timeval tv;
    char *s, *name;
    void *event;
    char c;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "C:c:D:de:F:f:q:")) != EOF) {
	switch (c) {

	case 'c':	/* new client: ID */
	    context = atoi(optarg);
	    sts = pmdaEventNewClient(context);
	    fprintf(stderr, "new client(%d) -> %d", context, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'C':	/* remove client: ID */
	    context = atoi(optarg);
	    sts = pmdaEventEndClient(context);
	    fprintf(stderr, "end client(%d) -> %d", context, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'd':	/* dump queues, events, clients? */
	    break;

	case 'e':	/* create an event on named queue of given size */
	    name = optarg;
	    s = strtok(optarg, ",");
	    if (!s) {
		fprintf(stderr, "%s: invalid event size specification (%s)\n",
			pmProgname, optarg);
		errflag++;
		break;
	    }
	    size = atoi(s);
	    context = pmdaEventQueueHandle(name);
	    if (context < 0) {
		fprintf(stderr, "%s: invalid event queue specification (%s)\n",
			pmProgname, name);
		errflag++;
		break;
	    }
	    event = make_event(size, &tv);
	    sts = pmdaEventQueueAppend(context, event, size, &tv);
	    fprintf(stderr, "add event(%s,%d) -> %d ", name, size, sts);
	    if (sts < 0) fprintf(stderr, "%s", pmErrStr(sts));
	    else __pmPrintStamp(stderr, &tv);
	    fputc('\n', stderr);
	    break;

	case 'q':	/* create queue with name and a max memory size */
	    name = optarg;
	    s = strtok(optarg, ",");
	    if (!s) {
		fprintf(stderr, "%s: invalid queue memory specification (%s)\n",
			pmProgname, optarg);
		errflag++;
		break;
	    }
	    size = atoi(s);
	    sts = pmdaEventNewQueue(name, size);
	    fprintf(stderr, "new queue(%s,%d) -> %d", name, size, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'f':	/* create client filter, limits size */
	    name = optarg;
	    s = strtok(optarg, ",");
	    if (!s) {
		fprintf(stderr, "%s: invalid client filter size specification (%s)\n",
			pmProgname, optarg);
		errflag++;
		break;
	    }
	    size = atoi(s);
	    context = atoi(name);
	    sts = pmdaEventSetFilter(context, (void *)size, apply_filter, release_filter);
	    fprintf(stderr, "set filter(%d,%d) -> %d", context, size, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'F':	/* remove a clients filter */
	    context = atoi(optarg);
	    sts = pmdaEventSetFilter(context, NULL, NULL, NULL);
	    fprintf(stderr, "end filter(%d) -> %d", context, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr, "Usage: %s ...\n", pmProgname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -c id          create a new client\n");
	fprintf(stderr, "  -e name,size   append an event of size on queue\n");
	fprintf(stderr, "  -q name,size   create a new queue with max size\n");
	fprintf(stderr, "  -f id,size     create client filter, limits size\n");
	fprintf(stderr, "  -C id          remove a client by id (ordinal)\n");
	fprintf(stderr, "  -F id          remove a clients filter\n");
	fprintf(stderr, "  -D debug\n");
	fprintf(stderr, "  -d             dump\n");
	exit(1);
    }

    return 0;
}

