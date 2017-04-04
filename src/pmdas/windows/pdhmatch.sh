#!/bin/sh
#
# take the output from pdhlist.exe
#	- remove hostname
#	- collapse known instance domains to a symbolic representation
#	- match up against patterns in data.c
#

#debug# tmp=/var/tmp/$$
#debug# trap "rm -f $tmp.*; exit 0" 0 1 2 3 15
tmp=`pwd`/tmp

# Examples of instance domains to collapse from pdhlist.exe
# output
#
# SQLServer:Buffer Partition(0)\Free pages
# Job Object Details(Winlogon Job 0-57c89/logon.scr)\% User Time
# Job Object Details(Winlogon Job 0-57c89/_Total)\% User Time
# Job Object Details(WmiProviderSubSystemHostJob/wmiprvse)\% User Time
# Job Object Details(WmiProviderSubSystemHostJob/_Total)\% User Time
# [not] Job Object Details(_Total/_Total)\% User Time 
# Job Object(WmiProviderSubSystemHostJob)\Current % User Mode Time
# [not] Job Object(_Total)\Current % User Mode Time
# Thread(Idle/0)\Context Switches/sec
# Thread(csrss/0#1)\Context Switches/sec
# LogicalDisk(C:)\% Free Space
# [not] LogicalDisk(_Total)\% Free Space
# Network Interface(Intel[R] PRO_1000 MT Dual Port Network # Connection _2)\Bytes Received/sec
# PhysicalDisk(0 C:)\% Disk Read Time
# [not] PhysicalDisk(_Total)\% Disk Read Time
# Print Queue(Canon LBP-3260 PCL6 on SCRIBE (from LUKE) in session 1)\Add Network Printer Calls
# [not] Print Queue(_Total)\Add Network Printer Calls
# Process(Idle)\% Privileged Time
# [not] Process(_Total)\% Privileged Time
# Processor(0)\% Idle Time
# [not] Processor(_Total)\% Idle Time
# RAS Port(LPT1)\Alignment Errors
# SQLServer:Databases(alice)\Active Transactions
# [not] SQLServer:Databases(_Total)\Active Transactions
# SQLServer:Locks(Database)\Average Wait Time (ms)
# [not] SQLServer:Locks(_Total)\Lock Requests/sec
# Server Work Queues(3)\Active Threads
# SQLServer:Cache Manager(Adhoc Sql Plans)\Cache Hit Ratio
# Terminal Services Session(Console)\% Privileged Time
#

