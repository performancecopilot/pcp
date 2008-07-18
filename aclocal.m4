# generated automatically by aclocal 1.10.1 -*- Autoconf -*-

# Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004,
# 2005, 2006, 2007, 2008  Free Software Foundation, Inc.
# This file is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

# 
# Find format of installed man pages.
# Always gzipped on Debian, but not Redhat pre-7.0.
# We don't deal with bzip2'd man pages, which Mandrake uses,
# someone will send us a patch sometime hopefully. :-)
# 
AC_DEFUN([AC_MANUAL_FORMAT],
  [ have_zipped_manpages=false
    for d in ${prefix}/share/man ${prefix}/man ; do
        if test -f $d/man1/man.1.gz
        then
            have_zipped_manpages=true
            break
        fi
    done
    AC_SUBST(have_zipped_manpages)
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

    DEBUG=${DEBUG:-'-DDEBUG'}		dnl  -DNDEBUG
    debug_build="$DEBUG"
    AC_SUBST(debug_build)

    OPTIMIZER=${OPTIMIZER:-'-g -O2'}
    opt_build="$OPTIMIZER"
    AC_SUBST(opt_build)

    MALLOCLIB=${MALLOCLIB:-''}		dnl  /usr/lib/libefence.a
    malloc_lib="$MALLOCLIB"
    AC_SUBST(malloc_lib)

    pkg_user=`id -u -n root`
    test $? -eq 0 || pkg_user=`id -u -n`
    test -z "$INSTALL_USER" || pkg_user="$INSTALL_USER"
    AC_SUBST(pkg_user)

    pkg_group=`id -g -n root`
    test $? -eq 0 || pkg_group=`id -g -n`
    test -z "$INSTALL_GROUP" || pkg_group="$INSTALL_GROUP"
    AC_SUBST(pkg_group)

    pkg_distribution=`uname -s`
    test -z "$DISTRIBUTION" || pkg_distribution="$DISTRIBUTION"
    AC_SUBST(pkg_distribution)

    pkg_platform=`uname -s | tr 'A-Z' 'a-z' | sed -e 's/irix64/irix/'`
    test -z "$PLATFORM" || pkg_platform="$PLATFORM"
    AC_SUBST(pkg_platform)
  ])

#
# Find base location of installed documents, esp. the html manual.
#
AC_DEFUN([AC_PACKAGE_PATHS],
  [ pkg_name="$1"
    pkg_doc_dir=`eval echo "${datadir}/doc/${pkg_name}"`
    pkg_doc_dir=`eval echo "${pkg_doc_dir}"`
    AC_SUBST(pkg_doc_dir)
    pkg_html_dir=`eval echo "${pkg_doc_dir}"/html`
    pkg_html_dir=`eval echo "${pkg_html_dir}"`
    AC_SUBST(pkg_html_dir)
    pkg_desktop_dir=`eval echo "${datadir}"/applications`
    pkg_desktop_dir=`eval echo "${pkg_desktop_dir}"`
    AC_SUBST(pkg_desktop_dir)
    pkg_icon_dir=`eval echo "${datadir}"/pixmaps`
    pkg_icon_dir=`eval echo "${pkg_icon_dir}"`
    AC_SUBST(pkg_icon_dir)
  ])

