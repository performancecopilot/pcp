#! /bin/sh
#
# Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
#

#
# weblogconv: convert weblog.conf files to posix regex
#

# only needed on Linux

progname=$0

if [ $# -gt 0 -a "$1" = "-?" ]; then
    echo "Usage: $progname infile [outfile]"
    exit 0
fi

if [ $# -gt 2 ]; then
    echo "$progname: Too many arguments."
    exit 1
fi

if [ $# -lt 1 ] ; then
    infile=""
else	
    infile=$1
fi

if [ $# -lt 2 ]; then
    outfile=""
else
    outfile="> $2"
fi

if [ -n "$infile" -a ! -r "$infile" ]; then
    echo "$progname: cannot read $infile"
    exit 1
fi

sed \
    -e '/)\$[01]/!s/^regex[ \t][ \t]*\([^ \t][^ \t]*\)[ \t][ \t]*/regex_posix \1 - /' \
    -e 's/^regex[ \t][ \t]*\([^ \t][^ \t]*\)[ \t][ \t]*\(.*$0.*$1.*\)/regex_posix \1 method,size \2/' \
    -e 's/^regex[ \t][ \t]*\([^ \t][^ \t]*\)[ \t][ \t]*\(.*$1.*$0.*\)/regex_posix \1 size,method \2/' \
    -e 's/)$0/)/g' \
    -e 's/)$1/)/g' $infile | eval cat $outfile

exit 0
