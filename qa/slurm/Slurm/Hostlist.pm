package Slurm::Hostlist;
use strict;
use warnings;

use Net::Domain qw( hostname );

my $numcalls = 0;

sub create {
    my $self = {};
    bless $self;
    return $self;
};

sub shift {
    if ( $numcalls == 0){
        $numcalls = 1;
        return "cpn-d07-04-01";
    }
    else{
        return undef;
    }
};

sub count {
    return 1;
};

sub find {
    return 0;
};

1;
