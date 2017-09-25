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
	memset(buffer, 0xA, size);
    gettimeofday(tv, NULL);
    return buffer;
}

/*
 * Simple filter based on event size, also report whenever called
 */
int apply_filter(void *data, void *event, size_t size)
{
    size_t filter = *(size_t *)data;

    fprintf(stderr, "=> apply-filter(%d<%d) -> %d\n",
	    (int)filter, (int)size, size >= filter);
    return (size >= filter);
}

/*
 * Client context ended, ensure release method is called
 */
void release_filter(void *data)
{
    size_t filter = *(size_t *)data;
    fprintf(stderr, "=> release filter(%d)\n", (int)filter);
}

/*
 * Report stats about the state of given queue identifier
 */
void queue_statistics(int q)
{
    pmAtomValue count, bytes, clients, memory;

    pmdaEventQueueCounter(q, &count);
    pmdaEventQueueBytes(q, &bytes);
    pmdaEventQueueClients(q, &clients);
    pmdaEventQueueMemory(q, &memory);

    fprintf(stderr, "event queue#%d count=%d, bytes=%d, clients=%d, mem=%lld\n",
	    q, (int)count.ul, (int)bytes.ull, (int)clients.ul,
	    (long long)memory.ull);
}

/*
 * Report and check contents of events in a given queue identifier
 * decode_event is a callback, called once per-event.
 */

int decode_event(int key, void *event, size_t size,
		 struct timeval *timestamp, void *data)
{
    char *buffer = (char *)event;
    int *datap = (int *)data;
    int queueid = datap[0];
    int context = datap[1];
    int i, ok = 1;

    for (i = 0; i < size; i++)
	if (buffer[i] != 0xA)
	    ok = 0;
    fprintf(stderr, "queue#%d client#%d event: %p, size=%d check=%s\n",
	    queueid, context, event, (int)size, ok? "ok" : "bad");
    return 0;
}

void queue_events(int q, int context)
{
    pmAtomValue records;
    int data[2] = { q, context };

    fprintf(stderr, "walking queue#%d events for client#%d\n", q, context);
    pmdaEventQueueRecords(q, &records, context, decode_event, &data);
    fprintf(stderr, "end walk queue#%d\n", q);
}

