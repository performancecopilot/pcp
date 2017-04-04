#include <perfmanager.h>
#include <perfinterface.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    int i;
    perfmanagerhandle_t *p;
    perf_counter *data;
    int size;
    int enabled;

    perfhandle_t *dummyperf = perf_event_create("");
    p = manager_init( " );

    assert( p != 0 );

    for(i = 0; i < 10; ++i)
    {
        perf_get_r(p, &data, &size);
        enabled = perf_enabled(p);
        printf("Perf enabled = %d\n", enabled);
        sleep(1);
    }

    manager_destroy(p);

    perf_event_destroy(dummyperf);

    return 0;
}
