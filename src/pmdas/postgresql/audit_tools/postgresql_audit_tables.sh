#! /bin/sh
#
# Copyright (c) 2018 Red Hat.
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
# 
# This script prints the column names for each of the  postgresql pg_stat
# tables that are probed by the PCP pmdapostgresql(1) PMDA. To audit and
# verify a particular version of postgresql for compatibility with the PCP
# PMDA, run this script and diff the output with the qualified listing
# that most closely matches the version of postgresql server that you are
# using, e.g. postgres_pg_stat_tables.9.6.7 in this directory is currently
# the highest version supported. Any differences need to be investigated
# because the PMDA is sensitive to the order of columns in each of the
# tables that it probes (this may be relaxed some day by using more specific
# select commands that name each column).
#
# Note: this script assumes you already have the postgresql service enabled,
# configured and started. If not, consult some instructions on how to do that,
# such as https://fedoramagazine.org/postgresql-quick-start-fedora-24/

echo '#version' `sudo -u postgres psql -V`

for table in \
pg_stat_activity \
pg_stat_bgwriter \
pg_stat_database \
pg_stat_database_conflicts \
pg_stat_replication \
pg_stat_all_tables \
pg_stat_sys_tables \
pg_stat_user_tables \
pg_stat_all_indexes \
pg_stat_sys_indexes \
pg_stat_user_indexes \
pg_statio_all_tables \
pg_statio_sys_tables \
pg_statio_user_tables \
pg_statio_all_indexes \
pg_statio_sys_indexes \
pg_statio_user_indexes \
pg_statio_all_sequences \
pg_statio_sys_sequences \
pg_statio_user_sequences \
pg_stat_user_functions \
pg_stat_xact_user_functions \
pg_stat_xact_all_tables \
pg_stat_xact_sys_tables \
pg_active \
pg_stat_xact_user_tables
do
    sudo -u postgres psql -c "select * from $table" |\
    head -1 | awk -F '|' 'BEGIN {print("#table '$table'")}
    {for(i=1; i <= NF; i++) {printf("%03d '$table' %s\n", i-1, $i); }}' |\
    sed -e 's/ [ ]*/ /g'
done

exit 0
