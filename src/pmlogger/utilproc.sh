# Helper procedures for the scripts usually run from cron, e.g.
# pmlogger_check, pmie_daily, etc
#

# Handle shell expansion and quoting for $dir and adjust $args as
# appropriate.
# Called with $dir and $args set from reading the control file.
# Returns $dir and $args which may be modified.
# May use _error().
#
_do_dir_and_args()
{
    close_quote=''
    do_shell=false
    case "$dir"
    in
	\"*)
	    # "...."
	    close_quote='*"'
	    ;;
	\`*)
	    # `....`
	    close_quote='*`'
	    ;;
	*\`*)
	    _error "embedded \` without enclosing \": $dir"
	    ;;
	\$\(*)
	    # $(....)
	    close_quote='*)'
	    ;;
	*\$\(*)
	    _error "embedded \$( without enclosing \": $dir"
	    ;;
	*\$*)
	    # ...$...
	    do_shell=true
	    ;;
    esac
    if [ -n "$close_quote" ]
    then
	# we have a "dir" argument that begins with one of the recognized
	# quoting mechanisms ... append additional words to $dir (from
	# $args) until quote is closed
	#
	newargs=''
	newdir="$dir"
	for word in $args
	do
	    case $word
	    in
		$close_quote)
		    newdir="$newdir $word"
		    do_shell=true
		    ;;
		*)
		    if $do_shell
		    then
			# quote closed, gather remaining arguments
			if [ -z "$newargs" ]
			then
			    newargs="$word"
			else
			    newargs="$newargs $word"
			fi
		    else
			# still within quote
			newdir="$newdir $word"
		    fi
		    ;;
	    esac
	done
	if $do_shell
	then
	    dir="$newdir"
	    args="$newargs"
	else
	    _error "quote not terminated: $dir $args"
	fi
    fi
    $do_shell && dir="`eval echo $dir`"
}

# Usage: _save_prev_file pathname
#
# if pathname exists, try to preserve the contents in pathname.prev and
# remove pathname, prior to a new pathname being created by the caller
#
# return status indicates success
#
_save_prev_file()
{
    if [ ! -e "$1" ]
    then
	# does not exist, nothing to be done
	return 0
    elif [ -L "$1" ]
    then
	echo "_save_prev_filename: \"$1\" exists and is a symlink"
	ls -ld "$1"
	return 1
    elif [ -f "$1" ]
    then
	# complicated because pathname.prev may already exist and
	# pathname may already exist and one or other or both
	# may not be able to be removed
	#
	if [ -e "$1.prev" ]
	then
	    if rm -f "$1.prev"
	    then
		:
	    else
		echo "_save_prev_filename: unable to remove \"$1.prev\""
		ls -ld "$1.prev"
		if rm -f "$1"
		then
		    :
		else
		    echo "_save_prev_filename: unable to remove \"$1\""
		    ls -ld "$1"
		fi
		return 1
	    fi
	fi
	__sts=0
	if cp -p "$1" "$1.prev"
	then
	    :
	else
	    echo "_save_prev_filename: copy \"$1\" to \"$1.prev\" failed"
	    __sts=1
	fi
	if rm -f "$1"
	then
	    :
	else
	    echo "_save_prev_filename: unable to remove \"$1\""
	    ls -ld "$1"
	    __sts=1
	fi
	return $__sts
    else
	echo "_save_prev_filename: \"$1\" exists and is not a file"
	ls -ld "$1"
	return 1
    fi
}

# check for magic numbers in a file that indicate it is a PCP archive
#
# if file(1) was reliable, this would be much easier, ... sigh
#
_is_archive()
{
    if [ ! -f "$1" ]
    then
	return 1
    else
	case "$1"
	in
	    *.xz|*.lzma)	xz -dc "$1"
	    			;;
	    *.bz2|*.bz)		bzip2 -dc "$1"
	    			;;
	    *.gz|*.Z|*.z)	gzip -dc "$1"
	    			;;
	    *)			cat "$1"
	    			;;
	esac \
	| dd ibs=1 count=7 2>/dev/null \
	| od -X \
	| $PCP_AWK_PROG '
BEGIN						{ sts = 1 }
NR == 1 && NF == 5 && $2 == "0000" && $3 == "0084" && $4 == "5005" && $5 == "2600" { sts = 0 }
NR == 1 && NF == 5 && $2 == "0000" && $3 == "8400" && $4 == "0550" && $5 == "0026" { sts = 0 }
NR == 1 && NF == 3 && $2 == "00000084" && $3 == "50052600" { sts = 0 }
NR == 1 && NF == 3 && $2 == "84000000" && $3 == "00260550" { sts = 0 }
END						{ exit sts }'
    fi
    return $?
}
