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
