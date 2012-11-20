#
# Copyright (c) 2012 Red Hat.
# Copyright (c) 2011-2012 Aconex.  All Rights Reserved.
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
use warnings;
use FileHandle;
use PCP::PMDA;
use Net::SNMP qw(:asn1);

use Data::Dumper;
$Data::Dumper::Indent = 1;
$Data::Dumper::Sortkeys = 1;
$Data::Dumper::Quotekeys = 0;
$Data::Dumper::Useqq = 1;	# PMDA log doesnt like binary :-(

our $VERSION='0.3';
my $db = {};
my $option = {
	max_row => 100, # default maximum number of rows for a table
	pmid_per_host => 100, # default number of pmid's for each host
};

# SNMP string type name to numeric type number
#
my $snmptype2val = {
    INTEGER => INTEGER32,
    INTEGER32 => INTEGER32,
    OCTET_STRING => OCTET_STRING,
    STRING => OCTET_STRING,
    OBJECT_IDENTIFIER => OBJECT_IDENTIFIER,
    IPADDRESS => IPADDRESS,
    COUNTER => COUNTER32,
    COUNTER32 => COUNTER32,
    GAUGE => GAUGE32,
    GAUGE32 => GAUGE32,
    UNSIGNED32 => UNSIGNED32,
    TIMETICKS => TIMETICKS,
    OPAQUE => OPAQUE,
    COUNTER64 => COUNTER64,
};

# SNMP numeric type number to PCP type number
#
my $snmptype2pcp = {
    0x02 => { type=> PM_TYPE_32, sem=> PM_SEM_INSTANT },	# INTEGER32
    0x04 => { type=> PM_TYPE_STRING, sem=> PM_SEM_DISCRETE },	# OCTET_STRING
    0x06 => { type=> PM_TYPE_STRING, sem=> PM_SEM_DISCRETE },	# OBJECT_IDENTIFIER
    0x40 => { type=> PM_TYPE_STRING, sem=> PM_SEM_DISCRETE },	# IPADDRESS
    0x41 => { type=> PM_TYPE_U32, sem=> PM_SEM_COUNTER },	# COUNTER32
    0x42 => { type=> PM_TYPE_32, sem=> PM_SEM_INSTANT },	# GAUGE32
    0x42 => { type=> PM_TYPE_U32, sem=> PM_SEM_INSTANT },	# UNSIGNED32
    0x43 => { type=> PM_TYPE_64, sem=> PM_SEM_COUNTER },	# TIMETICKS
    0x44 => { type=> PM_TYPE_STRING, sem=> PM_SEM_DISCRETE },	# OPAQUE
    0x46 => { type=> PM_TYPE_64, sem=> PM_SEM_COUNTER },	# COUNTER64
};

my $dom_rows = 0;	# this indom nr used for generic row numbers

my $pmda = PCP::PMDA->new('snmp', 56);

