#include "perfinterface.h"
#include "configparser.h"
#include "architecture.h"

#include <stddef.h>
#include <assert.h>

#include <perfmon/pfmlib.h>
#include <stdlib.h>
#include <string.h>

#include "parse_events.h"
#include "mock_pfm.h"

void test_init()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/test_init.txt";

    perfhandle_t *h = perf_event_create(eventlist);

    assert( h != NULL );

    perf_counter *data = NULL;
    int nevents = 0;
    perf_derived_counter *pdata = NULL;
    int nderivedevents = 0;

    int i = perf_get(h, &data, &nevents, &pdata, &nderivedevents);

    assert(i > 0 );
    assert(nevents == 2);
    assert(data != NULL);

    /* Check that the data and pdata buffer gets reused in the next call to perf_get */

    perf_counter *olddata = data;
    int oldevents = nevents;

    i = perf_get(h, &data, &nevents, &pdata, &nderivedevents);

    assert(i > 0 );
    assert(nevents == oldevents);
    assert(data == olddata);

    perf_event_destroy(h);
    perf_counter_destroy(data, nevents, pdata, nderivedevents);
}

void test_event_programming_fail()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;
    // Simulate 6 CPU system
    wrap_sysconf_override = 1;
    wrap_sysconf_retcode = 6;

    // Load configuration file with 4 events - 2 succeed, 1 fails due to get_os_event_encoding, 1 fails due to perf_event_open
    pfm_get_os_event_encoding_retvals[6] = -1;

    perf_event_open_retvals[6] = -1;
    perf_event_open_retvals[7] = -1;
    perf_event_open_retvals[8] = -1;
    perf_event_open_retvals[9] = -1;
    perf_event_open_retvals[10] = -1;
    perf_event_open_retvals[11] = -1;

    const char *eventlist = "config/test_event_programming.txt";

    perfhandle_t *h = perf_event_create(eventlist);

    assert( h != NULL );

    perf_counter *data = NULL;
    int nevents = 0;
    perf_derived_counter *pdata = NULL;
    int nderivedevents = 0;

    int i = perf_get(h, &data, &nevents, &pdata, &nderivedevents);

    assert(i > 0 );
    assert(nevents == 2);
    assert(data != NULL);

    assert( 0 == strcmp("counter3", data[0].name) );
    assert( 0 == strcmp("counter0", data[1].name) );

    perf_counter_destroy(data, nevents, pdata, nderivedevents);
    perf_event_destroy(h);
}

void test_pfm_fail_init()
{
    int i;
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/test_init.txt";

    pfm_initialise_retval = -1;
    perfhandle_t *h = perf_event_create(eventlist);
    assert( h == NULL );
    init_mock();

    for(i = 0; i < RETURN_VALUES_COUNT; ++i) {
        pfm_get_os_event_encoding_retvals[i] = -1;
    }
    h = perf_event_create(eventlist);
    assert( h == NULL );
    init_mock();

    for(i = 0; i < RETURN_VALUES_COUNT; ++i) {
        perf_event_open_retvals[i] = -1;
    }
    h = perf_event_create(eventlist);
    assert( h == NULL );
    init_mock();

    //wrap_ioctl_retval = -1;
    //h = perf_event_create(eventlist);
    //assert( h == NULL );
    //init_mock();

}

void test_fail_init()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/test_fail_init.txt";

    perfhandle_t *h = perf_event_create(eventlist);

    assert( h == NULL );
}

