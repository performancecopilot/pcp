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
    quote_c=''
    close_quote=''
    strip_quote=false
    in_quote=false
    case "$dir"
    in
	\"*\")
	    # "..."
	    strip_quote=true
	    ;;
	\'*\')
	    # '...'
	    strip_quote=true
	    ;;
	\"*)
	    # "... ..."
	    in_quote=true
	    quote_c='"'
	    close_quote='*"'
	    dir=`echo "$dir" | sed -e 's/^.//'`
	    ;;
	\'*)
	    # '... ...'
	    in_quote=true
	    quote_c="'"
	    close_quote="*'"
	    dir=`echo "$dir" | sed -e 's/^.//'`
	    ;;
	*\`*\`*)	# [...]`...`[...] all one word, no whitespace
	    ;;
	\`*)
	    # `....`
	    in_quote=true
	    close_quote='*`'
	    ;;
	*\`*)
	    _error "embedded \` and whitespace, without initial \" or ': $dir"
	    return
	    ;;
	*\$\(*\)*)	# [...]$(...)[...] all one word, no whitespace
	    ;;
	\$\(*)
	    # $(....)
	    in_quote=true
	    close_quote='*)'
	    ;;
	*\$\(*)
	    _error "embedded \$( and whitespace, without initial \" or ': $dir"
	    return
	    ;;
	*\$*)
	    # ...$...
	    ;;
    esac
    if [ "$debug_do_dir_and_args" = true ]
    then
	# secret debugging ...
	#
	echo "_do_dir_and_args ... dir=\"$quote_c$dir\""
	__i=0
	for word in $args
	do
	    echo " args[$__i]=\"$word\""
	    __i=`expr $__i + 1`
	done
	echo " strip_quote=$strip_quote in_quote=$in_quote close_quote=$close_quote quote_c=$quote_c"
    fi
    if $strip_quote
    then
	# just remove the leading and trailing "
	#
	dir=`echo "$dir" | sed -e 's/^.//' -e 's/.$//'`
    elif $in_quote
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
		    if [ "$quote_c" = "'" -o "$quote_c" = '"'  ]
		    then
			# ' or ", strip it from end
			#
			word=`echo "$word" | sed -e 's/.$//'`
		    fi
		    newdir="$newdir $word"
		    in_quote=false
		    ;;
		*)
		    if $in_quote
		    then
			# still within quote
			newdir="$newdir $word"
		    else
			# quote closed, gather remaining arguments
			if [ -z "$newargs" ]
			then
			    newargs="$word"
			else
			    newargs="$newargs $word"
			fi
		    fi
		    ;;
	    esac
	done
	if $in_quote
	then
	    _error "quoted string or shell command not terminated: $quote_c$dir $args"
	    # put back an initial quote, if any
	    #
	    dir="$quote_c$dir"
	    return
	else
	    dir="$newdir"
	    args="$newargs"
	fi
    fi

    # save _all_ of the directory field after white space mangling
    # and "arg" combining
    #
    orig_dir="$dir"

    # do any shell expansion
    #
    case "$dir"
    in
	*\$*|*\`*)
	    dir=`eval echo "$dir"`
	;;
    esac

    if [ "$debug_do_dir_and_args" = true ]
    then
	# secret debugging ...
	#
	echo " end strip_quote=$strip_quote in_quote=$in_quote close_quote=$close_quote quote_c=$quote_c"
	echo " orig_dir=\"$orig_dir\" dir=\"$dir\" args=\"$args\""
    fi
}

# generic error reporting for log control tools and parse_log_control()
#
_error()
{
    [ -n "$filename$line" ] && echo "$prog: [$filename:$line]"
    echo "Error: $@"
    echo "... logging for host \"$host\" unchanged"
    touch $tmp/err
}

# generic warning reporting for log control tools and parse_log_control()
#
_warning()
{
    [ -n "$filename$line" ] && echo "$prog: [$filename:$line]"
    echo "Warning: $@"
}

# parse_log_control:
# - one argument, the name of a pmlogger.control(5) control file
#
# shell variables set by the caller before calling _parse_log_control():
# - $prog [caller's name] (one of pmlogger_check, pmlogger_janitor or pmlogger_daily)
# - $tmp [dir for temporary files]
# - $VERBOSE and $VERY_VERBOSE [from -V on command line]
# - $SHOWME [from -N on command line]
# - assorted $PCP_* things [assume /etc/pcp.conf has been sourced]
#
# functions defined in the caller:
# - _add_callback() [for pmlogger_daily only]
# 
# once parsed, each valid control line triggers a callback to
# _callback_log_control() in the caller with these shell variables set:
# $host [host field, fully expanded]
# $primary [y or n]
# $socks [y or n]
# $orig_dir [directory field after keyword substitution but before shell
#	expansion]
# $dir [directory field, fully processed]
# $args [args field]
# $logpush [true if only pushing remotely via pmproxy, else false]
# $filename [shortened version of $1 suitable for error|warning msgs]
# $line [current line number in $1]
# and the current directory is $dir
#
# scoping rules
# shell variables and temproary files in $tmp that are private to
# _parse_log_control() are named with a "_" prefix
#
# other files
# - $tmp/err [created on fatal error]
# - $dir/lock [held while $dir is being processed then released]
#
_parse_log_control()
{
    # strip likely leading directories from control file pathname to get
    # a useful short filename for messages
    #
    filename=`echo "$1" | sed -e "s@$(dirname $PCP_PMLOGGERCONTROL_PATH)/@@"`
    line=0

    # sanity checks on caller
    #
    if [ $# -ne 1 ]
    then
	_error "_parse_log_control: usage botch: expect 1 arg not $#: $*"
	return
    fi
    if [ ! -f "$1" ]
    then
	_error "_parse_log_control: controlfile \"$1\" not found"
	return
    fi
    if [ -z "$tmp" ]
    then
	_error "_parse_log_control: botch: \$tmp not set"
	return
    fi
    if [ -z "$prog" ]
    then
	_error "_parse_log_control: botch: \$prog not set"
	return
    fi

    if [ -z "$_PWDCMD" ]
    then
	 # determine path for pwd command to override shell built-in
	 #
	_PWDCMD=`which pwd 2>/dev/null | $PCP_AWK_PROG '
