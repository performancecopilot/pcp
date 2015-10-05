package Slurm::Hostlist;
use strict;
use warnings;

use Net::Domain qw( hostname );

sub create {
    my $self = {};
    bless $self;
    return $self;
};

sub count {
    return 1;
};

sub find {
    return 0;
};

1;