void test_lots_of_counters()
{
    wrap_sysconf_override = 1;
    wrap_sysconf_retcode = 1;

    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/test_lots_of_counters.txt";

    perfhandle_t *h = perf_event_create(eventlist);

    assert( h != NULL );

    perf_counter *data = NULL;
    int size = 0;
    perf_derived_counter *pdata = NULL;
    int derivedsize = 0;

    int count = perf_get(h, &data, &size, &pdata, &derivedsize);

    assert(count > 0 );
    assert(size > 0);
    assert(data != NULL);

    int i;
    int j;
    for(i = 0; i < size; ++i)
    {
        printf("data[%d].name = %s data[%d].instances = %d\n", i, data[i].name, i, data[i].ninstances);
        for(j = 0; j < data[i].ninstances; j++)
        {
            printf("\tvalue[%d] = %llu\n", j, (long long unsigned int)data[i].data[j].value);
        }
    }

    perf_event_destroy(h);
    perf_counter_destroy(data, size, pdata, derivedsize);
    wrap_sysconf_override = 0;
}

void test_node_rr()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    setenv("SYSFS_MOUNT_POINT", "./fakefs/sysrr", 1);
    wrap_sysconf_override = 1;
    wrap_sysconf_retcode = 32;

    const char *configfile = "config/test_node_rr.txt";

    perfhandle_t *h = perf_event_create(configfile);
    assert( h != NULL );

    perf_counter *data = NULL;
    int size = 0;
    perf_derived_counter *pdata = NULL;
    int derivedsize = 0;

    int count = perf_get(h, &data, &size, &pdata, &derivedsize);

    assert(count == (3 * 32 + 4 * 4)  );
    assert(size == (3 + 4) );
    assert(data != NULL);

    int i;
    for(i = 0; i < 4; ++i)
    {
        assert( data[i].ninstances == 4 );
        assert( data[i].data[0].id == (0 + i) );
        assert( data[i].data[1].id == (8 + i) );
        assert( data[i].data[2].id == (16+ i) );
        assert( data[i].data[3].id == (24+ i) );
    }
    
    for(i = 4; i < 7; ++i)
    {
        assert( data[i].ninstances == 32 );
        int j;
        for(j = 0; j < 32; ++j)
        {
            assert( data[i].data[j].id == j);
        }
    }

    perf_event_destroy(h);
    perf_counter_destroy(data, size, pdata, derivedsize);
}

void test_missing_pmu_config()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/empty.txt";

    perfhandle_t *h = perf_event_create(eventlist);
    assert( h == NULL );
}

void test_config()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    configuration_t *config;

    config = parse_configfile("config/test_config.txt");

    assert(config != NULL);
    assert(config->nConfigEntries == 2);
    assert(config->configArr != NULL );

    int i;
    for(i = 0; i < config->nConfigEntries; ++i)
    {
        assert(config->configArr[i].pmcTypeList != NULL);
        assert(config->configArr[i].pmcSettingList != NULL);
    }

    pmctype_t *type = config->configArr[0].pmcTypeList;
    assert(type->name != NULL);
    assert(0 == strcmp(type->name, "pmu1"));
    assert(type->next == NULL);

    pmcsetting_t *setting = config->configArr[0].pmcSettingList;
    assert(setting->name != NULL);
    assert(0 == strcmp(setting->name, "counter4"));
    assert(CPUCONFIG_EACH_CPU == setting->cpuConfig);
    assert(setting->next != NULL);

    setting = setting->next;
    assert(setting->name != NULL);
    assert(0 == strcmp(setting->name, "counter3"));
    assert(CPUCONFIG_ROUNDROBIN_NUMANODE == setting->cpuConfig);
    assert(setting->next != NULL);

    setting = setting->next;
    assert(setting->name != NULL);
    assert(0 == strcmp(setting->name, "counter2"));
    assert(CPUCONFIG_ROUNDROBIN_CPU == setting->cpuConfig);
    assert(setting->next != NULL);

    setting = setting->next;
    assert(setting->name != NULL);
    assert(0 == strcmp(setting->name, "counter1"));
    assert(CPUCONFIG_EACH_CPU == setting->cpuConfig);
    assert(setting->next != NULL);

    setting = setting->next;
    assert(setting->name != NULL);
    assert(0 == strcmp(setting->name, "counter0"));
    assert(CPUCONFIG_EACH_NUMANODE == setting->cpuConfig);
    assert(setting->next == NULL);

    type = config->configArr[1].pmcTypeList;
    assert(type->name != NULL);
    assert(0 == strcmp(type->name, "pmu2 c"));
    assert(type->next != NULL);

    type = type->next;
    assert(type->name != NULL);
    assert(0 == strcmp(type->name, "pmu2b"));
    assert(type->next != NULL);

    type = type->next;
    assert(type->name != NULL);
    assert(0 == strcmp(type->name, "pmu2a"));
    assert(type->next == NULL);

    free_configuration(config);
}

