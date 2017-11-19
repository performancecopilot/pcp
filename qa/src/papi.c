/*
 * Copyright (c) 2014 Red Hat.
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

#include <pcp/pmapi.h>
#include <papi.h>



int
PAPI_add_event(int EventSet, int EventCode)
{

}

int
PAPI_enum_event( int *EventCode, int modifier )
{

}

int
PAPI_state( int EventSet, int *status )
{

}

int
PAPI_library_init( int version )
{

}

int
PAPI_create_eventset( int *EventSet )
{

}

int
PAPI_start( int EventSet )
{

}

int
PAPI_read( int EventSet, long long *values )
{

}

int
PAPI_stop( int EventSet, long long *values )
{

}

int
PAPI_destroy_eventset( int *EventSet )
{

}

int
PAPI_cleanup_eventset( int EventSet )
{

}

int
PAPI_get_event_info( int EventCode, PAPI_event_info_t *info )
{

}

int
PAPI_reset( int EventSet )
{

}

int
PAPI_remove_events( int EventSet, int *Events, int number )
{

}