# Read in the config file(s)
#
sub load_config {
    my $db = shift;

    if (!defined $db->{hosts}) {
        $db->{hosts} = {};
    }
    if (!defined $db->{map}) {
        $db->{map} = {};
	$db->{map}{hosts} = [];
	$db->{map}{oids} = [];
    }

    for my $filename (@_) {
        my $fh = FileHandle->new($filename);
	if (!defined $fh) {
		warn "opening $filename $!";
		next;
	}

        while(<$fh>) {
            chomp; s/\r//g;

            # strip whitespace at the beginning and end of the line
            s/^\s+//;
            s/\s+$//;

            # strip comments
            s/^#.*//;       # line starts with a comment char
            s/[^\\]#.*//;   # non quoted comment char

            if (m/^$/) {
                # empty lines, or lines that were all comment
                next;
            }

            if (m/^set\s+(\w+)\s+(.*)$/) {
                # set an option
                my $key = $1;
                my $val = $2;

                $option->{$key}=$val;
            } elsif (m/^host\s+(\S+)\s+(.*)$/) {
		my $e = {};
		$e->{hostname}=$1;
		$e->{community}=$2;

		# The reversed dotted hostname is used in the metric name
		$e->{revname} = join('.',reverse(split('\.',$1)));

		# TODO - lazy create snmp sessions on first use
                my ($session,$error) = Net::SNMP->session(
                    -hostname =>$e->{hostname},
                    -community=>$e->{community},
                );
                if (!$session) {
                    warn("SNMP session to $e->{hostname}: $error");
                    $e->{error}=$error;
                } else {
                    $e->{snmp}=$session;
		    $e->{snmp}->translate([-timeticks=>0]);
                }
                $db->{hosts}{$1} = $e;
	        my $id = scalar @{$db->{map}{hosts}};
		# TODO - allow this pmid 'index base' to be set
		$e->{id} = $id;
		@{$db->{map}{hosts}}[$id]=$e;
            } elsif (m/^map\s+(single|column)\s+(\S+)\s+(\S+)\s+(\S+)\s+(.*)$/) {
                my $snmptype = $snmptype2val->{$3};
                if (!defined $snmptype) {
                    warn("Invalid SNMP type '$3' on oid '$2'\n");
                    next;
                }
                my $e = {};
                my $id = $4;
                if ($id eq '+') {
                    # select the next available number
                    $id = scalar @{$db->{map}{oids}};
                }
		if ($id > $option->{pmid_per_host}) {
		    warn("More metrics than allowed by pmid_per_host");
		    next;
		}
                $e->{type}=$1;
		$e->{oid}=$2;
		$e->{snmptype}=$snmptype;
		$e->{id}=$id;
		$e->{text}=$5;
		@{$db->{map}{oids}}[$id]=$e;
            } else {
                warn("Unrecognised config line: $_\n");
            }
            # TODO - add map tree, mib load
        }
    }

    $db->{max}{hosts} = scalar keys %{$db->{hosts}};
    $db->{max}{oids} = scalar @{$db->{map}{oids}};
    $db->{max}{static} = $db->{max}{hosts} * $option->{pmid_per_host};
    # any PMID above max static is available for dynamicly created mappings

    return $db;
}

# Create the fake generic rows indom
# TODO - demand create the rows indoms
sub db_create_indom {
    my ($db) = @_;

    my @dom;
    for my $row (0..$option->{max_row}) {
	# first is id, second is string description
	# for now, both are the same
	# TODO - populate the indom with rational names from an SNMP column
        push @dom,$row,$row;
    }
    $pmda->add_indom($dom_rows,\@dom,'SNMP rows','');
}

# Using the mappings, define all the metrics
#
sub db_add_metrics {
    my ($db) = @_;

    # TODO - nuke the PMDA.xs current list of metrics here
    # (there is a clear_metrics() in the xs that might be adapted to work)

    # add our version
    $pmda->add_metric(pmda_pmid(0,0), PM_TYPE_STRING,
        PM_INDOM_NULL, PM_SEM_DISCRETE,
        pmda_units(0,0,0,0,0,0), "snmp.version", '', '');

    for my $host (@{$db->{map}{hosts}}) {
	# calculate the pmid for the first metric for this host
	my $hostbase = $host->{id} * $option->{pmid_per_host};

	# skip hosts that did not setup their snmp session
	next if (!$host->{snmp});

        for my $e (@{$db->{map}{oids}}) {
            # for each predefined static mapping, register a metric

            if (!defined $e) {
                next;
            }
            my $id = $hostbase + $e->{id};

            # hack around the too transparent opaque datatype
            my $cluster = int($id /1024);
            my $item = $id %1024;

            my $type = $snmptype2pcp->{$e->{snmptype}};
            if (!defined $type) {
                warn("Unknown type=$type for id=$e->{id}\n");
                next;
            }

            my $indom;
            if ($e->{type} eq 'single') {
                $indom = PM_INDOM_NULL;
            } elsif ($e->{type} eq 'column') {
                # TODO - use metric specific indom, for now, just use generic
                $indom = $dom_rows;
                $e->{indom} = $indom;
            } else {
                warn("Unknown map type = $e->{type}\n");
                next;
            }
            $pmda->add_metric(pmda_pmid($cluster,$item),
                $type->{type},
                $indom, $type->{sem},
                pmda_units(0,0,0,0,0,0),
                'snmp.host.'.$host->{revname}.'.'.$e->{oid}, $e->{text}, ''
            );
	}
    }
}

