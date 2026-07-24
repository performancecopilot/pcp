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
# Rebuild the pmsearch full-text search index from running PMCD.
# Typically invoked nightly via systemd timer or cron.
#

. $PCP_DIR/etc/pcp.env

status=1
tmp=`mktemp -d "$PCP_TMPFILE_DIR/pmsearch_index.XXXXXXXXX"` || exit 1
trap "rm -rf $tmp; exit \$status" 0 1 2 3 15

prog=`basename $0`
INDEX="$PCP_VAR_DIR/lib/pcp.search"

cat > $tmp/usage << EOF
Options:
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

VERBOSE=false
SHOWME=false

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"` || exit 1
eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-N)	SHOWME=true ;;
	-o)	INDEX="$2"; shift ;;
	-V)	VERBOSE=true ;;
	-\?)	_usage; status=0; exit ;;
	--)	shift; break ;;
    esac
    shift
done

if $VERBOSE
then
    echo "$prog: index target: $INDEX"
fi

# Extract all metric help text from PMCD in newhelp format.
# pminfo -tT output format:
#   metricname [oneline text]
#   Help:
#   multi-line helptext
#   <blank line before next metric>
#
# Transform to newhelp format:
#   @ metricname oneline text
#   multi-line helptext
#
pminfo -tT 2>/dev/null | $PCP_AWK_PROG '
/^[a-zA-Z][a-zA-Z0-9_]*\.[a-zA-Z0-9_.]+ / {
    # metric line: "name.with.dots [oneline]" or error
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
    printf "@ %s %s\n", name, oneline
    next
}
/^Help:$/ { next }
/^Full Help: Error:/ { skip = 1; next }
skip { next }
{ print }
' > $tmp/helptext

nhelplines=`wc -l < $tmp/helptext`
if [ "$nhelplines" -eq 0 ]
then
    echo >&2 "$prog: warning: no metrics found from PMCD"
    status=0
    exit
fi

if $VERBOSE
then
    nmetrics=`grep -c '^@ ' $tmp/helptext`
    echo "$prog: extracted $nmetrics metrics ($nhelplines lines)"
fi

# Extract instance names from running PMCD.
# Build metric→indom mapping, then get instances via pmprobe -I.
# Deduplicate by (indom, instance_name) since many metrics share indoms.
#
pminfo -I 2>/dev/null | $PCP_AWK_PROG '
/^[a-zA-Z]/ { metric = $1 }
/InDom:/ && !/PM_INDOM_NULL/ {
    for (i = 1; i <= NF; i++) {
	if ($i == "InDom:") {
	    print metric "\t" $(i+1)
	    break
	}
    }
}' > $tmp/metric_indom

pmprobe -I 2>/dev/null > $tmp/pmprobe_output

$PCP_AWK_PROG '
FILENAME == ARGV[1] {
    split($0, a, "\t")
    metric_indom[a[1]] = a[2]
    next
}
FILENAME == ARGV[2] {
    # metric→oneline from the helptext file we already built
    if (/^@ [a-zA-Z]/) {
	name = $2
	start = index($0, " ")
	start = index(substr($0, start + 1), " ")
	oneline = (start > 0) ? substr($0, index($0, $2) + length($2) + 1) : ""
	metric_oneline[name] = oneline
    }
    next
}
{
    # pmprobe -I: metric count "inst1" "inst2" ...
    metric = $1
    count = $2 + 0
    if (count <= 0 || $2 == "PM_IN_NULL") next

    indom = metric_indom[metric]
    if (indom == "") next

    if (!(indom in indom_oneline) && metric_oneline[metric] != "")
	indom_oneline[indom] = metric_oneline[metric]

    rest = $0
    while (match(rest, /"[^"]*"/)) {
	inst = substr(rest, RSTART + 1, RLENGTH - 2)
	key = indom SUBSEP inst
	if (!(key in seen)) {
	    seen[key] = 1
	    inst_data[++ninst] = indom "\t" inst
	    inst_indom[ninst] = indom
	}
	rest = substr(rest, RSTART + RLENGTH)
    }
}
END {
    for (i = 1; i <= ninst; i++) {
	indom = inst_indom[i]
	oneline = indom_oneline[indom]
	printf "@ I %s\n%s\n\n", inst_data[i], oneline
    }
}
' "$tmp/metric_indom" "$tmp/helptext" "$tmp/pmprobe_output" >> $tmp/helptext

if $VERBOSE
then
    ninst=`grep -c '^@ I ' $tmp/helptext`
    echo "$prog: extracted $ninst instances"
fi

if $SHOWME
then
    echo "$prog: would run: $PCP_BINADM_DIR/newhelp -S -o $INDEX $tmp/helptext"
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

# Build the search index
if $PCP_BINADM_DIR/newhelp -S -o "$INDEX" $tmp/helptext
then
    if $VERBOSE
    then
	echo "$prog: index written to $INDEX"
    fi
    status=0
else
    echo >&2 "$prog: $PCP_BINADM_DIR/newhelp -S failed"
fi