void test_perf_strerror()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    assert( 0 == strcmp("Internal logic error", perf_strerror(-E_PERFEVENT_LOGIC) ) );
    assert( 0 == strcmp("Memory allocation error", perf_strerror(-E_PERFEVENT_REALLOC) ) );
    assert( 0 == strcmp("Runtime error", perf_strerror(-E_PERFEVENT_RUNTIME) ) );
    assert( 0 == strcmp("Unknown error", perf_strerror(0) ) );
}

void test_api_safety()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;
    
    /* Check that it doesn't segfault if the user calls the api with null pointers. */

    perf_event_create(NULL);
    perf_counter_destroy( NULL, 213, NULL, 0);
    perf_event_destroy( NULL );
    int tmp;
    perf_counter *data;
    perf_derived_counter *pdata;
    int derived_tmp;
    perf_get(NULL, &data, &tmp, &pdata, &derived_tmp);
    perfhandle_t tmp1;
    perf_get( &tmp1, NULL, &tmp, &pdata, &derived_tmp);
}

void test_malloc_checking()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/test_init.txt";

    wrap_malloc_fail = 1;
    perfhandle_t *h = perf_event_create(eventlist);
    wrap_malloc_fail = 0;

    assert( h == NULL );
}

void test_config_filenotfound()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    configuration_t *config;

    config = parse_configfile("config/no such file");

    assert(config == NULL);
}

void test_config_syntax_error()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    configuration_t *config;

    config = parse_configfile("config/syntax_error.txt");

    assert(config == NULL);
}

void test_config_empty()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    configuration_t *config;

    config = parse_configfile("config/empty.txt");

    assert(config != NULL);
    assert(config->nConfigEntries == 0);
    
    free_configuration(config);
}

void test_architechure_fail()
{
    wrap_sysconf_override = 1;
    wrap_sysconf_retcode = -1;

    const char *eventlist = "config/test_init.txt";
    perfhandle_t *h = perf_event_create(eventlist);
    assert(h != 0);
    perf_event_destroy(h);

    wrap_sysconf_override = 0;
}

void test_rapl()
{
    wrap_sysconf_override = 1;
    wrap_sysconf_retcode = 1;

    setenv("SYSFS_MOUNT_POINT", "./fakefs/sys", 1);

    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/test_rapl.txt";

    perfhandle_t *h = perf_event_create(eventlist);

    assert( h != NULL );

    perf_counter *data = NULL;
    int size = 0;
    perf_derived_counter *pdata = NULL;
    int derivedsize = 0;

    int count = perf_get(h, &data, &size, &pdata, &derivedsize);

    assert(count > 0 );
    assert(size > 0);
    assert(data != NULL);

    int i;
    int j;
    for(i = 0; i < size; ++i)
    {
        printf("data[%d].name = %s data[%d].instances = %d\n", i, data[i].name, i, data[i].ninstances);
        for(j = 0; j < data[i].ninstances; j++)
        {
            printf("\tvalue[%d] = %llu\n", j, (long long unsigned int)data[i].data[j].value);
        }
    }

    perf_event_destroy(h);
    perf_counter_destroy(data, size, pdata, derivedsize);
    wrap_sysconf_override = 0;
}

