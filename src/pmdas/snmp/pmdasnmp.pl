#
# Copyright (c) 2009 Aconex.  All Rights Reserved.
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

our $VERSION='0.2';
my $db = {};
my $option = {};

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
    0x02 => PM_TYPE_32,		# INTEGER32
    0x04 => PM_TYPE_STRING,	# OCTET_STRING
    0x06 => PM_TYPE_STRING,	# OBJECT_IDENTIFIER
    0x40 => PM_TYPE_STRING,	# IPADDRESS
    0x41 => PM_TYPE_32,		# COUNTER32
    0x42 => PM_TYPE_32,		# GAUGE32
    0x42 => PM_TYPE_U32,	# UNSIGNED32
    0x43 => PM_TYPE_64,		# TIMETICKS
    0x44 => PM_TYPE_STRING,	# OPAQUE
    0x46 => PM_TYPE_64,		# COUNTER64
};

my $pmda = PCP::PMDA->new('snmp', 56);

# Read in the config file(s)
#
sub load_config {
    my $db = shift;

    for my $filename (@_) {
        my $fh = FileHandle->new($filename) or warn "opening $filename $!";

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
		$e->{id}=$1;
		$e->{community}=$2;

                my ($session,$error) = Net::SNMP->session(
                    -hostname =>$e->{id},
                    -community=>$e->{community},
                );
                if (!$session) {
                    warn("SNMP session to $e->{id}: $error");
                    $e->{error}=$error;
                } else {
                    $e->{snmp}=$session;
		    $e->{snmp}->translate([-timeticks=>0]);
                }
                $db->{hosts}{$1} = $e;
            } elsif (m/^map\s+(\S+)\s+(\S+)\s+(\S)\s+(.*)$/) {
                if (!defined $snmptype2val->{$2}) {
                    warn("Invalid SNMP type '$2' on oid '$1'\n");
                    next;
                }
                my $e = {};
		$e->{oid}=$1;
		$e->{type}=$snmptype2val->{$2};
		$e->{id}=$3;
		$e->{text}=$4;
		@{$db->{map}{static}}[$3]=$e;
            }
            # TODO - add map tree, mib load, maxstatic
        }
    }
    return $db;
}

# Using the hosts data, create a list of instance domains
#
sub hosts_indom {
	my ($db) = @_;

	my @dom;
	my $i=0;

	for my $e (values %{$db->{hosts}}) {
		push @dom,$i,$e->{id};
		@{$db->{map}{hosts}}[$i] = $e;
		$i++;
	}
	return \@dom;
}

# Using the mappings, define all the metrics
#
sub db_add_metrics {
    my ($db) = @_;

    #$pmda->clear_metrics();

    # add our version
    $pmda->add_metric(pmda_pmid(0,0), PM_TYPE_STRING,
        PM_INDOM_NULL, PM_SEM_DISCRETE,
        pmda_units(0,0,0,0,0,0), "snmp.version", '', '');

    for my $e (@{$db->{map}{static}}) {
        if (!defined $e) {
            next;
        }
	# hack around the too transparent opaque datatype
	my $cluster = $e->{id} /1024;
	my $item = $e->{id} %1024;
        $pmda->add_metric(pmda_pmid($cluster,$item),
            $snmptype2pcp->{$e->{type}},
            0, PM_SEM_INSTANT,
            pmda_units(0,0,0,0,0,0),
            'snmp.oid.'.$e->{oid}, $e->{text}, ''
        );
    }
}

# debug when fetch is called
# fetch_func is called with no params during a "fetch", after refreshing the
# PMNS befure calling refresh_func
#
sub fetch {
    if ($option->{debug}) {
	$pmda->log("fetch\n");
    }
}

# debug when instance is called
# instance_func is called with "indom" param during a "instance", after
# refreshing the PMNS befure calling pmdaInstance
#
sub instance {
    my ($indom) = @_;
    if ($option->{debug}) {
	$pmda->log("instance $indom\n");
    }
}

# refresh_func is called with "clustertab[index]"
# in "refresh", "fetch",

# input_cb_func is called with "data", "string"
# in ?

# store_cb_func is called with params
# in "store_callback"

# actually fetch a metric
# fetch_cb_func is called with "cluster", "item", "inst" in "fetch_callback"
sub fetch_callback
{
    my ($cluster, $item, $inst) = @_;
    my $id = $cluster*1024 + $item;

    if ($option->{debug}) {
        my $metric_name = pmda_pmid_name($cluster, $item);
	$pmda->log("fetch_callback $metric_name $cluster:$item ($inst)\n");
    }

    if ($id == 0) {
        return ($VERSION,1);
    }
    if ($id == 2) {
        return (1,1);
    }

    if ($inst == PM_IN_NULL)	{ return (PM_ERR_INST, 0); }

    my $map = @{$db->{map}{static}}[$id];
    if (!defined $map) {
        return (PM_ERR_PMID, 0);
    }
    my $oid = $map->{oid};

    my $host = @{$db->{map}{hosts}}[$inst];
    if (!defined $host) {
        return (PM_ERR_NOTHOST, 0);
    }
    if (!defined $host->{snmp}) {
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
        # FIXME - a better errno?
        return (PM_ERR_IPC, 0);
    }

    my $types = $snmp->var_bind_types();
    if ($map->{type} != $types->{$oid}) {
        return (PM_ERR_CONV, 0);
    }
    return ($result->{$oid},1);
}

load_config($db,
	pmda_config('PCP_PMDAS_DIR').'/snmp/snmp.conf',
	'snmp.conf'
);

#add_indom(self,indom,list,help,longhelp)
$pmda->add_indom(0,hosts_indom($db),'SNMP hosts','');

db_add_metrics($db);

$pmda->set_fetch(\&fetch);
$pmda->set_instance(\&instance);
$pmda->set_fetch_callback(\&fetch_callback);

if ($option->{debug}) {
    $pmda->log("db=".Dumper($db)."\n");
    $pmda->log("option=".Dumper($option)."\n");
}
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

	# cd $PCP_PMDAS_DIR/bonding
	# ./Install

If you want to undo the installation, do the following as root:

	# cd $PCP_PMDAS_DIR/bonding
	# ./Remove

B<pmdasnmp> is launched by pmcd(1) and should never be executed
directly.  The Install and Remove scripts notify pmcd(1) when
the agent is installed or removed.

=head1 FILES

=over

=item $PCP_PMDAS_DIR/snmp/snmp2pcp.conf

optional configuration file for B<pmdasnmp>

snmp PMDA for PCP

=over

=item $PCP_PMDAS_DIR/snmp/Install

installation script for the B<pmdasnmp> agent

=item $PCP_PMDAS_DIR/snmp/Remove

undo installation script for the B<pmdasnmp> agent

=item $PCP_LOG_DIR/pmcd/snmp.log

default log file for error messages from B<pmdasnmp>

=back

=head1 SEE ALSO

pmcd(1) and snmp
