# generated automatically by aclocal 1.11.6 -*- Autoconf -*-

# Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
# 2005, 2006, 2007, 2008, 2009, 2010, 2011 Free Software Foundation,
# Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

# 
# Find format of installed man pages.
# 
AC_DEFUN([AC_MANUAL_FORMAT],
  [ have_zipped_manpages=false
    have_bzip2ed_manpages=false
    eval export `grep PCP_MAN_DIR /etc/pcp.conf`
    if test -f "$PCP_MAN_DIR/man1/pcp.1.gz"; then
        have_zipped_manpages=true
    elif test -f "$PCP_MAN_DIR/man1/pcp.1.bz2"; then
        have_bzip2ed_manpages=true
    fi
    AC_SUBST(have_zipped_manpages)
    AC_SUBST(have_bzip2ed_manpages)
  ])

#
# Generic macro, sets up all of the global packaging variables.
# The following environment variables may be set to override defaults:
#   DEBUG OPTIMIZER MALLOCLIB PLATFORM DISTRIBUTION INSTALL_USER INSTALL_GROUP
#   BUILD_VERSION
#
AC_DEFUN([AC_PACKAGE_GLOBALS],
  [ pkg_name="$1"
    AC_SUBST(pkg_name)

    . ./VERSION
    pkg_major=$PKG_MAJOR
    pkg_minor=$PKG_MINOR
    pkg_revision=$PKG_REVISION
    pkg_version=${PKG_MAJOR}.${PKG_MINOR}.${PKG_REVISION}
    AC_SUBST(pkg_major)
    AC_SUBST(pkg_minor)
    AC_SUBST(pkg_revision)
    AC_SUBST(pkg_version)
    pkg_release=$PKG_BUILD
    test -z "$BUILD_VERSION" || pkg_release="$BUILD_VERSION"
    AC_SUBST(pkg_release)

    pkg_build_date=`date +%Y-%m-%d`
    AC_SUBST(pkg_build_date)

    DEBUG=${DEBUG:-'-DDEBUG'}		dnl  -DNDEBUG
    debug_build="$DEBUG"
    AC_SUBST(debug_build)

    OPTIMIZER=${OPTIMIZER:-'-g -O2'}
    opt_build="$OPTIMIZER"
    AC_SUBST(opt_build)

    pkg_user=`id -u -n root`
    test $? -eq 0 || pkg_user=`id -u -n`
    test -z "$INSTALL_USER" || pkg_user="$INSTALL_USER"
    AC_SUBST(pkg_user)

    pkg_group=`id -g -n root`
    test $? -eq 0 || pkg_group=`id -g -n`
    test -z "$INSTALL_GROUP" || pkg_group="$INSTALL_GROUP"
    AC_SUBST(pkg_group)

    pkg_distribution=unknown
    test -f /etc/SuSE-release && pkg_distribution=suse
    test -f /etc/fedora-release && pkg_distribution=fedora
    test -f /etc/redhat-release && pkg_distribution=redhat
    test -f /etc/debian_version && pkg_distribution=debian
    test -z "$DISTRIBUTION" || pkg_distribution="$DISTRIBUTION"
    AC_SUBST(pkg_distribution)

    pkg_doc_dir=`eval echo $datadir`
    pkg_doc_dir=`eval echo $pkg_doc_dir/doc/pcp-gui`
    if test "`echo $pkg_doc_dir | sed 's;/.*\$;;'`" = NONE
    then
	if test -d /usr/share/doc
	then
	    pkg_doc_dir=/usr/share/doc/pcp-gui
	else
	    pkg_doc_dir=/usr/share/pcp-gui
	fi
    fi
    test -z "$DOCDIR" || pkg_doc_dir="$DOCDIR"
    AC_SUBST(pkg_doc_dir)

    pkg_html_dir=`eval echo $datadir`
    pkg_html_dir=`eval echo $pkg_html_dir/doc/pcp-doc`
    if test "`echo $pkg_html_dir | sed 's;/.*\$;;'`" = NONE
    then
	if test -d /usr/share/doc
	then
	    pkg_html_dir=/usr/share/doc/pcp-doc
	else
	    pkg_html_dir=/usr/share/pcp-doc
	fi
    fi
    test -z "$HTMLDIR" || pkg_html_dir="$HTMLDIR"
    AC_SUBST(pkg_html_dir)

    pkg_icon_dir=`eval echo $datadir`
    pkg_icon_dir=`eval echo $pkg_icon_dir/pixmaps`
    if test "`echo $pkg_icon_dir | sed 's;/.*\$;;'`" = NONE
    then
	if test -d /usr/share/doc
	then
	    pkg_icon_dir=/usr/share/doc/pcp-gui/pixmaps
	else
	    pkg_icon_dir=/usr/share/pcp-gui/pixmaps
	fi
    fi
    test -z "$ICONDIR" || pkg_icon_dir="$ICONDIR"
    AC_SUBST(pkg_icon_dir)
  ])

#
# Check if we have a pcp/pmapi.h installed
#
AC_DEFUN([AC_PACKAGE_NEED_PMAPI_H],
  [ AC_CHECK_HEADERS(pcp/pmapi.h)
    if test $ac_cv_header_pcp_pmapi_h = no; then
	echo
	echo 'FATAL ERROR: could not find a valid <pcp/pmapi.h> header.'
	exit 1
    fi
  ])