/* Note this test requires a pseudo sysfs filesystem setup by an init shell script */
void test_numa_parser()
{
    setenv("SYSFS_MOUNT_POINT", "./fakefs/sys1", 1);
    
    archinfo_t *inst = get_architecture();

    assert(inst != NULL);

    assert(inst->nnodes == 3);

    assert(inst->nodes[0].count == 4);
    assert(inst->nodes[0].index[0] == 0);
    assert(inst->nodes[0].index[1] == 2);
    assert(inst->nodes[0].index[2] == 4);
    assert(inst->nodes[0].index[3] == 6);

    assert(inst->nodes[1].count == 3);
    assert(inst->nodes[1].index[0] == 1);
    assert(inst->nodes[1].index[1] == 3);
    assert(inst->nodes[1].index[2] == 5);

    assert(inst->nodes[2].count == 4);
    assert(inst->nodes[2].index[0] == 8);
    assert(inst->nodes[2].index[1] == 9);
    assert(inst->nodes[2].index[2] == 10);
    assert(inst->nodes[2].index[3] == 14);

    assert(inst->ncpus_per_node == 4);

    assert(inst->cpunodes[0].count == 3);
    assert(inst->cpunodes[0].index[0] == 0);
    assert(inst->cpunodes[0].index[1] == 1);
    assert(inst->cpunodes[0].index[2] == 8);

    assert(inst->cpunodes[1].count == 3);
    assert(inst->cpunodes[1].index[0] == 2);
    assert(inst->cpunodes[1].index[1] == 3);
    assert(inst->cpunodes[1].index[2] == 9);

    assert(inst->cpunodes[2].count == 3);
    assert(inst->cpunodes[2].index[0] == 4);
    assert(inst->cpunodes[2].index[1] == 5);
    assert(inst->cpunodes[2].index[2] == 10);

    assert(inst->cpunodes[3].count == 2);
    assert(inst->cpunodes[3].index[0] == 6);
    assert(inst->cpunodes[3].index[1] == 14);

    free_architecture(inst);
    free(inst);
}

/* Note this test requires a pseudo sysfs filesystem setup by an init shell script */
void test_numa_parser_fail()
{
    setenv("SYSFS_MOUNT_POINT", "./fakefs/sysfail", 1);
    
    archinfo_t *inst = get_architecture();

    assert(inst != NULL);

    assert(inst->nnodes == 1);

    assert(inst->nodes[0].count == 4);
    assert(inst->nodes[0].index[0] == 0);
    assert(inst->nodes[0].index[1] == 1);
    assert(inst->nodes[0].index[2] == 2);
    assert(inst->nodes[0].index[3] == 3);

    assert(inst->ncpus_per_node == 4);
    assert(inst->cpunodes[0].count == 1);
    assert(inst->cpunodes[0].index[0] == 0);

    assert(inst->cpunodes[1].count == 1);
    assert(inst->cpunodes[1].index[0] == 1);

    assert(inst->cpunodes[2].count == 1);
    assert(inst->cpunodes[2].index[0] == 2);

    assert(inst->cpunodes[3].count == 1);
    assert(inst->cpunodes[3].index[0] == 3);

    free_architecture(inst);
    free(inst);
}

void test_derived_counters()
{
    wrap_sysconf_override = 1;
    wrap_sysconf_retcode = 1;

    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/test_derived_counters.txt";

    perfhandle_t *h = perf_event_create(eventlist);

    assert( h != NULL );

    perf_counter *data = NULL;
    int size = 0;
    perf_derived_counter *pddata = NULL;
    int derivedsize = 0;

    int count = perf_get(h, &data, &size, &pddata, &derivedsize);

    assert(count > 0 );
    assert(size > 0);
    assert(data != NULL);
    assert(pddata != NULL);
    assert(derivedsize == 2);

    int i;
    int j;
    for(i = 0; i < derivedsize; ++i)
    {
        printf("pddata[%d].name = %s pddata[%d].instances = %d\n", i, pddata[i].name, i, pddata[i].ninstances);
        perf_counter_list *clist = pddata[i].counter_list;
        while(clist)
        {
            printf("clist->name : %s\n", clist->counter->name);
            clist = clist->next;
        }
        for(j = 0; j < pddata[i].ninstances; j++)
        {
            printf("\tvalue[%d] = %llu\n", j, (long long unsigned int)pddata[i].data[j].value);
        }
    }

    perf_event_destroy(h);
    perf_counter_destroy(data, size, pddata, derivedsize);
    wrap_sysconf_override = 0;
}