#
# Check if we have a pcp/pmapi.h installed
#
AC_DEFUN([AC_PACKAGE_NEED_PMAPI_H],
  [ if test -n "$PCP_DIR"; then
	CFLAGS="$CFLAGS -I$PCP_DIR"
	CPPFLAGS="$CPPFLAGS -I$PCP_DIR/include"
    fi
    AC_CHECK_HEADERS(pcp/pmapi.h)
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
  [ if test -n "$PCP_DIR"; then
	CFLAGS="$CFLAGS -I$PCP_DIR/include"
	CPPFLAGS="$CPPFLAGS -I$PCP_DIR/include"
    fi
    AC_CHECK_HEADERS([pcp/pmda.h], [], [],
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
  [ if test -n "$PCP_DIR"; then
	LDFLAGS="$LDFLAGS -L$PCP_DIR/local/bin"
    fi
    AC_CHECK_LIB(pcp, pmNewContext,, [
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
  [ if test -n "$PCP_DIR"; then
	LDFLAGS="$LDFLAGS -L$PCP_DIR/local/bin"
    fi
    AC_CHECK_LIB(pcp, __pmSetProgname,
    [ have_pm_set_progname=1 ], [ have_pm_set_progname=0 ])
    AC_SUBST(have_pm_set_progname)
  ])

#
# Check if we have the pmdaMain routine in libpcp_pmda
#
AC_DEFUN([AC_PACKAGE_NEED_LIBPCP_PMDA],
  [ if test -n "$PCP_DIR"; then
	LDFLAGS="$LDFLAGS -L$PCP_DIR/local/bin"
    fi
    AC_CHECK_LIB(pcp_pmda, pmdaMain,, [
	echo
	echo 'FATAL ERROR: could not find a PCP PMDA library (libpcp_pmda).'
	exit 1
    ])
    libpcp_pmda=-lpcp_pmda
    AC_SUBST(libpcp_pmda)
  ])

AC_DEFUN([AC_PACKAGE_NEED_QT_QMAKE],
  [ if test -x "$QTDIR\bin\qmake.exe"; then
	QMAKE="$QTDIR\bin\qmake.exe"
    fi
    if test -z "$QMAKE"; then
	AC_PATH_PROGS(QMAKE, [qmake-qt4 qmake],, [$QTDIR/bin:/usr/bin:/usr/lib64/qt4/bin:/usr/lib/qt4/bin])
    fi
    qmake=$QMAKE
    AC_SUBST(qmake)
    AC_PACKAGE_NEED_UTILITY($1, "$qmake", qmake, [Qt make])
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
#  CC MAKE TAR MAKEDEPEND AWK SED ECHO SORT RPM
#
AC_DEFUN([AC_PACKAGE_UTILITIES],
  [ AC_PROG_CXX
    cc="$CXX"
    AC_SUBST(cc)
    AC_PACKAGE_NEED_UTILITY($1, "$cc", cc, [C++ compiler])

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

    if test -z "$RPM"; then
        AC_PATH_PROG(RPM, rpm,, /bin:/usr/bin)
    fi
    rpm=$RPM
    AC_SUBST(rpm)

    dnl .. and what version is rpm
    rpm_version=0
    test -n "$RPM" && test -x "$RPM" && rpm_version=`$RPM --version \
                        | awk '{print $NF}' | awk -F. '{V=1; print $V}'`
    AC_SUBST(rpm_version)
    dnl At some point in rpm 4.0, rpm can no longer build rpms, and
    dnl rpmbuild is needed (rpmbuild may go way back; not sure)
    dnl So, if rpm version >= 4.0, look for rpmbuild.  Otherwise build w/ rpm
    if test $rpm_version -ge 4; then
        AC_PATH_PROG(RPMBUILD, rpmbuild)
        rpmbuild=$RPMBUILD
    else
        rpmbuild=$RPM
    fi
    AC_SUBST(rpmbuild)

    dnl check if the dpkg program is available
    if test -z "$DPKG"
    then
	AC_PATH_PROG(DPKG, dpkg)
    fi
    dpkg=$DKPG
    AC_SUBST(dpkg)

    dnl Check for mac PackageMaker
    AC_MSG_CHECKING([for PackageMaker])
    if test -z "$PACKAGE_MAKER"
    then
	if test -x /Developer/Applications/PackageMaker.app/Contents/MacOS/PackageMaker       
	then # Darwin 6.x
	    package_maker=/Developer/Applications/PackageMaker.app/Contents/MacOS/PackageMaker
	    AC_MSG_RESULT([ yes (darwin 6.x)])
	elif test -x /Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker
	then # Darwin 7.x
	    AC_MSG_RESULT([ yes (darwin 7.x)])
	    package_maker=/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker
	else
	    AC_MSG_RESULT([ no])
	fi
    else
	package_maker="$PACKAGE_MAKER"
    fi
    AC_SUBST(package_maker)

    dnl check if the hdiutil program is available
    test -z "$HDIUTIL" && AC_PATH_PROG(HDIUTIL, hdiutil)
    hdiutil=$HDIUTIL
    AC_SUBST(hdiutil)
  ])