int
main(int argc, char **argv)
{
    int	c, sts;
    int errflag = 0;
    int context, queueid;
    size_t size;
    size_t filter_size;
    struct timeval tv;
    char *s, *name;
    void *event;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "A:a:C:c:D:E:e:F:f:q:s:S:")) != EOF) {
	switch (c) {

	case 'a':	/* disallow a clients queue access */
	case 'A':	/* allow a clients queue access */
	    s = optarg;
	    name = strsep(&s, ",");
	    if (!s) {
		fprintf(stderr, "%s: invalid client queue access specification (%s)\n",
			pmProgname, optarg);
		errflag++;
		break;
	    }
	    context = atoi(name);
	    queueid = pmdaEventQueueHandle(s);

	    sts = pmdaEventSetAccess(context, queueid, (c == 'A'));
	    fprintf(stderr, "enable queue#%d access(%d) -> %d",
			    queueid, context, (c == 'A'));
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

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

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case 'e':	/* create an event on named queue of given size */
	    s = optarg;
	    name = strsep(&s, ",");
	    if (!s) {
		fprintf(stderr, "%s: invalid event size specification (%s)\n",
			pmProgname, optarg);
		errflag++;
		break;
	    }
	    size = atoi(s);
	    queueid = pmdaEventQueueHandle(name);
	    if (queueid < 0) {
		fprintf(stderr, "%s: invalid event queue specification (%s)\n",
			pmProgname, name);
		errflag++;
		break;
	    }
	    event = make_event(size, &tv);
	    sts = pmdaEventQueueAppend(queueid, event, size, &tv);
	    fprintf(stderr, "add event(%s,%d) -> %d ", name, (int)size, sts);
	    if (sts < 0) fprintf(stderr, "%s", pmErrStr(sts));
	    else __pmPrintStamp(stderr, &tv);
	    fputc('\n', stderr);
	    break;

	case 'E':	/* create an event on queue ID of given size */
	    s = optarg;
	    name = strsep(&s, ",");
	    if (!s) {
		fprintf(stderr, "%s: invalid event size specification (%s)\n",
			pmProgname, optarg);
		errflag++;
		break;
	    }
	    queueid = atoi(name);
	    size = atoi(s);
	    event = make_event(size, &tv);
	    sts = pmdaEventQueueAppend(queueid, event, size, &tv);
	    fprintf(stderr, "add queue#%d event(%s,%d) -> %d ",
		    queueid, name, (int)size, sts);
	    if (sts < 0) fprintf(stderr, "%s", pmErrStr(sts));
	    else __pmPrintStamp(stderr, &tv);
	    fputc('\n', stderr);
	    break;

	case 'q':	/* create queue with name and a max memory size */
	    s = optarg;
	    name = strsep(&s, ",");
	    if (!s) {
		fprintf(stderr, "%s: invalid queue memory specification (%s)\n",
			pmProgname, optarg);
		errflag++;
		break;
	    }
	    size = atoi(s);
	    sts = pmdaEventNewQueue(name, size);
	    fprintf(stderr, "new queue(%s,%d) -> %d", name, (int)size, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'f':	/* create client filter, limits size */
	    s = optarg;
	    name = strsep(&s, ",");
	    if (!s) {
		fprintf(stderr, "%s: invalid client filter queue specification (%s)\n",
			pmProgname, optarg);
		errflag++;
		break;
	    }
	    context = atoi(name);
	    name = strsep(&s, ",");
	    if (!s) {
		fprintf(stderr, "%s: invalid client filter size specification (%s)\n",
			pmProgname, optarg);
		errflag++;
		break;
	    }
	    queueid = pmdaEventQueueHandle(name);
	    filter_size = atoi(s);
	    sts = pmdaEventSetFilter(context, queueid, (void *)&filter_size,
				     apply_filter, release_filter);
	    fprintf(stderr, "client#%d set filter(sz<%d) on queue#%d-> %d",
		    context, (int)filter_size, queueid, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 'F':	/* remove a clients filter */
	    s = optarg;
	    name = strsep(&s, ",");
	    if (!s) {
		fprintf(stderr, "%s: invalid client filter queue specification (%s)\n",
			pmProgname, optarg);
		errflag++;
		break;
	    }
	    context = atoi(name);
	    queueid = pmdaEventQueueHandle(s);

	    sts = pmdaEventSetFilter(context, queueid, NULL, NULL, NULL);
	    fprintf(stderr, "end queue#%d filter(%d) -> %d", queueid, context, sts);
	    if (sts < 0) fprintf(stderr, " %s", pmErrStr(sts));
	    fputc('\n', stderr);
	    break;

	case 's':	/* dump queue, events, clients counters */
	    name = optarg;
	    queueid = pmdaEventQueueHandle(name);
	    if (queueid < 0) {
		fprintf(stderr, "%s: invalid queue specification (%s)\n",
			pmProgname, name);
		errflag++;
		break;
	    }
	    queue_statistics(queueid);
	    break;

	case 'S':	/* dump queue contents, check events */
	    s = optarg;
	    name = strsep(&s, ",");
	    if (!s) {
		fprintf(stderr, "%s: invalid client filter queue specification (%s)\n",
			pmProgname, optarg);
		errflag++;
		break;
	    }
	    context = atoi(name);
	    queueid = pmdaEventQueueHandle(s);
	    queue_events(queueid, context);
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
	fprintf(stderr, "  -a id,name     disable a clients access\n");
	fprintf(stderr, "  -A id,name     enable a clients access\n");
	fprintf(stderr, "  -c id          create a new client\n");
	fprintf(stderr, "  -C id          remove a client by id\n");
	fprintf(stderr, "  -e name,size   append an event of size on queue\n");
	fprintf(stderr, "  -E id,size     append an event of size on queue\n");
	fprintf(stderr, "  -q name,size   create a new queue with max size\n");
	fprintf(stderr, "  -f id,name,sz  client queue filter, limit size\n");
	fprintf(stderr, "  -F id,name     remove a clients queue filter\n");
	fprintf(stderr, "  -D debug\n");
	fprintf(stderr, "  -s name        report statistics for a queue\n");
	fprintf(stderr, "  -S id,name     report clients events in a queue\n");
	exit(1);
    }

    return 0;
}
