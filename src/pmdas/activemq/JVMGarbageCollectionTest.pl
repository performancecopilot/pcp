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
use PCP::JVMGarbageCollection;

BEGIN {
    plan(tests => 2)
}

my $user_agent = mock;

my $jvm_garbage_collection = JVMGarbageCollection->new( $user_agent );

when($user_agent)->get('/api/jolokia/read/java.lang:type=GarbageCollector,name=PS%20Scavenge?ignoreErrors=true')->then_return({'value' => {'CollectionCount' => 1234 }});
is($jvm_garbage_collection->attribute_for('ps_scavenge', 'collection_count'), 1234, "gc attribute from ps_scavenge");

when($user_agent)->get('/api/jolokia/read/java.lang:type=GarbageCollector,name=PS%20MarkSweep?ignoreErrors=true')->then_return({'value' => {'CollectionCount' => 1234 }});
is($jvm_garbage_collection->attribute_for('ps_mark_sweep', 'collection_count'), 1234, "gc attribute from ps_mark_sweep");