BEGIN           { i = 0 }
/ not in /      { i = 1 }
/ aliased to /  { i = 1 }
                { if ( i == 0 ) print }
'`
	[ -z "$_PWDCMD" ] && _PWDCMD=/bin/pwd
	eval $_PWDCMD -P >/dev/null 2>&1
	[ $? -eq 0 ] && _PWDCMD="$_PWDCMD -P"
    fi
    _here=`$_PWDCMD`

    if echo "$1" | grep -q -e '\.rpmsave$' -e '\.rpmnew$' -e '\.rpmorig$' -e '\.dpkg-dist$' -e '\.dpkg-old$' -e '\.dpkg-new$'
    then
	echo "Warning: ignoring packaging backup control file \"$1\""
	return
    fi

    sed <"$1" \
        -e "s;PCP_ARCHIVE_DIR;$PCP_ARCHIVE_DIR;g" \
        -e "s;PCP_LOG_DIR;$PCP_LOG_DIR;g" \
    | while read host primary socks dir args
    do
	# start in one place for each iteration (beware of relative paths)
	cd "$_here"
	line=`expr $line + 1`

	if $VERY_VERBOSE
	then
	    case "$host"
	    in
		\#!#*)	# stopped by pmlogctl ... for pmlogger_daily only
			# we need to check this one
			[ "$prog" = pmlogger_daily ] \
			    && echo >&2 "[$filename:$line] host=\"$host\" primary=\"$primary\" socks=\"$socks\" dir=\"$dir\" args=\"$args\""
			;;
		\#*|'')	# comment or empty
			;;
		*)	echo >&2 "[$filename:$line] host=\"$host\" primary=\"$primary\" socks=\"$socks\" dir=\"$dir\" args=\"$args\""
			;;
	    esac
	fi

	case "$host"
	in
	    \#!#*)	# stopped by pmlogctl ... for pmlogger_daily only
			# we need to check this one
			[ "$prog" = pmlogger_daily ] \
			    && host=`echo "$host" | sed -e 's/^#!#//'`
		;;
	    \#*|'')	# comment or empty
		continue
		;;
	    \$*)	# in-line variable assignment
		$SHOWME && echo "# $host $primary $socks $dir $args"
		_cmd=`echo "$host $primary $socks $dir $args" \
		     | sed -n \
			 -e "/='/s/\(='[^']*'\).*/\1/" \
			 -e '/="/s/\(="[^"]*"\).*/\1/' \
			 -e '/=[^"'"'"']/s/[;&<>|].*$//' \
			 -e '/^\\$[A-Za-z][A-Za-z0-9_]*=/{
s/^\\$//
s/^CULLAFTER=/PCP_CULLAFTER/
s/^\([A-Za-z][A-Za-z0-9_]*\)=/export \1; \1=/p
}'`
		if [ -z "$_cmd" ]
		then
		    # in-line command, not a variable assignment
		    _warning "in-line command is not a variable assignment, line ignored"
		else
		    rm -f $tmp/_cmd
		    case "$_cmd"
		    in
			'export PATH;'*)
			    _warning "cannot change \$PATH, line ignored"
			    ;;

			'export IFS;'*)
			    _warning "cannot change \$IFS, line ignored"
			    ;;

			'export PCP_CULLAFTER;'*)
			    _old_value="$PCP_CULLAFTER"
			    _check=`echo "$_cmd" | sed -e 's/.*=//' -e 's/  *$//'`
			    $PCP_BINADM_DIR/find-filter </dev/null >/dev/null 2>&1 mtime "+$_check"
			    if [ $? != 0 -a -n "$_check" -a X"$_check" != Xforever -a X"$_check" != Xnever ]
			    then
				_error "\$PCP_CULLAFTER value ($_check) must be number, time, \"forever\" or \"never\""
			    else
				$SHOWME && echo "+ $_cmd"
				echo eval $_cmd >>$tmp/_cmd
				eval $_cmd
				if [ -n "$_old_value" -a "$_old_value" != "$PCP_CULLAFTER" ]
				then
				    _warning "\$PCP_CULLAFTER ($PCP_CULLAFTER) reset from control file, previous value ($_old_value) ignored"
				fi
				if [ -n "$PCP_CULLAFTER" -a -n "$CULLAFTER_CMDLINE" -a "$PCP_CULLAFTER" != "$CULLAFTER_CMDLINE" ]
				then
				    _warning "\$PCP_CULLAFTER ($PCP_CULLAFTER) reset from control file, -k value ($CULLAFTER_CMDLINE) ignored"
				    CULLAFTER_CMDLINE=""
				fi
			    fi
			    ;;

			'export PCP_COMPRESS;'*)
			    _old_value="$PCP_COMPRESS"
			    $SHOWME && echo "+ $_cmd"
			    echo eval $_cmd >>$tmp/_cmd
			    eval $_cmd
			    if [ -n "$_old_value" -a "$_old_value" != "$$PCP_COMPRESS" ]
			    then
				_warning "\$PCP_COMPRESS ($PCP_COMPRESS) reset from control file, previous value ($_old_value) ignored"
			    fi
			    if [ -n "$PCP_COMPRESS" -a -n "$COMPRESS_CMDLINE" -a "$PCP_COMPRESS" != "$COMPRESS_CMDLINE" ]
			    then
				_warning "\$PCP_COMPRESS ($PCP_COMPRESS) reset from control file, -X value ($COMPRESS_CMDLINE) ignored"
				COMPRESS_CMDLINE=""
			    fi
			    ;;

			'export PCP_COMPRESSAFTER;'*)
			    _old_value="$PCP_COMPRESSAFTER"
			    _check=`echo "$_cmd" | sed -e 's/.*=//' -e 's/  *$//'`
			    $PCP_BINADM_DIR/find-filter </dev/null >/dev/null 2>&1 mtime "+$_check"
			    if [ $? != 0 -a -n "$_check" -a X"$_check" != Xforever -a X"$_check" != Xnever ]
			    then
				_error "\$PCP_COMPRESSAFTER value ($_check) must be number, time, \"forever\" or \"never\""
			    else
				$SHOWME && echo "+ $_cmd"
				echo eval $_cmd >>$tmp/_cmd
				eval $_cmd
				if [ -n "$_old_value" -a "$_old_value" != "$PCP_COMPRESSAFTER" ]
				then
				    _warning "\$PCP_COMPRESSAFTER ($PCP_COMPRESSAFTER) reset from control file, previous value ($_old_value) ignored"
				fi
				if [ -n "$PCP_COMPRESSAFTER" -a -n "$COMPRESSAFTER_CMDLINE" -a "$PCP_COMPRESSAFTER" != "$COMPRESSAFTER_CMDLINE" ]
				then
				    _warning "\$PCP_COMPRESSAFTER ($PCP_COMPRESSAFTER) reset from control file, -x value ($COMPRESSAFTER_CMDLINE) ignored"
				    COMPRESSAFTER_CMDLINE=""
				fi
			    fi
			    ;;

			'export PCP_COMPRESSREGEX;'*)
			    _old_value="$PCP_COMPRESSREGEX"
			    $SHOWME && echo "+ $_cmd"
			    echo eval $_cmd >>$tmp/_cmd
			    eval $_cmd
			    if [ -n "$_old_value" -a "$_old_value" != "$PCP_COMPRESSREGEX" ]
			    then
				_warning "\$PCP_COMPRESSREGEX ($PCP_COMPRESSREGEX) reset from control file, previous value ($_old_value) ignored"
			    fi
			    if [ -n "$PCP_COMPRESSREGEX" -a -n "$COMPRESSREGEX_CMDLINE" -a "$PCP_COMPRESSREGEX" != "$COMPRESSREGEX_CMDLINE" ]
			    then
				_warning "\$PCP_COMPRESSREGEX ($PCP_COMPRESSREGEX) reset from control file, -Y value ($COMPRESSREGEX_CMDLINE) ignored"
				COMPRESSREGEX_CMDLINE=""
			    fi
			    ;;

			'export PCP_MERGE_CALLBACK;'*)
			    if ! $COMPRESSONLY && [ "$prog" = pmlogger_daily ]
			    then
				$SHOWME && echo "+ $_cmd"
				_script="`echo "$_cmd" | sed -e 's/.*BACK; PCP_MERGE_CALLBACK=//'`"
				if _add_callback "$_script" $tmp/merge_callback
				then
				    $VERBOSE && echo "Add merge callback: $script"
				fi
			    fi
			    ;;

			'export PCP_COMPRESS_CALLBACK;'*)
			    if [ "$prog" = pmlogger_daily ]
			    then
				$SHOWME && echo "+ $_cmd"
				_script="`echo "$_cmd" | sed -e 's/.*BACK; PCP_COMPRESS_CALLBACK=//'`"
				if _add_callback "$_script" $tmp/compress_callback
				then
				    $VERBOSE && echo "Add compress callback: $script"
				fi
			    fi
			    ;;


			*)
			    $SHOWME && echo "+ $_cmd"
			    echo eval $_cmd >>$tmp/_cmd
			    eval $_cmd
			    ;;
		    esac
		fi
		continue
		;;
	esac

	# set the version and other global variables
	#
	[ -f $tmp/_cmd ] && . $tmp/_cmd

	# Note on control file format version
	#  1.0 was shipped as part of SGI's PCPWEB beta, and did not
	#  include the socks field ... this is no longer supported
	#  1.1 is the first production release and the default
	#  (and only) version
	#
	if [ -n "$version" -a "$version" != "1.1" ]
	then
	    _error "version $version not supported"
	    continue
	fi

	# do shell expansion of $dir if needed
	#
	_do_dir_and_args
	$VERY_VERBOSE && echo >&2 "After _do_dir_and_args: orig_dir=$orig_dir dir=$dir"

	# substitute LOCALHOST keyword in selected fields
	# $dir - LOCALHOSTNAME -> hostname(1) or localhost
	# $host - LOCALHOSTNAME -> local:
	#
	_dirhostname=`hostname || echo localhost`
	dir=`echo "$dir" | sed -e "s;LOCALHOSTNAME;$_dirhostname;g"`
	[ "$primary" = y -o "X$host" = XLOCALHOSTNAME ] && host=local:

	# check for archive ``push'' to remote pmproxy with + prefix for
	# directory field
	#
	case "$dir"
	in
	    +*)	dir=`echo "$dir" | sed -e 's/^+//'`
	    	orig_dir=`echo "$orig_dir" | sed -e 's/^+//'`
		logpush=true
		;;
	    *)	logpush=false
	    	;;
	esac

	if [ -z "$primary" -o -z "$socks" -o -z "$dir" -o -z "$args" ]
	then
	    _error "insufficient fields in control file record"
	    continue
	fi

	if [ -f $tmp/_dir ]
	then
	    # check for directory duplicate entries
	    #
	    grep "^$dir " <$tmp/_dir >$tmp/_tmp
	    if [ -s $tmp/_tmp ]
	    then
		_error "Duplicate pmlogger instances for archive directory \"$dir\""
		echo "Prior instance(s) for this directory:"
		cat $tmp/_tmp
		continue
	    fi
	fi
	echo "$dir $filename:$line" >>$tmp/_dir

	# make sure output directory hierarchy exists and $PCP_USER
	# user can write there
	#
	if [ ! -d "$dir" ]
	then
	    # SHOWME note: we need to try and make the dir (for QA)
	    # and only give up on this control line if the mkdir
	    # or the cd later fails
	    #
	    $SHOWME && echo "+ mkdir -p -m 0775 $dir"
	    # mode rwxrwxr-x is the default for pcp:pcp dirs
	    umask 002
	    mkdir -p -m 0775 "$dir" >$tmp/_tmp 2>&1
	    # reset the default mode to rw-rw-r- for files
	    umask 022
	    if [ ! -d "$dir" ]
	    then
		cat $tmp/_tmp
		if $SHOWME
		then
		    echo "+ ... cannot show any more for this control line"
		else
		    _error "cannot create directory ($dir) for PCP archive files"
		fi
		continue
	    else
		_warning "creating directory ($dir) for PCP archive files"
	    fi
	fi

	# check $dir, cd there, acquire lock
	#
	$SHOWME && echo "+ cd $dir"
	if cd "$dir"
	then
	    :
	else
	    if $SHOWME
	    then
		echo "+ ... cannot show any more for this control line"
	    else
		_error "cannot chdir to directory ($dir) for PCP archive files"
	    fi
	    continue
	fi
	dir=`$_PWDCMD`
	$VERY_VERBOSE && echo >&2 "Current dir: $dir"

	# pmlogger_janitor does NOT need the lock because it is only
	# ever called from pmlogger_check with the lock already held
	#
	if [ "$prog" != pmlogger_janitor ]
	then
	    if $SHOWME
	    then
		echo "+ get mutex lock"
	    else
		if [ ! -w "$dir" ]
		then
		    _warning "no write access in $dir skip lock file processing"
		else
		    # demand mutual exclusion
		    #
		    rm -f $tmp/_stamp $tmp/_out
		    _delay=200	# tenths of a second
		    while [ $_delay -gt 0 ]
		    do
			if pmlock -i "$$ $prog" -v "$dir/lock" >>$tmp/_out 2>&1
			then
			    if $VERY_VERBOSE
			    then
				echo "Acquired lock:"
				LC_TIME=POSIX ls -l "$dir/lock"
				[ -s "$dir/lock" ] && cat "$dir/lock"
			    fi
			    echo "$dir/lock" >$tmp/_lock
			    break
			else
			    [ -f $tmp/_stamp ] || touch -t `pmdate -30M %Y%m%d%H%M` $tmp/_stamp
			    find $tmp/_stamp -newer "$dir/lock" -print 2>/dev/null >$tmp/_tmp
			    if [ -s $tmp/_tmp ]
			    then
				if [ -f "$dir/lock" ]
				then
				    _warning "removing lock file older than 30 minutes"
				    LC_TIME=POSIX ls -l "$dir/lock"
				    [ -s "$dir/lock" ] && cat "$dir/lock"
				    rm -f "$dir/lock"
				else
				    # there is a small timing window here where pmlock
				    # might fail, but the lock file has been removed by
				    # the time we get here, so just keep trying
				    #
				    :
				fi
			    fi
			fi
			pmsleep 0.1
			_delay=`expr $_delay - 1`
		    done
		    if [ $_delay -eq 0 ]
		    then
			# failed to gain mutex lock
			#
			# if we are not pmlogger_daily and pmlogger_daily is already
			# running ... check it, and silently move on if this is the
			# case
			#
			# Note: $PCP_RUN_DIR may not exist (see pmlogger_daily note),
			#       but only if pmlogger_daily has not run, so no chance
			#       of a collision
			#
			if [ "$prog" != pmlogger_daily -a -f "$PCP_RUN_DIR"/pmlogger_daily.pid ]
			then
			    # maybe, check pid matches a running /bin/sh
			    #
			    _pid=`cat "$PCP_RUN_DIR"/pmlogger_daily.pid`
			    if _get_pids_by_name sh | grep "^$_pid\$" >/dev/null
			    then
				# seems to be still running ... nothing for us to see
				# or do here
				#
				continue
			    fi
			fi
			if [ -f "$dir/lock" ]
			then
			    echo "$prog: Warning: is another PCP cron job running concurrently?"
			    LC_TIME=POSIX ls -l "$dir/lock"
			    [ -s "$dir/lock" ] && cat "$dir/lock"
			else
			    echo "$prog: `cat $tmp/_out`"
			fi
			_warning "failed to acquire exclusive lock ($dir/lock) ..."
			continue
		    fi
		fi
	    fi
	fi

	# now hand back to the caller to do the real work ...
	#
	_callback_log_control

	# unlock
	#
	if $SHOWME
	then
	    echo "+ release mutex lock"
	else
	    rm -f "$dir/lock"
	    echo >$tmp/_lock
	fi

    done
}

