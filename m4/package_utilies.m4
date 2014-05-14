#
# Check for specified utility (env var) - if unset, fail.
#
AC_DEFUN([AC_PACKAGE_NEED_UTILITY],
  [ if test -z "$2"; then
	echo
	echo FATAL ERROR: $3 does not seem to be installed.
	echo $1 cannot be built without a working $4 installation.
	exit 1
    fi
  ])

#
# Generic macro, sets up all of the global build variables.
# The following environment variables may be set to override defaults:
#  MAKE TAR BZIP2 MAKEDEPEND AWK SED ECHO SORT RPMBUILD DPKG LEX YACC
#
AC_DEFUN([AC_PACKAGE_UTILITIES],
  [ AC_PROG_CXX
    AC_PACKAGE_NEED_UTILITY($1, "$CXX", cc, [C++ compiler])

    if test -z "$MAKE"; then
	AC_PATH_PROG(MAKE, mingw32-make,, /mingw/bin:/usr/bin:/usr/local/bin)
    fi
    if test -z "$MAKE"; then
	AC_PATH_PROG(MAKE, gmake,, /usr/bin:/usr/local/bin)
    fi
    if test -z "$MAKE"; then
	AC_PATH_PROG(MAKE, make,, /usr/bin)
    fi
    make=$MAKE
    AC_SUBST(make)
    AC_PACKAGE_NEED_UTILITY($1, "$make", make, [GNU make])

    if test -z "$TAR"; then
	AC_PATH_PROG(TAR, tar,, /bin:/usr/local/bin:/usr/bin)
    fi
    tar=$TAR
    AC_SUBST(tar)

    if test -z "$ZIP"; then
	AC_PATH_PROG(ZIP, gzip,, /bin:/usr/bin:/usr/local/bin)
    fi
    zip=$ZIP
    AC_SUBST(zip)

    if test -z "$BZIP2"; then
	AC_PATH_PROG(BZIP2, bzip2,, /bin:/usr/bin:/usr/local/bin)
    fi
    bzip2=$BZIP2
    AC_SUBST(bzip2)

    if test -z "$MAKEDEPEND"; then
	AC_PATH_PROG(MAKEDEPEND, makedepend, /bin/true)
    fi
    makedepend=$MAKEDEPEND
    AC_SUBST(makedepend)

    if test -z "$AWK"; then
	AC_PATH_PROG(AWK, awk,, /bin:/usr/bin)
    fi
    awk=$AWK
    AC_SUBST(awk)

    if test -z "$SED"; then
	AC_PATH_PROG(SED, sed,, /bin:/usr/bin)
    fi
    sed=$SED
    AC_SUBST(sed)

    if test -z "$ECHO"; then
	AC_PATH_PROG(ECHO, echo,, /bin:/usr/bin)
    fi
    echo=$ECHO
    AC_SUBST(echo)

    if test -z "$SORT"; then
	AC_PATH_PROG(SORT, sort,, /bin:/usr/bin)
    fi
    sort=$SORT
    AC_SUBST(sort)

    dnl check if symbolic links are supported
    AC_PROG_LN_S

    dnl check if rpmbuild is available
    if test -z "$RPMBUILD"
    then
	AC_PATH_PROG(RPMBUILD, rpmbuild)
    fi
    rpmbuild=$RPMBUILD
    AC_SUBST(rpmbuild)

    dnl check if the dpkg program is available
    if test -z "$DPKG"
    then
	AC_PATH_PROG(DPKG, dpkg)
    fi
    dpkg=$DKPG
    AC_SUBST(dpkg)

    dnl Check for the MacOSX PackageMaker
    AC_MSG_CHECKING([for PackageMaker])
    if test -z "$PACKAGE_MAKER"
    then
	devapps=/Developer/Applications
	darwin6=${devapps}/PackageMaker.app/Contents/MacOS
	darwin7=${devapps}/Utilities/PackageMaker.app/Contents/MacOS
	if test -x ${darwin6}/PackageMaker       
	then
	    package_maker=${darwin6}/PackageMaker
	    AC_MSG_RESULT([ yes (darwin 6.x)])
	elif test -x ${darwin7}/PackageMaker
	then
	    AC_MSG_RESULT([ yes (darwin 7.x)])
	    package_maker=${darwin7}/PackageMaker
	else
	    AC_MSG_RESULT([ no])
	fi
    else
	package_maker="$PACKAGE_MAKER"
    fi
    AC_SUBST(package_maker)

    dnl check if the MacOSX hdiutil program is available
    test -z "$HDIUTIL" && AC_PATH_PROG(HDIUTIL, hdiutil)
    hdiutil=$HDIUTIL
    AC_SUBST(hdiutil)

    dnl check if a toolchain is available for the books
    test -z "$PUBLICAN" && AC_PATH_PROG(PUBLICAN, publican)
    publican=$PUBLICAN
    AC_SUBST(publican)
    test -z "$DBLATEX" && AC_PATH_PROG(DBLATEX, dblatex)
    dblatex=$DBLATEX
    AC_SUBST(dblatex)
    test -z "$XMLTO" && AC_PATH_PROG(XMLTO, xmlto)
    xmlto=$XMLTO
    AC_SUBST(xmlto)

    book_toolchain=""
    if test "$do_books" = "check" -o "$do_books" = "yes"
    then
        if test "$BOOK_TOOLCHAIN" != ""
        then
            book_toolchain=$BOOK_TOOLCHAIN
        elif test "$DBLATEX" != ""
        then
            book_toolchain=dblatex
        elif test "$PUBLICAN" != ""
        then
            book_toolchain=publican
        elif test "$XMLTO" != ""
        then
            book_toolchain=xmlto
        elif test "$do_books" = "yes"
	then
	    AC_MSG_ERROR(cannot enable books build - no toolchain found)
        fi
    fi
    AC_SUBST(book_toolchain)

    dnl check if user wants their own lex, yacc
    AC_PROG_YACC
    yacc=$YACC
    AC_SUBST(yacc)
    AC_PROG_LEX
    lex=$LEX
    AC_SUBST(lex)
    
    dnl extra check for lex and yacc as these are often not installed
    AC_MSG_CHECKING([if yacc is executable])
    binary=`echo $yacc | awk '{cmd=1; print $cmd}'`
    binary=`which "$binary"`
    if test -x "$binary"
    then
	AC_MSG_RESULT([ yes])
    else
	AC_MSG_RESULT([ no])
	echo
	echo "FATAL ERROR: did not find a valid yacc executable."
	echo "You can either set \$YACC as the full path to yacc"
	echo "in the environment, or install a yacc/bison package."
	exit 1
    fi
    AC_MSG_CHECKING([if lex is executable])
    binary=`echo $lex | awk '{cmd=1; print $cmd}'`
    binary=`which "$binary"`
    if test -x "$binary"
    then
	AC_MSG_RESULT([ yes])
    else
	AC_MSG_RESULT([ no])
	echo
	echo "FATAL ERROR: did not find a valid lex executable."
	echo "You can either set \$LEX as the full path to lex"
	echo "in the environment, or install a lex/flex package."
	exit 1
    fi
])