#
# Check if we have a pcp/pmda.h installed
#
AC_DEFUN([AC_PACKAGE_NEED_PMDA_H],
  [ AC_CHECK_HEADERS([pcp/pmda.h], [], [],
[[#include <pcp/pmapi.h>
#include <pcp/impl.h>
]])
    if test $ac_cv_header_pcp_pmda_h = no; then
	echo
	echo 'FATAL ERROR: could not find a valid <pcp/pmda.h> header.'
	exit 1
    fi
  ])

#
# Check if we have the pmNewContext routine in libpcp
#
AC_DEFUN([AC_PACKAGE_NEED_LIBPCP],
  [ AC_CHECK_LIB(pcp, pmNewContext,, [
	echo
	echo 'FATAL ERROR: could not find a PCP library (libpcp).'
	exit 1
    ])
    libpcp=-lpcp
    AC_SUBST(libpcp)
  ])

#
# Check if we have the __pmSetProgname routine in libpcp
#
AC_DEFUN([AC_PACKAGE_HAVE_PM_SET_PROGNAME],
  [ AC_CHECK_LIB(pcp, __pmSetProgname,
    [ have_pm_set_progname=1 ], [ have_pm_set_progname=0 ])
    AC_SUBST(have_pm_set_progname)
  ])

#
# Check if we have the __pmPathSeparator routine in libpcp
#
AC_DEFUN([AC_PACKAGE_HAVE_PM_PATH_SEPARATOR],
  [ AC_CHECK_LIB(pcp, __pmPathSeparator,
    [ have_pm_path_separator=1 ], [ have_pm_path_separator=0 ])
    AC_SUBST(have_pm_path_separator)
  ])

#
# Check if we have the __pmtimevalNow routine in libpcp
#
AC_DEFUN([AC_PACKAGE_HAVE_PM_TIMEVAL_NOW],
  [ AC_CHECK_LIB(pcp, __pmtimevalNow,
    [ have_pm_timeval_now=1 ], [ have_pm_timeval_now=0 ])
    AC_SUBST(have_pm_timeval_now)
  ])

#
# Check if we have the PM_TYPE_EVENT macro in pmapi.h
#
AC_DEFUN([AC_PACKAGE_HAVE_PM_TYPE_EVENT],
  [ AC_CHECK_DECLS(PM_TYPE_EVENT, [], [], [[#include <pcp/pmapi.h>]])
  ])

#
# Check if we have the pmdaMain routine in libpcp_pmda
#
AC_DEFUN([AC_PACKAGE_NEED_LIBPCP_PMDA],
  [ AC_CHECK_LIB(pcp_pmda, pmdaMain,, [
	echo
	echo 'FATAL ERROR: could not find a PCP PMDA library (libpcp_pmda).'
	exit 1
    ])
    libpcp_pmda=-lpcp_pmda
    AC_SUBST(libpcp_pmda)
  ])

AC_DEFUN([AC_PACKAGE_NEED_QT_QMAKE],
  [ if test -x "$QTDIR/bin/qmake.exe"; then
	QMAKE="$QTDIR/bin/qmake.exe"
    fi
    if test -z "$QMAKE"; then
	AC_PATH_PROGS(QMAKE, [qmake-qt4 qmake],, [$QTDIR/bin:/usr/bin:/usr/lib64/qt4/bin:/usr/lib/qt4/bin])
    fi
    qmake=$QMAKE
    AC_SUBST(qmake)
    AC_PACKAGE_NEED_UTILITY($1, "$qmake", qmake, [Qt make])
  ])

AC_DEFUN([AC_PACKAGE_NEED_QT_VERSION4],
  [ AC_MSG_CHECKING([Qt version])
    eval `$qmake --version | awk '/Using Qt version/ { ver=4; print $ver }' | awk -F. '{ major=1; minor=2; point=3; printf "export QT_MAJOR=%d QT_MINOR=%d QT_POINT=%d\n",$major,$minor,$point }'`
    if test "$QT_MAJOR" -lt 4 ; then
	echo
	echo FATAL ERROR: Qt version 4 does not seem to be installed.
	echo Cannot proceed with the Qt $QT_MAJOR installation found.
	exit 1
    fi
    if test "$QT_MAJOR" -eq 4 -a "$QT_MINOR" -lt 4 ; then
	echo
	echo FATAL ERROR: Qt version 4.$QT_MINOR is too old.
	echo Qt version 4.4 or later is required.
	exit 1
    fi
    AC_MSG_RESULT([$QT_MAJOR.$QT_MINOR.$QT_POINT])
  ])

AC_DEFUN([AC_PACKAGE_NEED_QT_UIC],
  [ if test -z "$UIC"; then
	AC_PATH_PROGS(UIC, [uic-qt4 uic],, [$QTDIR/bin:/usr/bin:/usr/lib64/qt4/bin:/usr/lib/qt4/bin])
    fi
    uic=$UIC
    AC_SUBST(uic)
    AC_PACKAGE_NEED_UTILITY($1, "$uic", uic, [Qt User Interface Compiler])
  ])

AC_DEFUN([AC_PACKAGE_NEED_QT_MOC],
  [ if test -z "$MOC"; then
	AC_PATH_PROGS(MOC, [moc-qt4 moc],, [$QTDIR/bin:/usr/bin:/usr/lib64/qt4/bin:/usr/lib/qt4/bin])
    fi
    moc=$MOC
    AC_SUBST(moc)
    AC_PACKAGE_NEED_UTILITY($1, "$uic", uic, [Qt Meta-Object Compiler])
  ])

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

