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
use JVMMemoryPool;

BEGIN {
    plan(tests => 5)
}

my $user_agent = mock;

my $jvm_memory_pool = JVMMemoryPool->new( $user_agent );

when($user_agent)->get('/api/jolokia/read/java.lang:type=MemoryPool,name=PS%20Survivor%20Space?ignoreErrors=true')->then_return({'value' => {'PeakUsage' => { 'max' => 1234 } }});
is($jvm_memory_pool->attribute_for('ps_survivor_space', 'peak_usage', 'max'), 1234, "memory pool attribute from a ps_survivor_space");

when($user_agent)->get('/api/jolokia/read/java.lang:type=MemoryPool,name=Code%20Cache?ignoreErrors=true')->then_return({'value' => {'Usage' => { 'committed' => 55 } }});
is($jvm_memory_pool->attribute_for('code_cache', 'usage', 'committed'), 55, "memory pool attribute from code_cache");

when($user_agent)->get('/api/jolokia/read/java.lang:type=MemoryPool,name=PS%20Eden%20Space?ignoreErrors=true')->then_return({'value' => {'Usage' => { 'committed' => 55 } }});
is($jvm_memory_pool->attribute_for('ps_eden_space', 'usage', 'committed'), 55, "memory pool attribute from ps_eden_space");

when($user_agent)->get('/api/jolokia/read/java.lang:type=MemoryPool,name=PS%20Old%20Gen?ignoreErrors=true')->then_return({'value' => {'Usage' => { 'committed' => 55 } }});
is($jvm_memory_pool->attribute_for('ps_old_gen', 'usage', 'committed'), 55, "memory pool attribute from ps_old_gen");

when($user_agent)->get('/api/jolokia/read/java.lang:type=MemoryPool,name=PS%20Perm%20Gen?ignoreErrors=true')->then_return({'value' => {'Usage' => { 'committed' => 55 } }});
is($jvm_memory_pool->attribute_for('ps_perm_gen', 'usage', 'committed'), 55, "memory pool attribute from ps_perm_gen");