void test_derived_counters_fail_mismatch()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/test_derived_counters_fail_mismatch.txt";

    perfdata_t *h = (perfdata_t *)perf_event_create(eventlist);

    assert( h->nderivedevents == 1 );
}

void test_derived_counters_fail_missing()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/test_derived_counters_fail_missing.txt";

    perfdata_t *h = (perfdata_t *)perf_event_create(eventlist);

    assert( h->nderivedevents == 1 );
}

void test_derived_alternate_group()
{
    wrap_sysconf_override = 1;
    wrap_sysconf_retcode = 1;

    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/test_alternate_derived_groups.txt";

    perfhandle_t *h = perf_event_create(eventlist);

    assert( h != NULL );

    perf_counter *data = NULL;
    int size = 0;
    perf_derived_counter *pddata = NULL;
    int derivedsize = 0;

    int count = perf_get(h, &data, &size, &pddata, &derivedsize);

    assert(count > 0 );
    assert(size > 0);
    assert(data != NULL);
    assert(pddata != NULL);
    printf("derived size : %d\n", derivedsize);
    assert(derivedsize == 1);

    int i;
    int j;
    for(i = 0; i < derivedsize; ++i)
    {
        printf("pddata[%d].name = %s pddata[%d].instances = %d\n", i, pddata[i].name, i, pddata[i].ninstances);
        perf_counter_list *clist = pddata[i].counter_list;
        while(clist)
        {
            printf("clist->name : %s\n", clist->counter->name);
            clist = clist->next;
        }
        for(j = 0; j < pddata[i].ninstances; j++)
        {
            printf("\tvalue[%d] = %llu\n", j, (long long unsigned int)pddata[i].data[j].value);
        }
    }

    perf_event_destroy(h);
    perf_counter_destroy(data, size, pddata, derivedsize);
    wrap_sysconf_override = 0;
}

void test_derived_events_scale(void)
{
    wrap_sysconf_override = 1;
    wrap_sysconf_retcode = 1;

    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/test_derived_events_scale.txt";

    perfhandle_t *h = perf_event_create(eventlist);

    assert( h != NULL );

    perf_counter *data = NULL;
    int size = 0;
    perf_derived_counter *pddata = NULL;
    int derivedsize = 0;

    int count = perf_get(h, &data, &size, &pddata, &derivedsize);

    assert(count > 0 );
    assert(size > 0);
    assert(data != NULL);
    assert(pddata != NULL);
    printf("derived size : %d\n", derivedsize);
    assert(derivedsize == 1);

    printf("pddata[0].name = %s pddata[0].instances = %d\n", pddata[0].name, pddata[0].ninstances);
    perf_counter_list *clist = pddata[0].counter_list;
    assert(clist->scale == 0.1);
    printf("clist->name : %s, scale : %f\n", clist->counter->name, clist->scale);
    clist = clist->next;
    assert(clist->scale == 1.0);
    printf("clist->name : %s, scale : %f\n", clist->counter->name, clist->scale);

    perf_event_destroy(h);
    perf_counter_destroy(data, size, pddata, derivedsize);
    wrap_sysconf_override = 0;
}

static int
compar(const void *a, const void *b)
{
    return strcmp(*((const char **)a), *((const char **)b));
}

