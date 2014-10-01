#include <rapl-interface.h>
#include <stdio.h>
#include <assert.h>

#include "mock_pfm.h"

int main(int argc, char **argv)
{
    wrap_sysconf_override = 1;
    wrap_sysconf_retcode = 12;

    int i;

    rapl_init();

    i = rapl_get_os_event_encoding("test", 0, 0);
    assert( i == -1 );

    rapl_data_t arg;

    i = rapl_get_os_event_encoding("test", 0, &arg);
    assert( i == -1 );

    i = rapl_get_os_event_encoding("RAPL:PACKAGE_ENERGY", 0, &arg);
    assert( i == 0 );

    rapl_data_t arg2;
    i = rapl_get_os_event_encoding("RAPL:PP1_ENERGY", 0, &arg2);
    assert( i == -1 );

    i = rapl_get_os_event_encoding("RAPL:DRAM_ENERGY", 11, &arg2);
    assert( i == 0 );

    i = rapl_open(&arg);
    assert( i == 0);

    i = rapl_open(&arg2);

    assert( i == 0);

    uint64_t res = 0;
    i = rapl_read(&arg, &res);
    assert( i == 0 );
    assert( res == 16383750 ); 
    
    rapl_destroy();

    return 0;
}
