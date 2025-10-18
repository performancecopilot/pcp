#
# Copyright (C) 2014-2015 Marko Myllynen <myllynen@redhat.com>
# Copyright (C) 2021 Raul Mahiques <rmahique@redhat.com>
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
use PCP::PMDA;
use POSIX;

my $have_ldap = eval {
  require Net::LDAP;
  Net::LDAP->import();
  use Net::LDAP::Util;
  1;
};
if (!$have_ldap) { die("Net::LDAP unavailable on this platform"); }

# Default values
our $aname = 'ds389';
our $server = 'localhost';
# Default LDAP version to use.
our $ldapver = 3;
our $binddn = 'cn=Directory Manager';
our $bindpw = 'Manager12';
# Default scope
our $dfscope  = 'base';
# Default LDAP filter
our $dffilter = '(objectclass=*)';
# Default LDAP attributes to retrieve
our $dattrs = ['*', '+'];
# How often it will check
our $query_interval = 2; # seconds
our $query_timeout = 1; # seconds
# Metrics defaults
our $mpm_type = PM_TYPE_U32;
our $mpm_indom = PM_INDOM_NULL;
our $mpm_sem = PM_SEM_INSTANT;
# Format: dim_space, dim_time, dim_count, scale_space, scale_time, scale_count
our $mpmda_units = '0,0,1,0,0,'.PM_COUNT_ONE;
our @add_met = ();
our %dclu;

