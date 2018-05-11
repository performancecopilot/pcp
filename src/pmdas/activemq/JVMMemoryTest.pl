#!/usr/bin/env perl
#
# Copyright (c) 2014 Aconex
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
# 
use strict;

use File::Basename;
use lib dirname (__FILE__);

use Test::More;
use Test::Magpie qw(mock when);
use PCP::JVMMemory;

BEGIN {
    plan(tests => 2)
}

my $user_agent = mock;

my $jvm_memory = JVMMemory->new( $user_agent );

when($user_agent)->get('/api/jolokia/read/java.lang:type=Memory?ignoreErrors=true')->then_return({'value' => {'NonHeapMemoryUsage' => { 'max' => 1234 } }});
is($jvm_memory->attribute_for('non_heap_memory_usage', 'max'), 1234, "non_heap_memory_usage attribute is available");

when($user_agent)->get('/api/jolokia/read/java.lang:type=Memory?ignoreErrors=true')->then_return({'value' => {'HeapMemoryUsage' => { 'max' => 1234 } }});
is($jvm_memory->attribute_for('heap_memory_usage', 'max'), 1234, "heap_memory_usage attribute is available");