# debug when fetch is called
# fetch_func is called with no params during a "fetch", after refreshing the
# PMNS before calling refresh_func
#
sub fetch {
    if ($option->{debug}) {
	$pmda->log("fetch");
    }
}

# debug when instance is called
# instance_func is called with "indom" param during a "instance", after
# refreshing the PMNS before calling pmdaInstance
#
sub instance {
    my ($indom) = @_;
    if ($option->{debug}) {
	$pmda->log("instance $indom");
    }
}

sub fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $id = $cluster*1024 + $item;

    if ($option->{debug}) {
        my $metric_name = pmda_pmid_name($cluster, $item);
	$pmda->log("fetch_callback $metric_name $cluster:$item ($inst)");
    }

    if ($id == 0) {
        return ($VERSION,1);
    }

    my $hostnr = int($id / $option->{pmid_per_host});
    my $host = @{$db->{map}{hosts}}[$hostnr];
    if (!defined $host) {
        return (PM_ERR_NOTHOST, 0);
    }

    my $map = @{$db->{map}{oids}}[$id % $option->{pmid_per_host}];
    if (!defined $map) {
        return (PM_ERR_PMID, 0);
    }
    my $oid = $map->{oid};

    if (defined $map->{indom}) {
	# only metrics with rows have an indom
        $oid.='.'.$inst;
    }

    # TODO - maybe check if a map single has been called with an inst other
    # than PM_INDOM_NULL

    if ($option->{debug}) {
	$pmda->log("fetch_callback hostnr=$hostnr rownr=$inst");
    }

    if (!defined $host->{snmp}) {
	# We have no snmp object for this host
        # FIXME - a better errno?
        return (PM_ERR_EOF, 0);
    }
    my $snmp = $host->{snmp};

    my $result = $snmp->get_request(
        -varbindlist=>[
            $oid,
        ]
    );

    if (!$result) {
	# We didnt get a valid snmp response
        return (PM_ERR_PMID, 0);
    }

    my $types = $snmp->var_bind_types();
    if ($map->{snmptype} != $types->{$oid}) {
        return (PM_ERR_CONV, 0);
    }
    return ($result->{$oid},1);
}

load_config($db,
	pmda_config('PCP_PMDAS_DIR').'/snmp/snmp.conf',
#	'snmp.conf'
);

db_create_indom($db);

db_add_metrics($db);

$pmda->set_fetch(\&fetch);
$pmda->set_instance(\&instance);
$pmda->set_fetch_callback(\&fetch_callback);

if ($option->{debug}) {
    $pmda->log("db=".Dumper($db)."\n");
    $pmda->log("option=".Dumper($option)."\n");
}
$pmda->set_user('pcp');
$pmda->run;

=pod

=head1 NAME

pmdasnmp - Gateway from SNMP to PCP (PMDA)

=head1 DESCRIPTION

B<pmdasnmp> is a Performance Metrics Domain Agent (PMDA) which
provides a generic gateway from PCP queries from a PCP client to SNMP queries
to one or more SNMP agents.

=head1 INSTALLATION

If you want access to the SNMP gateway performance
metrics, do the following as root:

	# cd $PCP_PMDAS_DIR/snmp
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/snmp
	# ./Remove

B<pmdasnmp> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 CONFIGURATION

TODO: define config file format here - map/set/host/... etc

=head1 FILES

=over

=item $PCP_PMDAS_DIR/snmp/snmp.conf

optional configuration file for B<pmdasnmp>

=item $PCP_PMDAS_DIR/snmp/Install

installation script for the B<pmdasnmp> agent

=item $PCP_PMDAS_DIR/snmp/Remove

undo installation script for the B<pmdasnmp> agent

=item $PCP_LOG_DIR/pmcd/snmp.log

default log file for error and warn() messages from B<pmdasnmp>

=back

=head1 SEE ALSO

pmcd(1) and SNMP