# Default base and metrics
our %dataclusters = (
        '0' => ['0','cn=monitor','cn.',$dfscope,$dffilter,$dattrs],
        '1' => ['0','cn=monitor,cn=userRoot,cn=ldbm database,cn=plugins,cn=config','userroot.',$dfscope,$dffilter,$dattrs],
        '2' => ['0','cn=monitor,cn=changelog,cn=ldbm database,cn=plugins,cn=config','changelog_mon.',$dfscope,$dffilter,$dattrs],
        '3' => ['0','cn=snmp,cn=monitor','snmp_mon.',$dfscope,$dffilter,$dattrs]
);
our @def_met = (
        [0,0,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'threads'],
        [0,1,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'currentconnections'],
        [0,2,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'totalconnections'],
        [0,3,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'currentconnectionsatmaxthreads'],
        [0,4,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'maxthreadsperconnhits'],
        [0,5,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'dtablesize'],
        [0,6,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'readwaiters'],
        [0,7,PM_TYPE_U64,$mpm_indom,$mpm_sem,$mpmda_units,'opsinitiated'],
        [0,8,PM_TYPE_U64,$mpm_indom,$mpm_sem,$mpmda_units,'opscompleted'],
        [0,9,PM_TYPE_U64,$mpm_indom,$mpm_sem,$mpmda_units,'entriessent'],
        [0,10,PM_TYPE_U64,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','bytessent'],
        [0,11,$mpm_type,$mpm_indom,$mpm_sem,'0,1,0,0,'.PM_TIME_SEC.',0','uptime'],
        [0,12,PM_TYPE_STRING,$mpm_indom,PM_SEM_DISCRETE,'0,0,0,0,0,0','version'],
        [0,13,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'nbackends'],
        [1,0,$mpm_type,$mpm_indom,$mpm_sem,'0,0,0,0,0,0','readonly'],
        [1,1,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'entrycachehits'],
        [1,2,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'entrycachetries'],
        [1,3,PM_TYPE_U64,$mpm_indom,$mpm_sem,$mpmda_units,'entrycachehitratio'],
        [1,4,PM_TYPE_U64,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','currententrycachesize'],
        [1,5,PM_TYPE_U64,$mpm_indom,PM_SEM_DISCRETE,'1,0,0,'.PM_SPACE_BYTE.',0,0','maxentrycachesize'],
        [1,6,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'currententrycachecount'],
        [1,7,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'maxentrycachecount'],
        [1,8,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'dncachehits'],
        [1,9,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'dncachetries'],
        [1,10,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'dncachehitratio'],
        [1,11,$mpm_type,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','currentdncachesize'],
        [1,12,$mpm_type,$mpm_indom,PM_SEM_DISCRETE,'1,0,0,'.PM_SPACE_BYTE.',0,0','maxdncachesize'],
        [1,13,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'currentdncachecount'],
        [1,14,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'maxdncachecount'],
        [1,15,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'normalizeddncachehits'],
        [1,16,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'normalizeddncachetries'],
        [1,17,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'normalizeddncachehitratio'],
        [1,18,PM_TYPE_32,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','currentnormalizeddncachesize'],
        [1,19,PM_TYPE_32,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','maxnormalizeddncachesize'],
        [1,20,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'currentnormalizeddncachecount'],
        [1,21,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'normalizeddncachemisses'],
        [2,0,$mpm_type,$mpm_indom,$mpm_sem,'0,0,0,0,0,0','readonly'],
        [2,1,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'entrycachehits'],
        [2,2,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'entrycachetries'],
        [2,3,PM_TYPE_U64,$mpm_indom,$mpm_sem,$mpmda_units,'entrycachehitratio'],
        [2,4,PM_TYPE_U64,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','currententrycachesize'],
        [2,5,PM_TYPE_U64,$mpm_indom,PM_SEM_DISCRETE,'1,0,0,'.PM_SPACE_BYTE.',0,0','maxentrycachesize'],
        [2,6,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'currententrycachecount'],
        [2,7,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'maxentrycachecount'],
        [2,8,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'dncachehits'],
        [2,9,PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'dncachetries'],
        [2,10,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'dncachehitratio'],
        [2,11,$mpm_type,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','currentdncachesize'],
        [2,12,$mpm_type,$mpm_indom,PM_SEM_DISCRETE,'1,0,0,'.PM_SPACE_BYTE.',0,0','maxdncachesize'],
        [2,13,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'currentdncachecount'],
        [2,14,PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'maxdncachecount'],
        [3,0,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'anonymousbinds'],
        [3,1,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'unauthbinds'],
        [3,2,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'simpleauthbinds'],
        [3,3,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'strongauthbinds'],
        [3,4,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'bindsecurityerrors'],
        [3,5,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'inops'],
        [3,6,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'readops'],
        [3,7,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'compareops'],
        [3,8,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'addentryops'],
        [3,9,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'modifyentryops'],
        [3,10,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'modifyrdnops'],
        [3,11,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'listops'],
        [3,12,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'searchops'],
        [3,13,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'onelevelsearchops'],
        [3,14,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'wholesubtreesearchops'],
        [3,15,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'referrals'],
        [3,16,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'chainings'],
        [3,17,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'securityerrors'],
        [3,18,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'errors'],
        [3,19,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'connections'],
        [3,20,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'connectionseq'],
        [3,21,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'connectionsinmaxthreads'],
        [3,22,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'connectionsmaxthreadscount'],
        [3,23,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'bytesrecv'],
        [3,24,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'bytessent'],
        [3,25,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'entriesreturned'],
        [3,26,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'referralsreturned'],
        [3,27,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'masterentries'],
        [3,28,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'cacheentries'],
        [3,29,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'cachehits'],
        [3,30,$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'slavehits']
);

our @def_replagr_met = (
        [$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'nsds5ReplicaChangeCount'],
        [$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'nsds5replicareapactive']
);

our @def_repl_met = (
        [$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'nsds5replicareapactive'],
        [$mpm_type,$mpm_indom,$mpm_sem,'0,1,0,0,'.PM_TIME_SEC.',0','nsruvReplicaLastModified'],
        [$mpm_type,$mpm_indom,$mpm_sem,'0,1,0,0,'.PM_TIME_SEC.',0','nsds5replicaLastUpdateStart'],
        [$mpm_type,$mpm_indom,$mpm_sem,'0,1,0,0,'.PM_TIME_SEC.',0','nsds5replicaLastUpdateEnd'],
        [$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'nsds5replicaChangesSentSinceStartup'],
        [$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'replicaLastUpdateStatus'],
        [$mpm_type,$mpm_indom,$mpm_sem,'0,1,0,0,'.PM_TIME_SEC.',0','nsds5replicaLastInitStart'],
        [$mpm_type,$mpm_indom,$mpm_sem,'0,1,0,0,'.PM_TIME_SEC.',0','nsds5replicaLastInitEnd'],
        [$mpm_type,$mpm_indom,$mpm_sem,'0,1,0,0,'.PM_TIME_SEC.',0','nsds5replicaLastUpdateTime'],
        [$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'nsds5replicaUpdateInProgeress'],
        [$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'replicaChangesSkippedSinceStartup'],
        [$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'replicaChangesSentSinceStartup']
);

our @def_mon_met = (
        [$mpm_type,$mpm_indom,$mpm_sem,'0,0,0,0,0,0','readonly'],
        [PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'entrycachehits'],
        [PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'entrycachetries'],
        [PM_TYPE_U64,$mpm_indom,$mpm_sem,$mpmda_units,'entrycachehitratio'],
        [PM_TYPE_U64,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','currententrycachesize'],
        [PM_TYPE_U64,$mpm_indom,PM_SEM_DISCRETE,'1,0,0,'.PM_SPACE_BYTE.',0,0','maxentrycachesize'],
        [$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'currententrycachecount'],
        [PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'maxentrycachecount'],
        [PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'dncachehits'],
        [PM_TYPE_U64,$mpm_indom,PM_SEM_COUNTER,$mpmda_units,'dncachetries'],
        [$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'dncachehitratio'],
        [$mpm_type,$mpm_indom,$mpm_sem,'1,0,0,'.PM_SPACE_BYTE.',0,0','currentdncachesize'],
        [$mpm_type,$mpm_indom,PM_SEM_DISCRETE,'1,0,0,'.PM_SPACE_BYTE.',0,0','maxdncachesize'],
        [$mpm_type,$mpm_indom,$mpm_sem,$mpmda_units,'currentdncachecount'],
        [PM_TYPE_32,$mpm_indom,$mpm_sem,$mpmda_units,'maxdncachecount']
);


# Configuration files for overriding the above settings
for my $file (pmda_config('PCP_PMDAS_DIR') . "/$aname/$aname.conf", "./$aname.conf") {
  eval `cat $file` unless ! -f $file;
}

unless (!keys %dclu) {
  %dataclusters = (%dataclusters, %dclu)
}

use vars qw( $ldap $pmda %metrics );

sub ds389_connection_setup {
  if (!defined($ldap)) {
    if (!pmda_install()) { $pmda->log("binding to $server"); }
    $ldap = Net::LDAP->new($server, version => $ldapver, timeout => $query_timeout);
    if (!defined($ldap)) {
      if (!pmda_install()) { $pmda->log("bind failed, server down?"); }
      return;
    }
    my $mesg = $ldap->bind($binddn, password => $bindpw);
    if ($mesg->code) {
      $pmda->log("bind failed: " . $mesg->error);
      return;
    }
    $pmda->log("bind to $server ok");
  }
}

# Function copied from https://github.com/389ds/389-ds-base/blob/389-ds-base-1.3.10/ldap/admin/src/scripts/repl-monitor.pl.in
sub to_decimal_csn
{
  my ($maxcsn) = @_;
  if (!$maxcsn || $maxcsn eq "" || $maxcsn eq "Unavailable") {
    return "Unavailable";
  }

  my ($tm, $seq, $masterid, $subseq) = unpack("a8 a4 a4 a4", $maxcsn);

  $tm = hex($tm);
  $seq = hex($seq);
  $masterid = hex($masterid);
  $subseq = hex($subseq);

  return "$tm $seq $masterid $subseq";
}



sub ds389_time_to_epoch {
  my ($time) = @_;
  return mktime(substr($time,12,2),
                substr($time,10,2),
                substr($time,8,2),
                substr($time,6,2),
                substr($time,4,2) - 1,
                substr($time,0,4) - 1900);
}

sub ds389_process_entry {
  my ($entry, $prefix, $cluster) = @_;
  my $currtime;
  my $startrepltime = '';
  my $endrepltime = '';
  if ($entry && $entry->can('attributes')) {
    foreach my $attr ($entry->attributes) {
      my $value = $entry->get_value($attr);

      if ($attr eq 'currenttime') {
        $currtime = ds389_time_to_epoch($value);
        next;
      }

      if ($attr eq 'starttime') {
        my $starttime = ds389_time_to_epoch($value);
        $value = $currtime - $starttime;
        $attr = 'uptime';
      }

      if ($attr eq 'nsds5replicaLastUpdateStatus') {
        if ($value =~ /No replication sessions started since server startup/i) {
          $value = 30
        } elsif ($value =~ /agreement disabled/i) {
          $value = 31
        } elsif ($value =~ /Problem connecting to the replica/i) {
          $value = 20
        } else {
          $value = (split /\)/, (split /Error \(/, $value)[1])[0];
        }
        $attr = 'replicaLastUpdateStatus';
      }

      if ($attr eq 'nsds5replicaChangesSentSinceStartup' ) {
#       my $rep_id = (split /:/, $value)[0];
        my ($sent, $skipped) = (split /\//, (split /:/, $value)[1]);
#       $attr = 'replica'.$rep_id.'ChangesSentSinceStartup';
        $attr = 'replicaChangesSentSinceStartup';
        $metrics{"$aname." . $prefix . $attr} = $sent;
        $value = $skipped;
#       $attr = 'replica'.$rep_id.'ChangesSkippedSinceStartup';
        $attr = 'replicaChangesSkippedSinceStartup';
      }

      if ($attr eq 'nsds5replicaUpdateInProgress' ) {
        if ($value =~ /^(true|TRUE)$/) {
          $value = 1;
        } else {
          $value = 0;
        }
      }

      if ($attr =~ /^(nsds5replicaLastInitEnd|nsds5replicaLastUpdateStart|nsds5replicaLastInitStart)$/i ) {
        $value = ds389_time_to_epoch($value);
        if ($attr eq 'nsds5replicaLastUpdateStart') {
          $startrepltime = $value;
        }
        $metrics{"$aname." . $prefix . $attr} = $value;
      }

      if ($endrepltime ne '' && $startrepltime ne '' ) {
        $attr = 'nsds5replicaLastUpdateTime';
        $value = $endrepltime - $startrepltime;
        $startrepltime = $endrepltime = '';
      }

      if ($attr =~ /^(nsds5replicaLastUpdateEnd)$/i ) {
        $value = ds389_time_to_epoch($value);
        $endrepltime = $value;
      }

      if ($attr =~ /^nsds5AgmtMaxCSN$/i ) {
        my $maxcsn = &to_decimal_csn((split /\;/, $value)[5]);
        $value = (split / /, $maxcsn)[0];
      }

      $metrics{"$aname." . $prefix . $attr} = $value;
    }
  }
}

sub retrieve_ldap {
  my ($cluster, $ts, $base, $tname, $scope, $filter, $lattrs) = @_;
  my $mesg;
  
  if ((strftime("%s", localtime()) - $ts) > $query_interval) {
    $ts = strftime("%s", localtime());
    $mesg = $ldap->search(scope => $scope, base => $base, filter => $filter, attrs => $lattrs, timelimit => $query_timeout);
    if ($mesg->code) {
      $pmda->log("search(scope: \"$scope\", base: \"$base\", filter: \"$filter\", attrs: \"". join(' ', @$lattrs) ."\") failed: " . $mesg->error);
      undef $ldap;
      return;
    }
    ds389_process_entry($mesg->entry, $tname, $cluster);
  } 
}

sub ds389_fetch {
  if (!defined($ldap)) {
    ds389_connection_setup();
  }
  return unless defined($ldap);

  my ($cluster) = @_;
  my $mesg;

  \&retrieve_ldap($cluster,$dataclusters{$cluster}[0],$dataclusters{$cluster}[1],$dataclusters{$cluster}[2],$dataclusters{$cluster}[3],$dataclusters{$cluster}[4], $dataclusters{$cluster}[5]);
}

sub ds389_fetch_callback {
  my ($cluster, $item, $inst) = @_;

  if (!defined($ldap))        { return (PM_ERR_AGAIN, 0); }
  if ($inst != PM_INDOM_NULL) { return (PM_ERR_INST, 0); }

  my $pmnm = pmda_pmid_name($cluster, $item);
  my $value = $metrics{$pmnm};

  if (!defined($value))       { return (PM_ERR_APPVERSION, 0); }

  return ($value, 1);
}


sub ds389_simple_search {
  my ($scope, $base, $filter, $attrs) = @_;

  if (!defined($ldap)) { return; }
  my $mesg = $ldap->search(scope => $scope, base => $base, filter => $filter, attrs => $attrs);
  if ($mesg->code) {
    $pmda->log("search(scope: \"$scope\", base: \"$base\", filter: \"$filter\", attrs: \"". join(' ', @$attrs) ."\") failed: " . $mesg->error);
    undef $ldap;
    return;
  }
  return $mesg
}

sub push_to_met {
  my ($tc, @myarr) = @_;
  my $count = 0;
  foreach my $c (0 .. $#myarr) {
    push(@def_met, [$tc, $c, $myarr[$c][0], $myarr[$c][1], $myarr[$c][2], $myarr[$c][3], $myarr[$c][4]]);
  };
}

$pmda = PCP::PMDA->new($aname, 130);

# Add to the existing ones
my $topclu = 0;
foreach my $attr (keys %dataclusters) {
  if ($attr gt $topclu) {
    $topclu = $attr;
  }
};

ds389_connection_setup();

my $mesg = ds389_simple_search('sub','cn=config','objectclass=*',['nsslapd-defaultnamingcontext','nsslapd-backend']);

my $max = defined($mesg) ? $mesg->count : 0;
for ( my $i = 0 ; $i < $max ; $i++ ) {
  my $entry = $mesg->entry ( $i );
  foreach my $attr ($entry->attributes) {
    my $value = $entry->get_value($attr);
    my $value_short = $value =~ s/[,]*[a-zA-Z]*=/_/rgi;
    my $value_ldap = $value =~ s/=/\\3D/igr =~ s/,/\\2C/igr;
    if ($attr eq 'nsslapd-defaultnamingcontext') {
      $topclu++;
      $dataclusters{$topclu} = ['0',$value,$value_short . '.','sub','(&(nsuniqueid=ffffffff-ffffffff-ffffffff-ffffffff)(objectclass=nstombstone))',['nsds5agmtmaxcsn','nsds50ruv']];
      my $mesg2 = ds389_simple_search('sub',"cn=$value_ldap,cn=mapping tree,cn=config",'objectclass=nsds5replicationagreement',['cn']);
      my $max2 = $mesg2->count;
      for ( my $i2 = 0 ; $i2 < $max2 ; $i2++ ) {
        my $entry2 = $mesg2->entry ( $i2 );
        my $rplagr = $entry2->get_value('cn');
        $topclu++;
        my $value2_short = (split /\./, $rplagr)[0];
        $dataclusters{$topclu} = ['0',"cn=". $rplagr .",cn=replica,cn=". $value_ldap .",cn=mapping tree,cn=config",$value2_short . '.',$dfscope,$dffilter,$dattrs];
        push_to_met($topclu, @def_repl_met);
        $topclu++;
        $dataclusters{$topclu} = ['0',"cn=replica,cn=". $value_ldap .",cn=mapping tree,cn=config","rpl_". $value2_short . '.',$dfscope,$dffilter,$dattrs];
        push_to_met($topclu, @def_replagr_met);
      }
    }
    if (($attr eq 'nsslapd-backend') and ($value eq 'ipaca')) {
      $topclu++;
      $dataclusters{$topclu} = ['0',$value,$value_short . '.','sub','(&(nsuniqueid=ffffffff-ffffffff-ffffffff-ffffffff)(objectclass=nstombstone))',['nsds5agmtmaxcsn','nsds50ruv']];
      my $mesg2 = ds389_simple_search('sub',"cn=o\\3D$value_ldap,cn=mapping tree,cn=config",'objectclass=nsds5replicationagreement',['cn']);
      my $max2 = $mesg2->count;
      for ( my $i2 = 0 ; $i2 < $max2 ; $i2++ ) {
        my $entry2 = $mesg2->entry ( $i2 );
        my $rplagr = $entry2->get_value('cn');
        $topclu++;
        my $value2_short = (split /\./, $rplagr)[0];
        $dataclusters{$topclu} = ['0',"cn=". $rplagr .",cn=replica,cn=o\\3D". $value_ldap .",cn=mapping tree,cn=config",$value2_short . '.',$dfscope,$dffilter,$dattrs];
        push_to_met($topclu, @def_repl_met);
        $topclu++;
        $dataclusters{$topclu} = ['0',"cn=replica,cn=o\\3D". $value_ldap .",cn=mapping tree,cn=config","rpl_". $value2_short . '.',$dfscope,$dffilter,$dattrs];
        push_to_met($topclu, @def_replagr_met);
        $topclu++;
        $dataclusters{$topclu} = ['0',"cn=monitor,cn=$value,cn=ldbm database,cn=plugins,cn=config",$value ."_mon.",$dfscope,$dffilter,$dattrs];
        push_to_met($topclu, @def_mon_met);
      }
    }
  };
};  

# Add default metrics
while (my ($i, @met) = each @def_met) {
  if (defined($dataclusters{$def_met[$i][0]})) {
    $pmda->add_metric(pmda_pmid($def_met[$i][0],$def_met[$i][1]), $def_met[$i][2], $def_met[$i][3],$def_met[$i][4], pmda_units(split(',',$def_met[$i][5])),"$aname.$dataclusters{$def_met[$i][0]}[2]$def_met[$i][6]", '', '');
  }
};

# Add metrics from the configuration file
while (my ($i, @met) = each @add_met) {
  if (defined($dataclusters{$add_met[$i][0]})) {
    $pmda->add_metric(pmda_pmid($add_met[$i][0],$add_met[$i][1]), $add_met[$i][2], $add_met[$i][3],$add_met[$i][4], pmda_units(split(',',$add_met[$i][5])),"$aname.$dataclusters{$add_met[$i][0]}[2]$add_met[$i][6]", '', '');
  }
};

$pmda->set_refresh(\&ds389_fetch);
$pmda->set_fetch_callback(\&ds389_fetch_callback);
$pmda->set_user('pcp');
$pmda->run;
