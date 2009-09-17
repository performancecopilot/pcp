#!/usr/bin/perl
#
# Example server application that demonstrates use of the
# Perl PCP MMV module for runtime instrumentation.
#

use strict;
use warnings;
use PCP::MMV;
use Time::HiRes qw ( usleep );

my @db_instances = ( 0 => "tempdb", 1 => "datadb" );

my $db_indom = 1;
my @indoms = (
    [ $db_indom, \@db_instances,
	'Database instances',
	'An instance domain for each database used by this server.',
    ],
);

my @metrics = (
    [ 'response_time.requests',
	1, MMV_TYPE_U64, MMV_INDOM_NULL,
	mmv_units(0,0,1,0,0,MMV_COUNT_ONE), MMV_SEM_COUNTER,
	'Number of server requests processed', ''
    ],
    [ 'response_time.total',
	2, MMV_TYPE_U64, MMV_INDOM_NULL,
	mmv_units(0,0,1,0,0,MMV_COUNT_ONE), MMV_SEM_COUNTER,
	'Maximum observed response time in milliseconds', ''
    ],
    [ 'response_time.maximum',
	3, MMV_TYPE_DOUBLE, MMV_INDOM_NULL,
	mmv_units(0,1,0,0,MMV_TIME_MSEC,0), MMV_SEM_INSTANT,
	'Maximum observed response time in milliseconds', ''
    ],
    [ 'version',
	4, MMV_TYPE_STRING, MMV_INDOM_NULL,
	mmv_units(0,0,0,0,0,0), MMV_SEM_DISCRETE,
	'Version number of the server process', ''
    ],
    [ 'database.transactions.count',
	5, MMV_TYPE_U64, $db_indom,
	mmv_units(0,0,1,0,0,MMV_COUNT_ONE), MMV_SEM_COUNTER,
	'Number of requests issued to each database', ''
    ],
    [ 'database.transactions.time',
	6, MMV_TYPE_U64, $db_indom,
	mmv_units(0,1,0,0,MMV_TIME_MSEC,0), MMV_SEM_COUNTER,
	'Total time spent waiting for results from each database', ''
    ],
    [ 'idletime',
	7, MMV_TYPE_U64, MMV_INDOM_NULL,
	mmv_units(0,1,0,0,MMV_TIME_USEC,0), MMV_SEM_COUNTER,
	'Total time spent asleep, in-between requests', ''
    ],
);

my $handle = mmv_stats_init('server', 0, MMV_FLAG_PROCESS, \@metrics, \@indoms);
die("mmv_stats_init failed\n") unless (defined($handle));

mmv_stats_set_string($handle, 'version', '', '7.4.2-5');

my $maxtime = 0.0;	# milliseconds

for (;;) {

	my $idletime = 0.0;	# microseconds
	my $dbtime = 0.0;	# milliseconds
	my $response = 0.0;


	# start a request ...

	$dbtime = rand 1000;	# milliseconds
	$response += $dbtime;
	mmv_stats_inc($handle, 'database.transactions.count', 'tempdb');
	mmv_stats_add($handle, 'database.transactions.time', 'tempdb', $dbtime);

	# ... more work, involving a second DB request ...
	$dbtime = rand 1000;	# milliseconds
	$response += $dbtime;
	mmv_stats_inc($handle, 'database.transactions.count', 'datadb');
	mmv_stats_add($handle, 'database.transactions.time', 'datadb', $dbtime);

	# ... request completed


	$response += rand 42;	# milliseconds
	mmv_stats_inc($handle, 'response_time.requests', '');
	mmv_stats_add($handle, 'response_time.total', '', $response);
	if ($response > $maxtime) {
		$maxtime = $response;
		mmv_stats_set($handle, 'response_time.maximum', '', $maxtime);
	}

	$idletime = rand 50000;	# microseconds
	usleep($idletime);
	mmv_stats_add($handle, 'idletime', '', $idletime);
}