/*
 * Tests the init_dynamic_events parser code to see whether,
 * the pmus (pmu1 and pmu2) are detected. Also, tests for the total
 * no. of events including the software pmu.
 */
void test_init_dynamic_events(void)
{
    struct pmu *pmu_list = NULL, *tmp = NULL;
    struct pmu_event *event;
    int dyn_pmu_count = 0, dyn_event_count = 0;

    printf( " ===== %s ==== \n", __FUNCTION__) ;

    setenv("SYSFS_PREFIX", "./fakefs/syspmu", 1);

    init_dynamic_events(&pmu_list);
    for (tmp = pmu_list; tmp; tmp = tmp->next) {
	char	**namelist = NULL;
	int	n = 0;
	int	i;
	printf("PMU name : %s\n", tmp->name);
	dyn_pmu_count++;
	/*
	 * order of events is non-deterministic, depending on dir
	 * entry in filesystem, so stash 'em and sort 'em first
	 */
	for (event = tmp->ev; event; event = event->next) {
	    dyn_event_count++;
	    n++;
	    namelist = (char **)realloc(namelist, n*sizeof(namelist[0]));
	    namelist[n-1] = event->name;
	}
	qsort(namelist, n, sizeof(namelist[0]), compar);
	for (i = 0; i < n; i++) {
	    printf("\tevent name : %s\n", namelist[i]);
	}
	free(namelist);
    }
    /* PMUs : pmu1, pmu2, software */
    assert(3 == dyn_pmu_count);
    /* pmu1 : 2 events, pmu2 : 3 events, software : 9 events */
    assert(14 == dyn_event_count);
}

/*
 * This is the case where there are no PMUs exposed, it should only
 * detect the software events.
 */
void test_minimum_dynamic_events(void)
{
    struct pmu *pmu_list = NULL;
    struct pmu_event *event;
    int count = 0;

    printf( " ===== %s ==== \n", __FUNCTION__) ;

    setenv("SYSFS_PREFIX", "./fakefs/syspmu_empty", 1);
    init_dynamic_events(&pmu_list);
    for (event = pmu_list->ev; event; event = event->next) {
	count++;
    }

    printf("%d %s events found\n", count, pmu_list->name);
    /* software events will be initialized in any case */
    assert(0 == strcmp("software", pmu_list->name));
    assert(9 == count);
}

/*
 * A case where one of the events ("bar") in one of the pmus ("pmu1")
 * is rogue. Only 2 pmus ("pmu2" and "software") should be detected
 * in this case, since we won't trust the rogue event's pmu.
 */
void test_dynamic_events_fail_event(void)
{
    struct pmu *pmu_list = NULL, *tmp;
    int count;

    printf( " ===== %s ==== \n", __FUNCTION__) ;

    setenv("SYSFS_PREFIX", "./fakefs/syspmu_fail_event", 1);
    init_dynamic_events(&pmu_list);
    for (tmp = pmu_list, count = 0; tmp; tmp = tmp->next, count++) {
	printf("PMU : %s\n", tmp->name);
	assert(0 != strcmp("pmu1", tmp->name));
    }

    /*
     * 3 PMUs present, but parser reports only 2, ignoring the one
     * (pmu1) for which we have a invalid event value.
     */
    assert(2 == count);
}

/*
 * Events are fine, but one of the fields in the format directory has
 * gone missing ("pmu2") which is needed to decode the events' string.
 * Once again, the parser must ignore the pmu with the rogue format.
 */
void test_dynamic_events_fail_format(void)
{
    struct pmu *pmu_list = NULL, *tmp;
    int count;

    printf( " ===== %s ==== \n", __FUNCTION__) ;

    setenv("SYSFS_PREFIX", "./fakefs/syspmu_fail_format", 1);
    init_dynamic_events(&pmu_list);
    for (tmp = pmu_list, count = 0; tmp; tmp = tmp->next, count++) {
	printf("PMU : %s\n", tmp->name);
	assert(0 != strcmp("pmu2", tmp->name));
    }

    /*
     * 3 PMUs present, but parser reports only 2, ignoring the one
     * (pmu2) for which the format dir is missing the complete syntax.
     */
    assert(2 == count);
}

