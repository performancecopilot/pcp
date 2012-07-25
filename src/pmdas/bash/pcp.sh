# Shell interface to PCP shell event tracing PMDA

if [ -z "$PCP_SH_DONE" ]
then
    if [ -n "$PCP_CONF" ]
    then
	__CONF="$PCP_CONF"
    elif [ -n "$PCP_DIR" ]
    then
	__CONF="$PCP_DIR/etc/pcp.conf"
    else
	__CONF=/etc/pcp.conf
    fi
    if [ ! -f "$__CONF" ]
    then
	echo "pcp.env: Fatal Error: \"$__CONF\" not found" >&2
	exit 1
    fi
    eval `sed -e 's/"//g' $__CONF \
    | awk -F= '
/^PCP_/ && NF == 2 {
	exports=exports" "$1
	printf "%s=${%s:-\"%s\"}\n", $1, $1, $2
} END {
	print "export", exports
}'`
    export PCP_ENV_DONE=y
fi

. $PCP_SHARE_DIR/lib/bashproc.sh
