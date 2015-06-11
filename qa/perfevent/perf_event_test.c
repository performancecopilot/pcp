#include "perfinterface.h"
#include "configparser.h"
#include "architecture.h"

#include <stddef.h>
#include <assert.h>

#include <perfmon/pfmlib.h>
#include <stdlib.h>
#include <string.h>

#include "mock_pfm.h"

void test_init()
{
    printf( " ===== %s ==== \n", __FUNCTION__) ;

    const char *eventlist = "config/test_init.txt";

    perfhandle_t *h = perf_event_create(eventlist);

    assert( h != NULL );

    perf_counter *data = NULL;
    int nevents = 0;

    int i = perf_get(h, &data, &nevents);

    assert(i > 0 );
    assert(nevents == 2);
    assert(data != NULL);

    /* Check that the data buffer gets reused in the next call to perf_get */

    perf_counter *olddata = data;
    int oldevents = nevents;

    i = perf_get(h, &data, &nevents);

    assert(i > 0 );
    assert(nevents == oldevents);
    assert(data == olddata);


    perf_event_destroy(h);
    perf_counter_destroy(data, nevents);
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

    int i = perf_get(h, &data, &nevents);

    assert(i > 0 );
    assert(nevents == 2);
    assert(data != NULL);

    assert( 0 == strcmp("counter3", data[0].name) );
    assert( 0 == strcmp("counter0", data[1].name) );

    perf_counter_destroy(data, nevents);
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

    int count = perf_get(h, &data, &size);

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
    perf_counter_destroy(data, size);
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

    int count = perf_get(h, &data, &size);

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
    perf_counter_destroy(data, size);
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
    perf_counter_destroy( NULL, 213);
    perf_event_destroy( NULL );
    int tmp;
    perf_counter *data;
    perf_get(NULL, &data, &tmp);
    perfhandle_t tmp1;
    perf_get( &tmp1, NULL, &tmp);
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

    int count = perf_get(h, &data, &size);

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
    perf_counter_destroy(data, size);
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