# Called from _callback_log_control() [in pmlogger_check and pmlogger_janitor]
# to return the PID of the running pmlogger that matches the current control
# line, or '' if no such process exists
#
# See _parse_log_control() for the variables that are in play when this
# function is called.
#
_find_matching_pmlogger()
{
    _pid=''
    if [ "$primary" = y ]
    then
	if test -e "$PCP_TMP_DIR/pmlogger/primary"
	then
	    if $VERY_VERBOSE
	    then 
		_host=`sed -n 2p <"$PCP_TMP_DIR/pmlogger/primary"`
		_arch=`sed -n 3p <"$PCP_TMP_DIR/pmlogger/primary"`
		echo >&2 "... try $PCP_TMP_DIR/pmlogger/primary: _host=$_host _arch=$_arch"
	    fi
	    _pid=`_get_primary_logger_pid`
	    if [ -z "$_pid" ]
	    then
		if $VERY_VERBOSE
		then
		    echo >&2 "primary pmlogger process pid not found"
		    ls >&2 -l "$PCP_RUN_DIR/pmlogger.pid"
		    ls >&2 -l "$PCP_TMP_DIR/pmlogger"
		fi
	    elif _get_pids_by_name pmlogger | grep "^$_pid\$" >/dev/null
	    then
		$VERY_VERBOSE && echo >&2 "primary pmlogger process $_pid identified, OK"
		$VERY_VERY_VERBOSE && $PCP_PS_PROG $PCP_PS_ALL_FLAGS | grep >&2 -E '[P]ID|/[p]mlogger( |$)'
	    else
		$VERY_VERBOSE && echo >&2 "primary pmlogger process $_pid not running"
		$VERY_VERY_VERBOSE && $PCP_PS_PROG $PCP_PS_ALL_FLAGS | grep >&2 -E '[P]ID|/[p]mlogger( |$)'
		_pid=''
	    fi
	else
	    if $VERY_VERBOSE
	    then
		echo >&2 "$PCP_TMP_DIR/pmlogger/primary: missing?"
		echo >&2 "Contents of $PCP_TMP_DIR/pmlogger"
		ls >&2 -l $PCP_TMP_DIR/pmlogger
		echo >&2 "--- end of ls output ---"
	    fi
	fi
    else
	# not the primary pmlogger ...
	#
	for _log in $PCP_TMP_DIR/pmlogger/[0-9]*
	do
	    [ "$_log" = "$PCP_TMP_DIR/pmlogger/[0-9]*" ] && continue
	    if $logpush
	    then
		# the "archive" record in $PCP_TMP_DIR is not the
		# archive basename, but something like http://foo.com:44322
		# from last argument to pmlogger
		#
		_trydir=`echo "$args" | sed -E -e 's@(.* |^)http://@http://@' -e 's/ .*//'`
	    else
		_trydir="$dir"
	    fi
	    if $VERY_VERBOSE
	    then
		_host=`sed -n 2p <$_log`
		_archdir=`sed -n 3p <$_log`
		$PCP_ECHO_PROG >&2 $PCP_ECHO_N "... try $_log _host=$_host _archdir=$_archdir _trydir=$_trydir dir=$dir: ""$PCP_ECHO_C"
	    fi
	    # throw away stderr in case $log has been removed by now
	    # if "archive" basename starts with a /, strip filename to
	    # get dirname, else use http://foo.com:44322 in full
	    #
	    _match=`sed -e '3s/^\(\/.*\)\/[^/]*$/\1/' $_log 2>/dev/null \
	            | $PCP_AWK_PROG '
BEGIN				{ m = 0 }
NR == 3 && $0 == "'"$_trydir"'"	{ m = 2; next }
END				{ print m }'`
	    $VERY_VERBOSE && $PCP_ECHO_PROG >&2 $PCP_ECHO_N "match=$_match ""$PCP_ECHO_C"
	    if [ "$_match" = 2 ]
	    then
		_pid=`echo $_log | sed -e 's,.*/,,'`
		if _get_pids_by_name pmlogger | grep "^$_pid\$" >/dev/null
		then
		    $VERY_VERBOSE && echo >&2 "pmlogger process $_pid identified, OK"
		    $VERY_VERY_VERBOSE && $PCP_PS_PROG $PCP_PS_ALL_FLAGS | grep >&2 -E '[P]ID|/[p]mlogger( |$)'
		    break
		fi
		$VERY_VERBOSE && echo >&2 "pmlogger process $_pid not running, skip"
		$VERY_VERY_VERBOSE && $PCP_PS_PROG $PCP_PS_ALL_FLAGS | grep >&2 -E '[P]ID|/[p]mlogger( |$)'
		_pid=''
	    else
		$VERY_VERBOSE && echo >&2 "different directory, skip"
	    fi
	done
    fi
    echo "$_pid"
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
	echo "_save_prev_file: \"$1\" exists and is a symlink"
	ls -ld "$1"
	return 1
    elif [ -f "$1" ]
    then
	# Complicated because pathname.prev may already exist and
	# pathname may already exist and one or other or both
	# may not be able to be removed.
	# As we have no locks protecting these files, and the contents
	# are not really useful if we're experiencing a race between
	# concurrent executions, quietly do the best you can
	#
	rm -f "$1.prev" 2>/dev/null
	cp -f -p "$1" "$1.prev" 2>/dev/null
	rm -f "$1" 2>/dev/null
	return 0
    else
	echo "_save_prev_file: \"$1\" exists and is not a file"
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
    if [ $# -ne 1 ]
    then
	echo >&2 "Usage: _is_archive file"
	return 1
    fi
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
	    *.zst)		zstd -dc --quiet "$1"
	    			;;
	    *)			cat "$1"
	    			;;
	esac 2>/dev/null \
	| if [ "$PCP_PLATFORM" = openbsd ]
	then
	    # strange but true, xv | dd hangs xv here when
	    # dd quits
	    #
	    cat >/tmp/is.archive.$$
	    dd ibs=1 count=7 if=/tmp/is.archive.$$ 2>/dev/null
	else
	    dd ibs=1 count=7 2>/dev/null
	fi \
	| od -X \
	| $PCP_AWK_PROG '
BEGIN						{ sts = 1 }
# V3 big endian
NR == 1 && NF == 3 && $2 == "00000328" && $3 == "50052600" { sts = 0; next }	
# V3 little endian
NR == 1 && NF == 3 && $2 == "28030000" && $3 == "00260550" { sts = 0; next }	
# V1 or V2 big endian
NR == 1 && NF == 3 && $2 == "00000084" && $3 == "50052600" { sts = 0; next }	
# V1 or V2 little endian
NR == 1 && NF == 3 && $2 == "84000000" && $3 == "00260550" { sts = 0; next }	
# V1 or V2 big endian when od -X => 16-bit words
NR == 1 && NF == 5 && $2 == "0000" && $3 == "0084" && $4 == "5005" && $5 == "2600" { sts = 0; next }
# V1 or V2 little endian when od -X => 16-bit words
NR == 1 && NF == 5 && $2 == "0000" && $3 == "8400" && $4 == "0550" && $5 == "0026" { sts = 0; next }
END						{ exit sts }'
	__sts=$?
	rm -f /tmp/is.archive.$$
	return $__sts
    fi
}
