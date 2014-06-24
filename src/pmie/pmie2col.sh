#!/bin/sh
#
# A crude ascii reporting tool, convert pmie to column output
#
# The pmie rules need to be of the form:
# load_1 = kernel.all.load #'1 minute';
# idle = kernel.all.cpu.idle;
# column_name=some other expression;
# ...
#
# Each pmie expression has to produce a singular value.
#
# With timestamps (pmie -e or pmie output from a PCP archive), lines look like
#	metric (Tue Feb 13 05:01:19 2001): value
#       load_1 (Tue Dec 23 12:20:45 2003): 0.24
# the first sed step in the filter sorts this out.
#
# First e-mailed to Patrick Aland <paland@stetson.edu> and pcp@oss.sgi.com
# on Wed, 24 Jan 2001.
#
 
# Get standard environment
. $PCP_DIR/etc/pcp.env

tmp=`mktemp -d /tmp/pcp.XXXXXXXXX` || exit 1
status=1
trap "rm -rf $tmp; exit \$status" 0 1 2 3 15
prog=`basename $0`

# For interactive use, works better with line buffered output from sed(1)
# and awk(1).
#
case "$PCP_PLATFORM"
in
    linux|mingw|kfreebsd|gnu)
	SED="sed -u"
	;;
    freebsd|darwin)
	SED="sed -l"
	;;
    *)
	SED=sed
	;;
esac

echo > $tmp/usage
cat >> $tmp/usage <<EOF
Options:
  -d=CHAR,--delimiter=CHAR   set the output delimiter character
  -p=N,--precision=N   set the output floating point precision
  -w=N,--width=N       set the width of each column of output
  --help
EOF

_usage()
{
    pmgetopt --progname=$prog --config=$tmp/usage --usage
    exit 1
}

pre=2	# floating point precision
wid=7	# reporting column width
delim=' '

ARGS=`pmgetopt --progname=$prog --config=$tmp/usage -- "$@"`
[ $? != 0 ] && exit 1

eval set -- "$ARGS"
while [ $# -gt 0 ]
do
    case "$1"
    in
	-w)	wid="$2"
		shift
		;;
	-p)	pre="$2"
		shift
		;;
	-d)	delim="$2"
		shift
		;;
	--)	shift
		break
		;;
	-\?)	_usage
		;;
    esac
    shift
done

[ $# -eq 0 ] || _usage

# culled output at the start is produced by pmie and/or pmafm
$SED \
    -e '/^pmie: timezone set to/d' \
    -e '/^Note: running pmie serially, once per archive$/d' \
    -e '/^Host: [A-Za-z0-9]* Archive: /d' \
    -e '/^[^ ][^ ]* ([A-Z][a-z][a-z] [A-Z][a-z][a-z]  *[0-9][0-9]* [0-2][0-9]:[0-5][0-9]:[0-5][0-9] [0-9][0-9][0-9][0-9]): /{
s/ (/|/
s/): /|/
}' \
    -e '/^\([^ ][^ ]*\):/s//\1||/' \
| awk -F\| -v delim="$delim" '
NF == 0				{ if (state == 0) {
				    ncol = i
				    print ""
				  }
				  if (stamp != "") printf "%24s ",stamp
				  for (i = 0; i < ncol; i++) {
				    if (v[i] == "?")
					# no value
					printf "%s%'$wid'.'$wid's",delim,"?"
				    else if (v[i]+0 == v[i])
					# number
					printf "%s%'$wid'.'$pre'f",delim,v[i]
				    else
					# string
					printf "%s%'$wid'.'$wid's",delim,v[i]
				    v[i] = "?"
				  }
				  print ""
				  i = 0
				  stamp = ""
				  state = 1
				  next
				}
NF == 3 && stamp == ""		{ stamp = $2 }
state == 0			{ if (i == 0 && stamp != "") printf "%24s ",""
				  printf "%s%'$wid'.'$wid's",delim,$1 }
				{ v[i++] = $NF }'

status=0
exit
