#!/bin/sh
#
# Copyright (c) 2026 Red Hat.
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
# Rebuild the pmsearch full-text search index from running PMCD
# or a PCP archive.  Typically invoked nightly via systemd timer
# or cron.
#

. $PCP_DIR/etc/pcp.env

status=1
tmp=`mktemp -d "$PCP_TMPFILE_DIR/pmsearch_daily.XXXXXXXXX"` || exit 1
trap "rm -rf \"$tmp\" \"\$NEW\"; exit \$status" 0 1 2 3 15

prog=`basename $0`
INDEX="$PCP_VAR_DIR/lib/pmsearch.index"

cat > $tmp/usage << EOF
Options:
  -a=ARCHIVE,--archive=ARCHIVE   use archive instead of running PMCD
  -N,--showme           dry-run, show what would be done
  -o=INDEX,--output=INDEX   output index file path [default: $INDEX]
  -V,--verbose          verbose diagnostics
  --help
EOF

_usage()
{
    pmgetopt --progname=$prog --config=$tmp/usage --usage
    exit 1
}

ARCHIVE=""
VERBOSE=false
SHOWME=false

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"` || exit 1
eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-a)	ARCHIVE="$2"; shift ;;
	-N)	SHOWME=true ;;
	-o)	INDEX="$2"; shift ;;
	-V)	VERBOSE=true ;;
	-\?)	_usage ;;
	--)	shift; break ;;
    esac
    shift
done

# Build the pminfo/pmprobe flags for archive or live mode
if [ -n "$ARCHIVE" ]
then
    SRCFLAGS="-a $ARCHIVE"
else
    SRCFLAGS=""
fi

if $VERBOSE
then
    echo "$prog: index target: $INDEX"
fi

# Build metric→indom mapping first (needed for both metrics and instances).
pminfo $SRCFLAGS -d 2>/dev/null | $PCP_AWK_PROG '
/^[a-zA-Z]/ { metric = $1 }
/InDom:/ && !/PM_INDOM_NULL/ {
    for (i = 1; i <= NF; i++) {
	if ($i == "InDom:") {
	    print metric "\t" $(i+1)
	    break
	}
    }
}' > $tmp/metric_indom

# Extract all metric help text in newhelp format.
# Uses "@ M name<TAB>indom<TAB>oneline" for metrics with indoms,
# so newhelp -S can resolve indoms without needing PMNS/PMCD.
pminfo $SRCFLAGS -tT 2>/dev/null | $PCP_AWK_PROG '
FILENAME == ARGV[1] {
    split($0, a, "\t")
    metric_indom[a[1]] = a[2]
    next
}
/^[a-zA-Z][a-zA-Z0-9_]*\.[a-zA-Z0-9_.]+ / {
    if (index($0, "One-line Help: Error:") > 0) {
	skip = 1
	next
    }
    skip = 0
    name = $1
    oneline = ""
    start = index($0, "[")
    end = index($0, "]")
    if (start > 0 && end > start) {
	oneline = substr($0, start + 1, end - start - 1)
    }
    if (name in metric_indom)
	printf "@ M %s\t%s\t%s\n", name, metric_indom[name], oneline
    else
	printf "@ M %s\t\t%s\n", name, oneline
    next
}
/^Help:$/ { next }
/^Full Help: Error:/ { skip = 1; next }
skip { next }
{
    if (substr($0, 1, 1) == "@")
	printf " %s\n", $0
    else
	print
}
' "$tmp/metric_indom" - > $tmp/helptext

nhelplines=`wc -l < $tmp/helptext`
if [ "$nhelplines" -eq 0 ]
then
    echo >&2 "$prog: warning: no metrics found"
    status=0
    exit
fi

if $VERBOSE
then
    nmetrics=`grep -c '^@ ' $tmp/helptext`
    echo "$prog: extracted $nmetrics metrics ($nhelplines lines)"
fi

# Extract instance names.
# Deduplicate by (indom, instance_name) since many metrics share indoms.
pmprobe $SRCFLAGS -I 2>/dev/null > $tmp/pmprobe_output

$PCP_AWK_PROG '
FILENAME == ARGV[1] {
    split($0, a, "\t")
    metric_indom[a[1]] = a[2]
    next
}
{
    metric = $1
    count = $2 + 0
    if (count <= 0 || $2 == "PM_IN_NULL") next

    indom = metric_indom[metric]
    if (indom == "") next

    rest = $0
    while (match(rest, /"[^"]*"/)) {
	inst = substr(rest, RSTART + 1, RLENGTH - 2)
	key = indom SUBSEP inst
	if (!(key in seen)) {
	    seen[key] = 1
	    inst_data[++ninst] = indom "\t" inst
	}
	rest = substr(rest, RSTART + RLENGTH)
    }
}
END {
    for (i = 1; i <= ninst; i++)
	printf "@ I %s\n\n", inst_data[i]
}
' "$tmp/metric_indom" "$tmp/pmprobe_output" >> $tmp/helptext

if $VERBOSE
then
    ninst=`grep -c '^@ I ' $tmp/helptext`
    echo "$prog: extracted $ninst instances"
fi

if $SHOWME
then
    echo "$prog: would copy $PCP_SHARE_DIR/lib/pmsearch.index to $INDEX and add runtime data"
    status=0
    exit
fi

# Ensure output directory exists
outdir=`dirname "$INDEX"`
if [ ! -d "$outdir" ]
then
    mkdir -p "$outdir" 2>/dev/null
    if [ ! -d "$outdir" ]
    then
	echo >&2 "$prog: cannot create directory $outdir"
	exit
    fi
fi

# Start from the build-time base index (contains all metric help text).
# The nightly update copies it, then adds/updates runtime data.
NEW="$INDEX.new.$$"
BASE="$PCP_SHARE_DIR/lib/pmsearch.index"
if [ -f "$BASE" ]
then
    cp "$BASE" "$NEW" || exit
    if $VERBOSE
    then
	echo "$prog: copied base index from $BASE"
    fi
fi

nentries=`grep -c '^@ ' $tmp/helptext`
if [ "$nentries" -eq 0 ]
then
    if $VERBOSE
    then
	echo "$prog: no runtime data to add"
    fi
    status=0
    exit
fi

# Add/update all runtime data (metrics and instances) into the index.
# Metrics already in the base index are updated; new metrics from
# PMDAs without help files (e.g. Python PMDAs) are inserted.
if $PCP_BINADM_DIR/newhelp -S -o "$NEW" $tmp/helptext
then
    if mv -f "$NEW" "$INDEX"
    then
	if $VERBOSE
	then
	    nmetrics=`grep -c '^@ [^I]' $tmp/helptext`
	    ninst=`grep -c '^@ I ' $tmp/helptext`
	    echo "$prog: added $nmetrics metrics and $ninst instances to $INDEX"
	fi
	status=0
    else
	echo >&2 "$prog: cannot move $NEW to $INDEX"
    fi
else
    echo >&2 "$prog: $PCP_BINADM_DIR/newhelp -S failed"
fi
