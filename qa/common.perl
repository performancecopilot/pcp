# Common shell preamble for Perl tests
#
# Copyright (c) 2026 Ken McDonell.  All Rights Reserved.
#

# get standard filters
. ./common.product
. ./common.filter
. ./common.check

if [ "$PCP_PLATFORM" = "darwin" ]
then
    # on macOS our Perl modules are installed in /usr/local/lib/perl5
    # so make sure perl can find 'em
    #
    if [ -n "$PERL5LIB" ]
    then
	# already set, only append our path if it is not already
	# included
	#
	if echo ':'"$PERL5LIB"':' | grep -q ':/usr/local/lib/perl5:'
	then
	    : already setup
	else
	    PERL5LIB="$PERL5LIB:/usr/local/lib/perl5"
	fi
    else
	PERL5LIB="/usr/local/lib/perl5"
    fi
    export PERL5LIB
fi
