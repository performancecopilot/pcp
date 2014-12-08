#!/usr/bin/perl
use strict;

use File::Basename;
use lib dirname (__FILE__);

use Test::More;
use Test::Magpie qw(mock when);
use JVMMemory;

BEGIN {
  plan(tests => 2)
}

my $user_agent = mock;

my $jvm_memory = JVMMemory->new( $user_agent );

when($user_agent)->get('/api/jolokia/read/java.lang:type=Memory?ignoreErrors=true')->then_return({'value' => {'NonHeapMemoryUsage' => { 'max' => 1234 } }});
is($jvm_memory->attribute_for('non_heap_memory_usage', 'max'), 1234, "non_heap_memory_usage attribute is available");

when($user_agent)->get('/api/jolokia/read/java.lang:type=Memory?ignoreErrors=true')->then_return({'value' => {'HeapMemoryUsage' => { 'max' => 1234 } }});
is($jvm_memory->attribute_for('heap_memory_usage', 'max'), 1234, "heap_memory_usage attribute is available");