/*
 * With 3 dynamic events and 2 libpfm events mentioned in the config
 * file, this tests for the total no. of events detected.
 * Since, there are 3 dynamic events mentioned in the config file,
 * other two dynamic events are not allowed to be monitored. This
 * test case makes sure of that.
 */
void test_dynamic_events_config(void)
{
    setenv("SYSFS_PREFIX", "./fakefs/syspmu", 1);
    printf( " ===== %s ==== \n", __FUNCTION__) ;
    const char *configfile = "config/test_dynamic_counters.txt";
    perfdata_t *h = (perfdata_t *)perf_event_create(configfile);
    assert( h != NULL );

    perf_counter *data = NULL;
    int size = 0;
    perf_derived_counter *pdata = NULL;
    int derivedsize = 0;

    perf_get((perfhandle_t *)h, &data, &size, &pdata, &derivedsize);

    /* 2 libpfm counters, 5 dynamic PMU counters, 9 software counters */
    assert(size == (2 + 5 + 9));
    assert(data != NULL);

    event_t *events = h->events;
    int i, j, ev_count = h->nevents;

    /* Allowed events are entered in the config file */
    char *allowed_events[3] = {
	"pmu1.bar",
	"pmu2.cm_event1",
	"pmu2.cm_event3",
    };
    /* Dis-allowed events */
    char *denied_events[2] = {
	"pmu1.foo",
	"pmu2.cm_event2",
    };
    for (i = 0; i < ev_count; i++) {
	for (j = 0; j < 3; j++) {
	    if (!strcmp(allowed_events[j], events[i].name)) {
		assert(0 == events[i].disable_event);
		break;
	    }
	}
	for (j = 0; j < 2; j++) {
	    if (!strcmp(denied_events[j], events[i].name)) {
		assert(1 == events[i].disable_event);
		break;
	    }
	}
    }
}

/*
 * Test dynamic events are loaded correctly if there is no
 * cpumask file in the /sys directory.
 */
void test_dynamic_events_cpumask_handling(void)
{
    setenv("SYSFS_PREFIX", "./fakefs/syspmu_no_cpumask", 1);
    printf( " ===== %s ==== \n", __FUNCTION__) ;
    const char *configfile = "config/test_dynamic_counters.txt";
    perfdata_t *h = (perfdata_t *)perf_event_create(configfile);
    assert( h != NULL );

    perf_counter *data = NULL;
    int size = 0;
    perf_derived_counter *pdata = NULL;
    int derivedsize = 0;

    perf_get((perfhandle_t *)h, &data, &size, &pdata, &derivedsize);

    /* 2 libpfm counters, 5 dynamic PMU counters, 9 software counters */
    assert(size == (2 + 5 + 9));
    assert(data != NULL);

    event_t *events = h->events;
    int i, j, ev_count = h->nevents;

    /* Allowed events are entered in the config file */
    char *allowed_events[3] = {
        "pmu1.bar",
        "pmu2.cm_event1",
        "pmu2.cm_event3",
    };
    /* Dis-allowed events */
    char *denied_events[2] = {
        "pmu1.foo",
        "pmu2.cm_event2",
    };
    for (i = 0; i < ev_count; i++) {
        for (j = 0; j < 3; j++) {
            if (!strcmp(allowed_events[j], events[i].name)) {
                assert(0 == events[i].disable_event);
                break;
            }
        }
        for (j = 0; j < 2; j++) {
            if (!strcmp(denied_events[j], events[i].name)) {
                assert(1 == events[i].disable_event);
                break;
            }
        }
    }
}