if [ $# -eq 1 ]
then
    cat $1
elif [ $# -eq 0 ]
then
    cat
else
    echo "Usage: $0 [output-file-from-pdhlist]" >&2
    exit 1
fi \
| dos2unix \
| sed >$tmp.tmp \
    -e 's/^\\\\[^\]*\\//' \
    -e '/^SQLServer:Buffer Partition(/s/([0-9]*)\\/(<n>)\\/' \
    -e '/^Job Object Details(/{
/(_Total\//!s/(.*\/_Total)\\/(<job>\/_Total)\\/
/\/_Total)/!s/(.*\/.*)\\/(<job>\/<?>)\\/
}' \
    -e '/^Job Object(/{
/(_Total)/!s/(.*)\\/(<job>)\\/
}' \
    -e '/^Thread(/{
s/([^/]*\/[0-9]*)\\/(<name>\/<pid>)\\/
s/([^/]*\/[0-9]*#[0-9]*)\\/(<name>\/<pid>#<tid>)\\/
}' \
    -e '/^LogicalDisk(/s/([A-Z]:)\\/(<drive>)\\/' \
    -e '/^Network Interface(/s/([^)]*)\\/(<if>)\\/' \
    -e '/^PhysicalDisk(/s/([0-9][0-9]* [A-Z]:)\\/(<dev>)\\/' \
    -e '/^Print Queue(/{
/(_Total)/!s/(.*)\\/(<queue>)\\/
}' \
    -e '/^Process(/{
/(_Total)/!s/(.*)\\/(<pname>)\\/
}' \
    -e '/^Processor(/{
/(_Total)/!s/(.*)\\/(<cpu>)\\/
}' \
    -e '/^RAS Port(/s/(.*)\\/(<port>)\\/' \
    -e '/^SQLServer:Databases(/{
/(_Total)/!s/(.*)\\/(<db>)\\/
}' \
    -e '/^SQLServer:Locks(/{
/(_Total)/!s/(.*)\\/(<type>)\\/
}' \
    -e '/^Server Work Queues(/s/(.*)\\/(<queue>)\\/' \
    -e '/^SQLServer:Cache Manager(/s/(.*)\\/(<cache>)\\/' \
    -e '/^Terminal Services Session(/s/(.*)\\/(<tty>)\\/'

# This step tries to deal with this class of cases ...
# pdhlist reports stuff like
#	SQLServer:Locks\Average Wait Time (ms)
#	SQLServer:Locks(_Total)\Average Wait Time (ms)
#	SQLServer:Locks(*/*#*)\Average Wait Time (ms)
# but the first one is in fact bogus (only the second 2 forms
# can be looked up.
#
sed <$tmp.tmp \
    -e '/^.NET CLR Exceptions\\/d' \
    -e '/^.NET CLR Interop\\/d' \
    -e '/^.NET CLR Jit\\/d' \
    -e '/^.NET CLR Loading\\/d' \
    -e '/^.NET CLR LocksAndThreads\\/d' \
    -e '/^.NET CLR Memory\\/d' \
    -e '/^.NET CLR Remoting\\/d' \
    -e '/^.NET CLR Security\\/d' \
    -e '/^NBT Connection\\/d' \
    -e '/^Paging File\\/d' \
    -e '/^SQLServer:User Settable\\/d' \
    -e '/^Server Work Queues\\/d' \
    -e '/^SQLServer:Buffer Partition\\/d' \
    -e '/^Job Object Details\\/d' \
    -e '/^Job Object\\/d' \
    -e '/^Thread\\/d' \
    -e '/^LogicalDisk\\/d' \
    -e '/^Network Interface\\/d' \
    -e '/^PhysicalDisk\\/d' \
    -e '/^Print Queue\\/d' \
    -e '/^Process\\/d' \
    -e '/^Processor\\/d' \
    -e '/^RAS Port\\/d' \
    -e '/^SQLServer:Databases\\/d' \
    -e '/^SQLServer:Locks\\/d' \
| LC_COLLATE=POSIX sort \
| uniq >$tmp.munged

# extract patterns from PMDA source
#
if [ -f data.c ]
then
    sed -n <data.c \
	-e '/"\\\\/{
s/"[ 	]*$//
s/.*"//
s/\\\\/\\/g
s/^\\//
p
}' \
    | sed \
	-e '/^Network Interface(/s/(\*\/\*#\*)\\/(<if>)\\/' \
	-e '/^PhysicalDisk(/s/(\*\/\*#\*)\\/(<dev>)\\/' \
	-e '/^Processor(/s/(\*\/\*#\*)\\/(<cpu>)\\/' \
	-e '/^SQLServer:Locks(/s/(\*\/\*#\*)\\/(<type>)\\/' \
	-e '/^LogicalDisk(/s/(\*\/\*#\*)\\/(<drive>)\\/' \
    | LC_COLLATE=POSIX sort \
    | uniq >$tmp.pmda

else
    echo "Warning: no data.c, cannot match metrics up with PMDA patterns" >&2
    sed -e 's/^/? /' <$tmp.munged
fi

# match 'em up
#

comm -23 $tmp.pmda $tmp.munged >$tmp.tmp
if [ -s $tmp.tmp ]
then
    echo "============================================"
    echo "=== Warning: These current PMDA patterns do NOT match ANY metrics ..."
    echo "============================================"
    cat $tmp.tmp
    echo
fi

comm -12 $tmp.pmda $tmp.munged >$tmp.tmp
if [ -s $tmp.tmp ]
then
    echo "============================================"
    echo "=== Metrics supported in the current PMDA ..."
    echo "============================================"
    cat $tmp.tmp
else
    echo "============================================"
    echo "=== Warning: The current PMDA patterns match NO metric!"
    echo "============================================"
fi

comm -13 $tmp.pmda $tmp.munged >$tmp.tmp
if [ -s $tmp.tmp ]
then
    echo
    echo "============================================"
    echo "=== Metrics NOT supported in the current PMDA"
    echo "============================================"
    cat $tmp.tmp
fi