/*
 * Test dynamic events are loaded correctly if the entire config
 * file is empty except for the [dynamic] section.
 */
void test_only_dynamic_events()
{
    setenv("SYSFS_PREFIX", "./fakefs/syspmu", 1);
    printf( " ===== %s ==== \n", __FUNCTION__) ;
    const char *configfile = "config/test_only_dynamic_events.txt";
    perfdata_t *h = (perfdata_t *)perf_event_create(configfile);
    assert( h != NULL );

    perf_counter *data = NULL;
    int size = 0;
    perf_derived_counter *pdata = NULL;
    int derivedsize = 0, count = 0;

    perf_get((perfhandle_t *)h, &data, &size, &pdata, &derivedsize);

    /* 5 dynamic PMU counters, 9 software counters */
    assert(size == (5 + 9));
    assert(data != NULL);

    event_t *events = h->events;
    int i, j, ev_count = h->nevents;

    /* Allowed events are entered in the config file */
    char *allowed_events[3] = {
	"pmu1.bar",
	"pmu2.cm_event1",
	"pmu2.cm_event3",
    };
    /* Dis-allowed events */
    char *denied_events[2] = {
	"pmu1.foo",
	"pmu2.cm_event2",
    };
    for (i = 0; i < ev_count; i++) {
	for (j = 0; j < 3; j++) {
	    if (!strcmp(allowed_events[j], events[i].name)) {
		assert(0 == events[i].disable_event);
		count++;
		break;
	    }
	}
	for (j = 0; j < 2; j++) {
	    if (!strcmp(denied_events[j], events[i].name)) {
		assert(1 == events[i].disable_event);
		break;
	    }
	}
    }
    assert(3 == count);
    printf("%d allowed events\n", count);
}

int runtest(int n)
{
    init_mock();

    int ret = 0;
    switch(n)
    {
        case 0:
            test_init();
            break;
        case 1:
            test_fail_init();
            break;
        case 2:
            test_lots_of_counters();
            break;
        case 3:
            test_config();
            break;
        case 4:
            test_config_filenotfound();
            break;
        case 5:
            test_config_syntax_error();
            break;
        case 6:
            test_config_empty();
            break;
        case 7:
            test_pfm_fail_init();
            break;
        case 8:
            test_perf_strerror();
            break;
        case 9:
            test_missing_pmu_config();
            break;
        case 10:
            test_api_safety();
            break;
        case 11:
            test_malloc_checking();
            break;
        case 12:
            test_architechure_fail();
            break;
        case 13:
            test_numa_parser();
            break;
        case 14:
            test_numa_parser_fail();
            break;
        case 15:
            test_node_rr();
            break;
        case 16:
            test_event_programming_fail();
            break;
        case 17:
            test_rapl();
            break;
        case 18:
            test_derived_counters();
            break;
        case 19:
            test_derived_counters_fail_mismatch();
            break;
        case 20:
            test_derived_counters_fail_missing();
            break;
        case 21:
            test_derived_alternate_group();
            break;
        case 22:
            test_derived_events_scale();
            break;
        case 23:
	    test_init_dynamic_events();
	    break;
        case 24:
	    test_minimum_dynamic_events();
	    break;
        case 25:
	    test_dynamic_events_fail_event();
	    break;
        case 26:
	    test_dynamic_events_fail_format();
	    break;
        case 27:
	    test_dynamic_events_config();
	    break;
        case 28:
            test_dynamic_events_cpumask_handling();
            break;
        case 29:
            test_only_dynamic_events();
            break;
        default:
            ret = -1;
    }
    return ret;
}

int main(int argc, char **argv)
{
    int i;
    int test_num = 0;

    if(argc > 1) {
        if(0 == strcmp("all", argv[1]) )
        {
            for(i = 0; runtest(i) == 0; ++i)
            {
            }
            return 0;
        }
         
        test_num = atoi(argv[1]);
    }
    runtest(test_num);
    return 0;
}
