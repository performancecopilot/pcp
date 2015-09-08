dnl
dnl Copyright (c) 2012-2015 Red Hat.
dnl Copyright (c) 2008 Aconex.  All Rights Reserved.
dnl Copyright (c) 2000-2004,2008 Silicon Graphics, Inc.  All Rights Reserved.
dnl 
dnl This program is free software; you can redistribute it and/or modify it
dnl under the terms of the GNU General Public License as published by the
dnl Free Software Foundation; either version 2 of the License, or (at your
dnl option) any later version.
dnl 
dnl This program is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
dnl or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
dnl for more details.
dnl 

dnl unpacking check - this file must exist
AC_INIT(src/include/pcp/pmapi.h)
AC_PREREQ([2.60])dnl

dnl Irix build issue ... use the tools from the local filesystems
unset ROOT TOOLROOT

AC_ARG_WITH(
    [64bit],
    [AS_HELP_STRING([--with-64bit],
		    [turn on 64 bit compilation mode (default is platform dependent)])],
    [use_64bit=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-64bit=$withval"])

AC_ARG_WITH(
    [threads],
    [AC_HELP_STRING([--with-threads],
		    [enable support for multiple threads (default is on)])],
    [do_threads=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-threads=$withval"],
    [do_threads=check])

AC_ARG_WITH(
    [secure-sockets],
    [AC_HELP_STRING([--with-secure-sockets],
		    [enable support for secure sockets (default is on)])],
    [do_secure=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-secure-sockets=$withval"],
    [do_secure=check])

AC_ARG_WITH(
    [static-probes],
    [AC_HELP_STRING([--with-static-probes],
		    [enable support for static probes (default is on)])],
    [do_probes=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-static-probes=$withval"],
    [do_probes=check])

AC_ARG_WITH(
    [infiniband],
    [AC_HELP_STRING([--with-infiniband],
		    [enable support for Infiniband metrics (default is on)])],
    [do_infiniband=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-infiniband=$withval"],
    [do_infiniband=check])

AC_ARG_WITH(
    [user],
    [AS_HELP_STRING([--with-user],
		    [user account under which daemons run (default is pcp)])],
    [pcp_user=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-user-account=$withval"],
    [pcp_user=pcp])
AC_SUBST(pcp_user)

AC_ARG_WITH(
    [group],
    [AS_HELP_STRING([--with-group],
		    [user group under which daemons run (default is pcp)])],
    [pcp_group=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-group-account=$withval"],
    [pcp_group=pcp])
AC_SUBST(pcp_group)

AC_ARG_WITH([discovery],
    [AC_HELP_STRING([--with-discovery],
                    [enable support for service discovery (default is on)])],
    [do_discovery=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-discovery=$withval"],
    [do_discovery=check])

AC_ARG_WITH([systemd],
    [AC_HELP_STRING([--with-systemd],
                    [enable support for systemd services (default is on)])],
    [do_systemd=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-systemd=$withval"],
    [do_systemd=check])

AC_ARG_WITH([qt],
    [AC_HELP_STRING([--with-qt],
                    [enable support for tools requiring Qt (default is on)])],
    [do_qt=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-qt=$withval"],
    [do_qt=check])

AC_ARG_WITH([python],
    [AC_HELP_STRING([--with-python],
                    [enable support for tools requiring Python (default is on)])],
    [do_python=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-python=$withval"],
    [do_python=check])

AC_ARG_WITH([python3],
    [AC_HELP_STRING([--with-python3],
                    [enable support for tools requiring Python3 (default is on)])],
    [do_python3=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-python3=$withval"],
    [do_python3=check])

AC_ARG_WITH([books],
    [AC_HELP_STRING([--with-books],
                    [enable building of the PCP books (default is off)])],
    [do_books=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-books=$withval"],
    [do_books=no])

AC_ARG_WITH([books-brand],
    [AC_HELP_STRING([--with-books-brand=BRAND],
                    [choose the docbook brand when building the PCP books (default is common)])],
    [do_books_brand=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-books-brand=$withval"],
    [do_books_brand=common])

AC_ARG_WITH([papi],
    [AC_HELP_STRING([--with-papi@<:@=DIR|=MODULE@:>@],
		   [enable performance api counter pmda (default is on)
		   The optional argument may be a full path of a PAPI installation prefix,
		   or a pkgconfig module name.])],
    [do_papi=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-papi=$withval"],
    [do_papi=check])

AC_ARG_WITH([perfevent],
    [AC_HELP_STRING([--with-perfevent],
		   [enable perfevent pmda (default is on)])],
    [do_perfevent=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-perfevent=$withval"],
    [do_perfevent=check])

AC_ARG_WITH([pmdajson],
    [AC_HELP_STRING([--with-pmdajson],
		   [enable JSON pmda (default is on)])],
    [do_pmdajson=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdajson=$withval"],
    [do_pmdajson=check])

AC_ARG_WITH([manager],
    [AC_HELP_STRING([--with-manager],
		   [enable daemon manager (default is on)])],
    [do_manager=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-manager=$withval"],
    [do_manager=check])

AC_ARG_WITH([webapi],
    [AC_HELP_STRING([--with-webapi],
		   [enable REST API daemon (default is on)])],
    [do_webapi=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-webapi=$withval"],
    [do_webapi=check])

AC_ARG_WITH([webjs],
    [AC_HELP_STRING([--with-webjs],
                    [enable building of webjs packages (default is off)])],
    [do_webjs=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-webjs=$withval"],
    [do_webjs=no])

AC_ARG_WITH([vector],
    [AC_HELP_STRING([--with-vector],
                    [enable building of vector package (default is off)])],
    [do_vector=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-vector=$withval"],
    [do_vector=no])

dnl things set in the environment by Makepkgs never make it into an RPM
dnl build where the environment is stripped ... use a --with-foo=path
dnl construct to workaround this

AC_ARG_WITH([make],
    [AC_HELP_STRING([--with-make],
                    [path to GNU compatible make(1) (default is empty for auto discovery)])],
    [MAKE=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-make=$withval"],
    [MAKE=''])

AC_ARG_WITH([tar],
    [AC_HELP_STRING([--with-tar],
                    [path to GNU compatible tar(1) (default is empty for auto discovery)])],
    [TAR=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-tar=$withval"],
    [TAR=''])

AC_ARG_WITH([zip],
    [AC_HELP_STRING([--with-zip],
                    [path to GNU compatible zip(1) (default is empty for auto discovery)])],
    [ZIP=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-zip=$withval"],
    [ZIP=''])

#
# Note: the following environment variables may be
# set to override the defaults.
#
# MAKE CC CPP LD LEX YACC INSTALL AWK SED ECHO
#

dnl Guess target platfrom
AC_CANONICAL_SYSTEM
if test -z "$target"
then
    echo '
FATAL ERROR: Cannot guess your target, try explicit specification
             using --target or mailto: pcp@oss.sgi.com to ask.'
    rm -rf conftest conftest.*
    exit 1
else
    dnl Remove 4th name component, if present, from target, target_os,
    dnl  build and build_os. Squash all x86 cpus into one LCD form - i386
    target=`echo $target | sed '[s/^\([^-][^-]*-[^-][^-]*-[^-][^-]*\)-.*$/\1/]'`
    target_os=`echo $target_os | sed '[s/solaris2\..*/solaris/]'`
    target_os=`echo $target_os | sed '[s/^\([^-][^-]*\)-.*$/\1/]' | sed '[s/[\.0-9]*//g]'`

    build=`echo $build | sed '[s/^\([^-][^-]*-[^-][^-]*-[^-][^-]*\)-.*$/\1/]'`
    build_os=`echo $build_os | sed '[s/solaris2\..*/solaris/]'`
    build_os=`echo $build_os | sed '[s/^\([^-][^-]*\)-.*$/\1/]'`
fi

echo Building on $build for $target 
echo "Build: os=$build_os cpu=$build_cpu"
echo "Target: os=$target_os cpu=$target_cpu"

if test "$cross_compiling" = "yes"; then
    if test -f ./config.$target_os; then
	. ./config.$target_os
    else
	echo
	echo "
FATAL ERROR: Cannot perform cross-compilation without a file to source
             configuration information from (config.$target_os is missing)"
	rm -rf conftest conftest.*
	exit 1
    fi
fi
AC_SUBST(cross_compiling)

dnl CFLAGS setting is a co-dependency between here and PCFLAGS in
dnl src/include/builddefs.in ... need to be the same in both places

target_distro=$target_os
if test $target_os = linux
then
    AC_DEFINE(IS_LINUX, [1], [Platform is Linux])
    test -f /etc/SuSE-release && target_distro=suse
    test -f /etc/fedora-release && target_distro=fedora
    test -f /etc/redhat-release && target_distro=redhat
    test -f /etc/debian_version && target_distro=debian
    test -f /etc/slackware-version && target_distro=slackware
    test -f /etc/gentoo-release && target_distro=gentoo
    test -f /etc/mandriva-release && target_distro=mandriva
    export CFLAGS="-fPIC -fno-strict-aliasing -D_GNU_SOURCE"
    pcp_platform_paths='/usr/bin/X11:/usr/local/bin'
    pcp_ps_all_flags=-efw
elif test $target_os = darwin
then
    AC_DEFINE(IS_DARWIN, [1], [Platform is Darwin (Mac OS X)])
    target_distro=macosx
    version=`sw_vers -productVersion | sed -e 's/\.//' -e 's/\..*//g'`
    CFLAGS="-fPIC -no-cpp-precomp -fno-strict-aliasing -arch i386"
    if test $version -ge 106
    then
	target_distro=cocoa
	CFLAGS="$CFLAGS -arch x86_64"
    fi
    export CFLAGS
    pcp_platform_paths='/usr/local/bin'
    pcp_ps_all_flags=auxww
elif test $target_os = mingw
then
    AC_DEFINE(IS_MINGW, [1], [Platform is MinGW (Windows)])
    CFLAGS="-fno-strict-aliasing"
    pcp_platform_paths=''
    pcp_ps_all_flags=-efW
elif test $target_os = solaris
then
    AC_DEFINE(IS_SOLARIS, [1], [Platform is Solaris])
    export CFLAGS_IF_GCC="-fPIC -fno-strict-aliasing"
    export CFLAGS_IF_SUNCC="-fPIC -xalias_level=any"
    pcp_platform_paths='/usr/bin/X11:/usr/local/bin:/opt/sfw/bin:/opt/csw/bin'
    pcp_ps_all_flags=-ef
elif test $target_os = aix
then
    AC_DEFINE(IS_AIX, [1], [Platform is AIX])
    export CFLAGS="-qcpluscmt"
    pcp_platform_paths='/usr/bin/X11:/usr/local/bin'
    pcp_ps_all_flags=-ef
elif test $target_os = freebsd || test $target_os = kfreebsd
then
    AC_DEFINE(IS_FREEBSD, [1], [Platform is FreeBSD])
    test -f /etc/debian_version && target_distro=debian
    export CFLAGS="-fPIC -fno-strict-aliasing -D_GNU_SOURCE"
    if $target_os = kfreebsd
    then
	pcp_platform_paths='/usr/bin/X11:/usr/local/bin'
	pcp_ps_all_flags=-efw
    else
	pcp_platform_paths='/usr/bin/X11:/usr/bsd'
	pcp_ps_all_flags=auxww
    fi
elif test $target_os = gnu
then
    AC_DEFINE(IS_GNU, [1], [Platform is GNU Hurd])
    test -f /etc/debian_version && target_distro=debian
    export CFLAGS="-fPIC -fno-strict-aliasing -D_GNU_SOURCE"
    pcp_platform_paths='/usr/bin/X11:/usr/local/bin'
    pcp_ps_all_flags=-efw
elif test $target_os = netbsdelf
then
    target_os=netbsd
    AC_DEFINE(IS_NETBSD, [1], [Platform is NetBSD])
    export CFLAGS="-fPIC -fno-strict-aliasing -D_GNU_SOURCE -D_NETBSD_SOURCE"
    pcp_platform_paths='/usr/pkg/bin'
    pcp_ps_all_flags=auxww
else
    echo
    echo "FATAL ERROR: need platform-specific customization for \"$target_os\""
    rm -rf conftest conftest.*
    exit 1
fi
AC_SUBST(pcp_platform_paths)
AC_SUBST(pcp_ps_all_flags)

PKG_PROG_PKG_CONFIG
if test -z "$PKG_CONFIG"
then
    echo
    echo "FATAL ERROR: did not find a pkg-config executable."
    echo "You can either set \$PKG_CONFIG as the full path to pkg-config"
    echo "in the environment, or install the pkgconfig package."
    rm -rf conftest conftest.*
    exit 1
fi

dnl check if user wants their own C compiler
cflags_abi=
AC_PROG_CC(suncc gcc cc)
if test $target_os = solaris
then
    AC_PATH_PROG(CCPATH,$CC,$CC)
    cc=$CCPATH
    if test "$GCC" = "yes"
    then
	CFLAGS="$CFLAGS $CFLAGS_IF_GCC"
    else
	CFLAGS="$CFLAGS $CFLAGS_IF_SUNCC"
    fi
    if test "$use_64bit" = "no"
    then
	:
    else
	AC_MSG_CHECKING([for 64 bit Solaris host])
	case `isainfo -k`
	in
	    amd64|sparcv9)
		cflags_abi=-m64
		CFLAGS="$CFLAGS -m64"
		LDFLAGS="$LDFLAGS -m64"
		AC_MSG_RESULT(yes)
		;;
	    *)
		AC_MSG_RESULT(no)
		;;
	esac
    fi
else
    cc=$CC
fi
AC_SUBST(cc)
cc_is_gcc=$GCC
AC_SUBST(cc_is_gcc)
AC_SUBST(cflags_abi)
dnl these become CFLAGS and LDFLAGS for the build
PCFLAGS="$CFLAGS"
PLDFLAGS=$LDFLAGS

if echo "$CFLAGS" | grep 'D_FORTIFY_SOURCE' >/dev/null
then
    dnl for cc use locally in configure (not exported via $PCFLAGS),
    dnl add -O in case -D_FORTIFY_SOURCE has been added to CFLAGS above,
    dnl as -D_FORTIFY_SOURCE requires -O but we've separated optimization
    dnl flags out into $CFLAGS_OPT for our build infrastructure
    CFLAGS="-O $PCFLAGS"
fi

dnl some versions of gcc (e.g. version 4.8.4 on Gentoo Hardened) have added
dnl a predefined for _FORTIFY_SOURCE but only when -O is specified ... so
dnl to be safe we have -U_FORTIFY_SOURCE before -D_FORTIFY_SOURCE=2 below

# Add some security-related gcc flags
if echo "$CFLAGS" | grep stack-protector >/dev/null
then
    dnl already set, probably from the environment
    :
else
    AC_ARG_ENABLE([ssp],
    [AS_HELP_STRING([--disable-ssp], [disable gcc stack-protector])])
    AS_IF([test "x$cc_is_gcc" = xyes -a "x$enable_ssp" != xno ],[
       save_CFLAGS="$CFLAGS"
       CFLAGS="$CFLAGS -O -fstack-protector-all -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2"
       AC_COMPILE_IFELSE([AC_LANG_SOURCE([int something ();])], [
	   AC_MSG_NOTICE([Compiling with gcc -fstack-protector-all et al.])
	   PCFLAGS="$PCFLAGS -fstack-protector-all -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2"],[
	   AC_MSG_NOTICE([Compiler does not support -fstack-protector-all et al.])
	   CFLAGS="$save_CFLAGS"
       ])
    ])
fi
AC_SUBST(PCFLAGS)
AC_SUBST(PLDFLAGS)

# Check for even more security-related gcc/linker flags, useful for daemons
AC_ARG_ENABLE([pie],
  [AS_HELP_STRING([--disable-pie], [disable position-independent-executable])])
AS_IF([test "x$cc_is_gcc" = xyes -a "x$enable_pie" != xno],[
   PIECFLAGS='-fPIE'
   PIELDFLAGS='-pie -Wl,-z,relro -Wl,-z,now'
   save_CFLAGS="$CFLAGS"
   save_LDFLAGS="$LDFLAGS"
   CFLAGS="$CFLAGS $PIECFLAGS"
   LDFLAGS="$LDFLAGS $PIELDFLAGS"
   AC_LINK_IFELSE([AC_LANG_SOURCE([void main () {}])], [
       AC_MSG_NOTICE([Compiling with gcc pie et al.])
   ], [
       AC_MSG_NOTICE([Compiler does not support -pie et al.])
       PIECFLAGS=""
       PIELDFLAGS=""
   ])
   CFLAGS="$save_CFLAGS"
   LDFLAGS="$save_LDFLAGS"
])
AC_SUBST(PIELDFLAGS)
AC_SUBST(PIECFLAGS)

# Check for support for symbol hiding via gcc flags
AC_ARG_ENABLE([visibility],
  [AS_HELP_STRING([--disable-visibility], [disable gcc symbol visibility])])
AS_IF([test "x$cc_is_gcc" = xyes -a "$enable_visibility" != no],[
   INVISIBILITY="-fvisibility=hidden"
   save_CFLAGS="$CFLAGS"
   CFLAGS="$CFLAGS -c $INVISIBILITY"
   AC_COMPILE_IFELSE([AC_LANG_SOURCE([int visible () { return 0; }])], [
       AC_MSG_NOTICE([Compiling DSO PMDAs with gcc -fvisibility=hidden])
   ], [
       AC_MSG_NOTICE([Compiler does not support -fvisibility.])
       INVISIBILITY=""
   ])
   CFLAGS="$save_CFLAGS"
])
AC_SUBST(INVISIBILITY)

AC_PROG_CXX
cxx=$CXX
AC_SUBST(cxx)

# Prevent shared libaries from being built for libpcp and other core libraries
AC_ARG_ENABLE([shared],
  [AS_HELP_STRING([--disable-shared], [disable core shared libary generation])],
  [PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --disable-shared=$withval"])
AC_SUBST(enable_shared)

dnl Check for static probes (dtrace, systemtap)
if test "$do_probes" = "check" -o "$do_probes" = "yes"
then
    enable_probes=true
    AC_CHECK_PROGS(DTRACE, dtrace, [])
    if test -z "$DTRACE"
    then
	if test "$do_probes" = "yes"
	then
	    AC_MSG_ERROR([cannot enable static probes - no dtrace executable])
	fi
	enable_probes=false
    fi
    AC_CHECK_HEADERS([sys/sdt.h], [
	if $enable_probes = true
	then
	    if test $target_os = kfreebsd
	    then
		enable_probes=false
	    elif test $target_os = freebsd
	    then
		dnl dtrace support in FreeBSD depends on configuration of kernel
		rm -f conftest.d conftest.h
		cat <<End-of-File >conftest.d
provider eek {
probe urk(int);
};
End-of-File
		if dtrace -s conftest.d -h >/dev/null 2>&1
		then
		    if test ! -f conftest.h
		    then
			enable_probes=false
			if test "$do_probes" = "yes"
			then
			    AC_MSG_ERROR([cannot enable static probes - no .h from dtrace -h])
			fi
		    fi
		else
		    enable_probes=false
		    if test "$do_probes" = "yes"
		    then
			AC_MSG_ERROR([cannot enable static probes - dtrace failed])
		    fi
		fi
	    fi
	fi
    ], [
	enable_probes=false
	if test "$do_probes" = "yes"
	then
	    AC_MSG_ERROR([cannot enable static probes - no SDT header])
	fi
    ])

    if test "$enable_probes" = "true"
    then
        AC_DEFINE(HAVE_STATIC_PROBES, [1], [Static probes (dtrace, systemtap)])
    fi
    AC_SUBST(enable_probes)
    AC_SUBST(DTRACE)
fi

dnl Check for service discovery mechanisms (DNS-SD, Avahi)
AS_IF([test "x$do_discovery" != "xno"], [
    # on Mac OS X, dns_sd.h

    # on Linux, aloha Avahi
    enable_avahi=true
    PKG_CHECK_MODULES([avahi], [avahi-client],
        [AC_CHECK_LIB(avahi-client, avahi_client_new,
    		      [lib_for_avahi="-lavahi-common -lavahi-client"],
                      [enable_avahi=false])
        ],[enable_avahi=false])
    AC_CHECK_HEADERS([avahi-client/publish.h],, [enable_avahi=false])
    AC_CHECK_HEADERS([avahi-common/alternative.h],, [enable_avahi=false])

    if test "$enable_avahi" = "true"
    then
	AC_SUBST(lib_for_avahi)
	AC_SUBST(avahi_CFLAGS)
	AC_DEFINE(HAVE_AVAHI, [1], [Service discovery via Avahi])
	enable_discovery=true
    fi

    if test "$do_discovery" != "check" -a "$enable_discovery" != "true"
    then
	AC_MSG_ERROR([cannot enable service discovery - no supported mechanisms])
    fi
    if test "$enable_discovery" = "true"
    then
	AC_DEFINE(HAVE_SERVICE_DISCOVERY, [1], [Service discovery mechanisms])
    fi
])
AC_SUBST(enable_discovery)
AC_SUBST(enable_avahi)

dnl Check for systemd services
enable_systemd=false
AS_IF([test "x$do_systemd" != "xno"], [
    enable_systemd=true

    PKG_CHECK_VAR([SYSTEMD_SYSTEMUNITDIR], [systemd], [systemdsystemunitdir],
	[pcp_systemdunit_dir=$SYSTEMD_SYSTEMUNITDIR], [enable_systemd=false])
    AC_SUBST(pcp_systemdunit_dir)
    if test -z "$pcp_systemdunit_dir"
    then
	if test "$do_systemd" = "yes"
	then
	    AC_MSG_ERROR([cannot enable systemd support - no systemunitdir path])
	fi
	enable_systemd=false
    fi
])
AC_SUBST(enable_systemd)

# NB: No AC_PREFIX_DEFAULT is needed, as the default configure invocation
# targets a build for non-system directories such as /usr/local.
# AC_PREFIX_DEFAULT([])

. ./VERSION.pcp
PACKAGE_VERSION=${PACKAGE_MAJOR}.${PACKAGE_MINOR}.${PACKAGE_REVISION}
AC_SUBST(PACKAGE_VERSION)
AC_SUBST(PACKAGE_MAJOR)
AC_SUBST(PACKAGE_MINOR)
AC_SUBST(PACKAGE_REVISION)
AC_SUBST(PACKAGE_BUILD)

if test -z "$PACKAGE_BUILD_DATE" ; then
    PACKAGE_BUILD_DATE=`date +%Y-%m-%d`
fi
AC_SUBST(PACKAGE_BUILD_DATE)

if test -z "$PACKAGE_DISTRIBUTION" ; then
    PACKAGE_DISTRIBUTION=$target_distro
fi
AC_SUBST(PACKAGE_DISTRIBUTION)

dnl generate PM_VERSION_CURRENT for header
pm_version_current=`awk </dev/null "BEGIN { print (${PACKAGE_MAJOR}*256+${PACKAGE_MINOR})*256+${PACKAGE_REVISION} }"`
AC_SUBST(pm_version_current)

dnl output header files with cpp defs HAVE_*, etc
AC_CONFIG_HEADERS(src/include/pcp/config.h src/include/pcp/configsz.h)
AC_OUTPUT(src/include/pcp/platform_defs.h)

AC_CHECK_PROGS(GIT, git)
AC_SUBST(GIT)

dnl check if python available for the build and runtime
AC_CHECK_PROGS(PYTHON, python)
AC_SUBST(PYTHON)

dnl check if python3 available for the build and runtime
AC_CHECK_PROGS(PYTHON3, python3)
AC_SUBST(PYTHON3)

dnl check if python tools/packages wanted (need python >= 2.6)
enable_python2=false
AS_IF([test "x$do_python" != "xno"], [
    enable_python2=true
    if test -z "$PYTHON"
    then
	enable_python2=false
    else
	AC_MSG_CHECKING([Python version])
	eval `$PYTHON -V 2>&1 | awk '/^Python/ { ver=2; print $ver }' | awk -F. '{ major=1; minor=2; point=3; printf "export PY_MAJOR=%d PY_MINOR=%d PY_POINT=%d\n",$major,$minor,$point }'`
	AC_MSG_RESULT([$PY_MAJOR.$PY_MINOR.$PY_POINT])
	if test "$PY_MAJOR" -lt 2; then
	    echo WARNING: Python version 2.6 or later does not seem to be installed.
	    echo Cannot proceed with the Python $PY_MAJOR installation found.
	    enable_python2=false
	fi
	if test "$PY_MAJOR" -eq 2 -a "$PY_MINOR" -lt 6 ; then
	    echo WARNING: Python version 2.$PY_MINOR is too old.
	    echo Python version 2.6 or later is required for Python builds.
	    enable_python2=false
	fi
	AC_CHECK_HEADERS([python${PY_MAJOR}.${PY_MINOR}/Python.h], [], [
	    echo WARNING: Python version $PY_MAJOR.$PY_MINOR header file missing.
	    echo Cannot proceed with the Python $PY_MAJOR installation found.
	    enable_python2=false
	])
    fi

    if test "$do_python" != "check" -a "$enable_python2" != "true"
    then
	AC_MSG_ERROR([cannot enable Python - no supported version found])
    fi
])
AC_SUBST(enable_python2)

dnl check if python3 tools/packages wanted (need python >= 3.3)
enable_python3=false
AS_IF([test "x$do_python3" != "xno"], [
    enable_python3=true
    if test -z "$PYTHON3"
    then
	enable_python3=false
    else
	AC_MSG_CHECKING([Python3 version])
	eval `$PYTHON3 -V 2>&1 | awk '/^Python/ { ver=2; print $ver }' | awk -F. '{ major=1; minor=2; point=3; printf "export PY_MAJOR=%d PY_MINOR=%d PY_POINT=%d\n",$major,$minor,$point }'`
	AC_MSG_RESULT([$PY_MAJOR.$PY_MINOR.$PY_POINT])
	if test "$PY_MAJOR" -lt 3; then
	    echo WARNING: Python version 3.3 or later does not seem to be installed.
	    echo Cannot proceed with the Python $PY_MAJOR installation found.
	    enable_python3=false
	fi
	if test "$PY_MAJOR" -eq 3 -a "$PY_MINOR" -lt 3 ; then
	    echo WARNING: Python version 3.$PY_MINOR is too old.
	    echo Python version 3.3 or later is required for Python builds.
	    enable_python3=false
	fi
	AC_CHECK_HEADERS([python${PY_MAJOR}.${PY_MINOR}m/Python.h], [], [
	    echo WARNING: Python version $PY_MAJOR.$PY_MINOR header file missing.
	    echo Cannot proceed with the Python $PY_MAJOR installation found.
	    enable_python3=false
	])
    fi

    if test "$do_python3" != "check" -a "$enable_python3" != "true"
    then
	AC_MSG_ERROR([cannot enable Python3 - no supported version found])
    fi
])
AC_SUBST(enable_python3)

dnl convenience macro to determine if either python is available
AC_MSG_CHECKING([for any python version])
if test "$cross_compiling" = "yes"; then
    ans=$have_python
    echo "cross-compile -> \"$ans\"" >&5
elif test "$enable_python2" = "true" -o "$enable_python3" = "true"
then
    have_python=true
else
    have_python=false
fi
AC_SUBST(have_python)

dnl choose the prefered python executable (py2 -> py3 transtion)
if test "$enable_python3" = "true"
then
    pcp_python_prog=$PYTHON3
elif test "$enable_python2" = "true"
then
    pcp_python_prog=$PYTHON
else
    pcp_python_prog=""
fi
AC_SUBST(pcp_python_prog)

dnl check availability of some Python modules
dnl usage PCP_CHECK_PYTHON_MODULE(module_name) or
dnl PCP_CHECK_PYTHON_MODULE(module_name, <import string>)
dnl default <import string> is "import module_name"
dnl
dnl sets $have_python_<module_name> as true or false
dnl
AC_DEFUN(PCP_CHECK_PYTHON_MODULE,
  [ AC_MSG_CHECKING(for Python module $1)
    if test -n "$2"
    then
	txt="$2"
    else
	txt="import $1"
    fi
    $pcp_python_prog -c 'import sys;
'"$txt" >>config.log 2>&1
    if test "$?" = 0
    then
	AC_MSG_RESULT(yes)
	eval have_python_$1=true
    else
	AC_MSG_RESULT(no)
	eval have_python_$1=false
    fi
  ])
PCP_CHECK_PYTHON_MODULE(jsonpointer)
AC_SUBST(have_python_jsonpointer)
PCP_CHECK_PYTHON_MODULE(six)
AC_SUBST(have_python_six)

pmda_json=false
AS_IF([test "x$do_pmdajson" = "xyes"], [pmda_json=true])
AS_IF([test "x$do_pmdajson" = "xcheck"], [
    dnl JSON PMDA needs both the six and jsonpointer Python modules
    $have_python_jsonpointer && $have_python_six && pmda_json=true
])
AC_SUBST(PMDA_JSON, $pmda_json)

dnl check if perl available for the build and runtime
AC_CHECK_PROGS(PERL, perl)
AC_SUBST(PERL)

AC_MSG_CHECKING([for any perl version])
if test "$cross_compiling" = "yes"; then
    ans=$have_perl
    echo "cross-compile -> \"$ans\"" >&5
elif test -n "$PERL"
then
    pcp_perl_prog=$PERL
    have_perl=true
else
    pcp_perl_prog=""
    have_perl=false
fi
AC_SUBST(pcp_perl_prog)
AC_SUBST(have_perl)

qmake=$QMAKE
enable_qt=false
qt_release=release
AS_IF([test "x$do_qt" != "xno"], [
    enable_qt=true
    
    if test -z "$QMAKE"
    then
	AC_PATH_PROGS(QMAKE, [qmake-qt4 qmake],, [$QTDIR/bin:/usr/bin:/usr/lib64/qt4/bin:/usr/lib/qt4/bin])
	test "x$cc_is_gcc" = xyes -a $target_os = solaris && QMAKE="$QMAKE -spec solaris-g++"
    fi
    qmake=$QMAKE
    if test -z "$QMAKE"
    then
	enable_qt=false
    else
	AC_MSG_CHECKING([Qt version])
	eval `$qmake --version | awk '/Using Qt version/ { ver=4; print $ver }' | awk -F. '{ major=1; minor=2; point=3; printf "export QT_MAJOR=%d QT_MINOR=%d QT_POINT=%d\n",$major,$minor,$point }'`
	AC_MSG_RESULT([$QT_MAJOR.$QT_MINOR.$QT_POINT])
	if test "$QT_MAJOR" -lt 4; then
	    echo WARNING: Qt version 4.4 or later does not seem to be installed.
	    echo Cannot proceed with the Qt $QT_MAJOR installation found.
	    enable_qt=false
	fi
	if test "$QT_MAJOR" -eq 4 -a "$QT_MINOR" -lt 4 ; then
	    echo WARNING: Qt version 4.$QT_MINOR is too old.
	    echo Qt version 4.4 or later is required for Qt builds.
	    enable_qt=false
	fi
    fi

    if test "$do_qt" != "check" -a "$enable_qt" != "true"
    then
	AC_MSG_ERROR([cannot enable Qt - no supported Qt installation found])
    elif test "$do_qt" = "debug"
    then
	dnl Debug/release style build? (non-debug by default)
	qt_release=debug
	enable_qt=true
    fi
])
AC_SUBST(qt_release)
AC_SUBST(enable_qt)
AC_SUBST(qmake)

dnl check if user wants their own make program
dnl note: all makefiles in this package use the gmake syntax
if test -z "$MAKE"
then
    AC_PATH_PROG(MAKE, gmake)
    if test -z "$MAKE" 
    then
	# look elsewhere ...
	AC_MSG_CHECKING([for GNU make elsewhere])
	for f in /usr/local/bin/gmake /usr/freeware/bin/gmake /usr/local/bin/make /opt/sfw/bin/gmake nowhere
	do
	    if test -x $f
	    then
		MAKE=$f
		break
	    fi
	done
	if test $f = nowhere
	then
	    # Check if /usr/bin/make is any good
            mver=`/usr/bin/make --version 2>/dev/null | sed -n -e1p | cut -c1-8`
	    if test "$mver" != "GNU Make"
	    then
		echo
		echo "FATAL ERROR: could not find GNU make anywhere"
		echo "You need to set \$MAKE as the full path to GNU make "
		echo "in the environment."
		rm -rf conftest conftest.*
		exit 1
	    else
		MAKE=/usr/bin/make
	    fi
	fi
	AC_MSG_RESULT($MAKE)
    fi 
fi
make=$MAKE
AC_SUBST(make)

dnl check if users wants their own CPP
if test -z "$CPP"; then
    AC_PROG_CPP
fi
cpp=$CPP
AC_SUBST(cpp)

dnl check if users wants their own linker
if test -z "$LD"; then
    AC_PATH_PROG(LD, ld, /usr/bin/ld)
fi
ld=$LD
AC_SUBST(ld)

dnl Provide ways to override owner and group for installed files
if test -z "$PCP_USER_INSTALL" ; then
    pcp_user_install=root
else
    pcp_user_install="$PCP_USER_INSTALL"
fi
AC_SUBST(pcp_user_install)
if test -z "$PCP_GROUP_INSTALL" ; then
    case "$target_os"
    in
	darwin|freebsd|netbsd)
	    pcp_group_install=wheel
	    ;;
	*)
	    pcp_group_install=root
	    ;;
    esac
else
    pcp_group_install="$PCP_GROUP_INSTALL"
fi
AC_SUBST(pcp_group_install)

dnl check if the tar program is available
if test -z "$TAR"; then
    AC_PATH_PROGS(TAR, gtar tar, tar)
fi
if test $target_os = darwin -a -x /usr/bin/gnutar
then
    TAR=/usr/bin/gnutar
fi
tar=$TAR
AC_SUBST(tar)

dnl check if the gzip program is available
dnl (needed to gzip man pages on some platforms)
if test -z "$ZIP"; then
    AC_PATH_PROG(ZIP, gzip, /bin/gzip)
fi
test ! -x "$ZIP" && ZIP=/usr/local/bin/gzip
test ! -x "$ZIP" && ZIP=/usr/freeware/bin/gzip
test ! -x "$ZIP" && ZIP=/usr/bin/gzip
gzip=$ZIP
test -z "$gzip" && gzip=no-gzip
AC_SUBST(gzip)

dnl check if the bzip2 program is available
dnl (needed to bzip2 man pages on some platforms)
if test -z "$BZIP2"; then
    AC_PATH_PROG(BZIP2, bzip2, /bin/bzip2)
fi
test ! -x "$BZIP2" && BZIP2=/usr/bin/bzip2
test ! -x "$BZIP2" && BZIP2=/usr/local/bin/bzip2
test ! -x "$BZIP2" && BZIP2=/usr/freeware/bin/bzip2
bzip2=$BZIP2
test -z "$bzip2" && bzip2=no-bzip2
AC_SUBST(bzip2)

dnl check if the lzma program is available
dnl (needed to lzma man pages on some platforms)
if test -z "$LZMA"; then
    AC_PATH_PROG(LZMA, lzma, /bin/lzma)
fi
test ! -x "$LZMA" && LZMA=/usr/bin/lzma
test ! -x "$LZMA" && LZMA=/usr/local/bin/lzma
test ! -x "$LZMA" && LZMA=/usr/freeware/bin/lzma
lzma=$LZMA
test -z "$lzma" && lzma=no-lzma
AC_SUBST(lzma)

dnl check if the xz program is available
dnl (needed to xz man pages on some platforms)
if test -z "$XZ"; then
    AC_PATH_PROG(XZ, xz, /bin/xz)
fi
test ! -x "$XZ" && XZ=/usr/bin/xz
test ! -x "$XZ" && XZ=/usr/local/bin/xz
test ! -x "$XZ" && XZ=/usr/freeware/bin/xz
xz=$XZ
test -z "$xz" && xz=no-xz
AC_SUBST(xz)

dnl Check for mac PackageMaker
AC_MSG_CHECKING([for PackageMaker])
if test -z "$PACKAGE_MAKER"
then
    if test $target_os = darwin
    then
	if test -x /Applications/PackageMaker.app/Contents/MacOS/PackageMaker
	then # local XCode extras install
	    package_maker=/Applications/PackageMaker.app/Contents/MacOS/PackageMaker
	    AC_MSG_RESULT([ yes (local install)])
	elif test -x /Developer/Applications/PackageMaker.app/Contents/MacOS/PackageMaker
	then # Darwin 6.x
	    package_maker=/Developer/Applications/PackageMaker.app/Contents/MacOS/PackageMaker
	    AC_MSG_RESULT([ yes (darwin 6.x)])
	elif test -x /Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker
	then # Darwin 7.x
	    AC_MSG_RESULT([ yes (darwin 7.x)])
	    package_maker=/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker
	else
	    AC_MSG_RESULT([ not found!])
	    AC_MSG_WARN([PackageMaker not found, mac packages will not be made])
	fi
    else
	AC_MSG_RESULT([ no])	
    fi
else
    package_maker="$PACKAGE_MAKER"
fi
AC_SUBST(package_maker)

dnl check if the hdiutil program is available
if test -z "$HDIUTIL"; then
    AC_PATH_PROG(HDIUTIL, hdiutil)
fi
hdiutil=$HDIUTIL
AC_SUBST(hdiutil)

dnl check if the mkinstallp program is available (AIX)
if test -z "$MKINSTALLP"; then
    AC_PATH_PROG(MKINSTALLP, mkinstallp)
fi
mkinstallp=$MKINSTALLP
AC_SUBST(mkinstallp)

dnl check for the Solaris pmgmk package maker
if test -z "$PKGMK"; then
    AC_PATH_PROG(PKGMK, pkgmk)
fi
pkgmk=$PKGMK
AC_SUBST(pkgmk)

dnl check if the dlltool program is available
if test -z "$DLLTOOL"; then
    AC_PATH_PROG(DLLTOOL, dlltool)
fi
dlltool=$DLLTOOL
AC_SUBST(dlltool)

dnl check if the rpmbuild program is available
if test -z "$RPMBUILD"; then
    AC_PATH_PROG(RPMBUILD, rpmbuild)
fi
test $target_distro = slackware && RPMBUILD=''
rpmbuild=$RPMBUILD
AC_SUBST(rpmbuild)

dnl check if the rpm program is available
if test -z "$RPM"; then
    AC_PATH_PROG(RPM, rpm)
fi
test $target_distro = slackware && RPM=''
rpm=$RPM
AC_SUBST(rpm)

dnl if rpmbuild exists, use it, otherwise use rpm
if test -n "$RPMBUILD" -a -x "$RPMBUILD"
then
    rpmprog=$RPMBUILD
else
    rpmprog=$RPM
fi
AC_SUBST(rpmprog)

dnl check if the pod2man program is available (perl man page builder)
if test -z "$POD2MAN"; then
    AC_PATH_PROG(POD2MAN, pod2man)
fi
pod2man=$POD2MAN
AC_SUBST(pod2man)

dnl extra check for the Perl MakeMaker package
AC_MSG_CHECKING([if ExtUtils::MakeMaker is installed])
perl -e "use ExtUtils::MakeMaker" 2>/dev/null
if test $? -eq 0
then
    AC_MSG_RESULT([ yes])
else
    AC_MSG_RESULT([ no])
    echo
    echo "FATAL ERROR: Perl ExtUtils::MakeMaker module missing."
    echo "You can either install this from your distribution, or"
    echo "download from CPAN (Comprehensive Perl Archive Network)."
    rm -rf conftest conftest.*
    exit 1
fi

AC_PATH_PROG(TRUEPROG, true)

dnl check if the makedepend program is available
if test -z "$MAKEDEPEND"; then
    AC_PATH_PROG(MAKEDEPEND, makedepend, $TRUEPROG)
fi
makedepend=$MAKEDEPEND
AC_SUBST(makedepend)

dnl check if the md5sum program is available
if test -z "$MD5SUM"; then
   AC_PATH_PROG(MD5SUM, md5sum, $TRUEPROG)
fi
md5sum=$MD5SUM
AC_SUBST(md5sum)

dnl check if the Debian dpkg program is available
if test -z "$DPKG"; then
   AC_PATH_PROG(DPKG, dpkg)
fi
dpkg=$DKPG
AC_SUBST(dpkg)

dnl check for the Slackware makepkg packaging tool
if test -z "$MAKEPKG"; then
   AC_PATH_PROG(MAKEPKG, makepkg)
fi
makepkg=$MAKEPKG
AC_SUBST(makepkg)

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
	AC_MSG_ERROR([cannot enable books build - no toolchain found])
    fi
fi
AC_SUBST(book_toolchain)

book_brand="common"
if test "$do_books_brand" != ""
then
    book_brand=$do_books_brand
fi
AC_SUBST(book_brand)

dnl check if symbolic links are supported
AC_PROG_LN_S
if test $target_os = mingw; then
    as_ln_s=/bin/true
fi

dnl check if user wants their own lex, yacc
AC_PROG_LEX
lex=$LEX
AC_SUBST(lex)
AC_PROG_YACC
yacc=$YACC
AC_SUBST(yacc)

dnl extra check for lex and yacc as these are often not installed
AC_MSG_CHECKING([if yacc is executable])
binary=`echo $yacc | awk '{print $1}'`
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
    rm -rf conftest conftest.*
    exit 1
fi
AC_MSG_CHECKING([if lex is executable])
binary=`echo $lex | awk '{print $1}'`
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
    rm -rf conftest conftest.*
    exit 1
fi

dnl check if user wants their own awk, sed and echo
if test -z "$AWK"; then
    AC_PATH_PROGS(AWK, gawk awk, /usr/bin/awk)
fi
case "$AWK"
in
    gawk|*/gawk)
	awk="$AWK --posix"
	;;
    *)
	awk=$AWK
	;;
esac
AC_SUBST(awk)
if test -z "$SED"; then
    AC_PATH_PROG(SED, sed, /bin/sed)
fi
sed=$SED
AC_SUBST(sed)
if test -z "$ECHO"; then
    AC_PATH_PROG(ECHO, echo, /bin/echo)
fi
echo=$ECHO
AC_SUBST(echo)
if test -z "$WHICH"; then
    AC_PATH_PROG(WHICH, which, /usr/bin/which)
fi
which=$WHICH
AC_SUBST(which)

dnl check we don't get the Windows sort ...
AC_MSG_CHECKING([where unix-like sort(1) lives])
if test $target_os = mingw; then
    for d in /bin /usr/bin /mingw/bin /mingw/usr/bin
    do
	if test -x $d/sort; then
	    sort=$d/sort
	    break
	fi
    done
else
    sort=`which sort`
fi
AC_MSG_RESULT($sort)
$sort -n </dev/null
if test $? != 0
then
    echo
    echo "FATAL ERROR: $sort -n failed!"
    rm -rf conftest conftest.*
    exit 1
fi
AC_SUBST(sort)

dnl echo_n set to -n if echo understands -n to suppress newline
dnl echo_c set to \c if echo understands \c to suppress newline
AC_MSG_CHECKING([if echo uses -n or backslash-c to suppress newlines])
if ( $echo "testing\c"; $echo 1,2,3 ) | grep c >/dev/null
then
  if ( $echo -n testing; $echo 1,2,3 ) | sed s/-n/xn/ | grep xn >/dev/null
  then
    echo_n= echo_c=
    AC_MSG_RESULT([neither?])
  else
    echo_n=-n echo_c=
    AC_MSG_RESULT([ -n])
  fi
else
  echo_n= echo_c='\c'
  AC_MSG_RESULT([backslash-c])
fi
AC_SUBST(echo_n)
AC_SUBST(echo_c)

dnl if /proc is not mounted, try and mount it
dnl before trying to run the ps style test below
if test -d /proc
then
    test -f /proc/stat || mount /proc >/dev/null 2>&1
fi

dnl set platform specific ps
if test -n "$PROCPS"
then
    pcp_ps_prog="$PROCPS"
else
    pcp_ps_prog=ps
fi
AC_SUBST(pcp_ps_prog)

dnl set platform specific event logger
if test $target_os = mingw
then
    pcp_syslog_prog=pcp-eventlog
else
    pcp_syslog_prog=logger
fi
AC_SUBST(pcp_syslog_prog)

grep=grep
if test $target_os = solaris
then
    test -f /usr/xpg4/bin/grep && grep=/usr/xpg4/bin/grep
fi
AC_SUBST(grep)

dnl checks for /proc pseudo file system
AC_MSG_CHECKING([for /proc ])
if test "$cross_compiling" = "yes"; then
    ans=$have_procfs
    echo "cross-compile -> \"$ans\"" >&5
elif test -d /proc
then
    have_procfs=true
else
    have_procfs=false
fi
if $have_procfs
then
    AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_PROCFS, [1], [Support for /proc pseudo-filesystem])
else
    AC_MSG_RESULT(no)
fi

dnl Checks for C header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h limits.h malloc.h strings.h syslog.h)
AC_CHECK_HEADERS(unistd.h stddef.h sched.h dlfcn.h dl.h)
AC_CHECK_HEADERS(sys/time.h sys/times.h)
AC_CHECK_HEADERS(sys/resource.h sys/prctl.h)
AC_CHECK_HEADERS(sys/sysinfo.h sys/systeminfo.h)
AC_CHECK_HEADERS(endian.h standards.h sys/byteorder.h)
AC_CHECK_HEADERS(libgen.h sys/param.h sys/mman.h sys/un.h)
AC_CHECK_HEADERS(values.h stdint.h ieeefp.h math.h)
AC_CHECK_HEADERS(pwd.h grp.h regex.h sys/wait.h)
AC_CHECK_HEADERS(termio.h termios.h sys/termios.h sys/ioctl.h sys/socket.h)
AC_CHECK_HEADERS(netdb.h net/if.h netinet/in.h netinet/tcp.h arpa/inet.h)
AC_CHECK_HEADERS(windows.h winsock2.h ws2tcpip.h)
AC_CHECK_HEADERS(execinfo.h bits/wordsize.h)
AC_CHECK_HEADERS(iptypes.h, [], [], [#include <windows.h>])
AC_CHECK_HEADERS(iphlpapi.h, [], [], [#include <winsock2.h>])
AC_CHECK_HEADERS(fts.h libzfs.h)

dnl Check if we have <sys/endian.h> ... standard way
AC_MSG_CHECKING([for sys/endian.h ])
AC_TRY_COMPILE(
[  
    #include <sys/endian.h> 
],
[
], AC_DEFINE(HAVE_SYS_ENDIAN_H, [1], [sys/endian.h]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl Check if we have <machine/endian.h> ... MacOSX way
AC_MSG_CHECKING([for machine/endian.h ])
AC_TRY_COMPILE(
[  
    #include <machine/endian.h> 
],
[
], AC_DEFINE(HAVE_MACHINE_ENDIAN_H, [1], [machine/endian.h]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl Check if we have <sys/endian.h> ... IRIX strangeness
AC_MSG_CHECKING([for sys/endian.h (IRIX variant) ])
AC_TRY_COMPILE(
[  
    #include <standards.h>
    #include <sys/endian.h> 
],
[
], AC_DEFINE(HAVE_SYS_ENDIAN_H, [1], [IRIX sys/endian.h]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_INLINE

dnl check if regex functions come from libregex (mingw)
AC_CHECK_LIB(regex, regcomp)
lib_for_regex=""
if test $ac_cv_lib_regex_regcomp = yes
then
    lib_for_regex="-lregex"
fi
AC_SUBST(lib_for_regex)

PKG_CHECK_MODULES([ncurses], [ncurses], [have_ncurses=true], [have_ncurses=false])
AC_SUBST(HAVE_NCURSES, [$have_ncurses])

PKG_CHECK_MODULES([SYSTEMD], [libsystemd-journal],
                  [pmda_systemd=true], [pmda_systemd=false])
AC_SUBST(PMDA_SYSTEMD, $pmda_systemd)

pmda_infiniband=false
AS_IF([test "x$do_infiniband" != "xno"], [
    AC_CHECK_HEADERS([infiniband/umad.h], [
	AC_CHECK_HEADERS([infiniband/mad.h], [pmda_infiniband=true], [])
    ], [])
    savedLIBS=$LIBS
    IB_LIBS=""
    AC_CHECK_LIB(ibmad, madrpc_init)
    AC_CHECK_LIB(ibumad, umad_init)
    if test $ac_cv_lib_ibmad_madrpc_init = yes -a $ac_cv_lib_ibumad_umad_init = yes
    then
	IB_LIBS="-libmad -libumad"
	LIBS="$IB_LIBS"
	AC_MSG_CHECKING([for port_performance_query_via])
	AC_TRY_LINK_FUNC(port_performance_query_via,
                AC_DEFINE(HAVE_PORT_PERFORMANCE_QUERY_VIA, [1], [port_performance_query_via API])
		have_port_performance_query_via=true
		AC_MSG_RESULT(yes),
                have_port_performance_query_via=false
		AC_MSG_RESULT(no))
	AC_MSG_CHECKING([for pma_query_via])
	AC_TRY_LINK_FUNC(pma_query_via,
		AC_DEFINE(HAVE_PMA_QUERY_VIA, [1], [pma_query_via API])
                have_pma_query_via=true
		AC_MSG_RESULT(yes),
                have_pma_query_via=false
		AC_MSG_RESULT(no))
	LIBS=$savedLIBS
	if test $have_pma_query_via -o $have_port_performance_query_via
	then
	    :
	else
	    pmda_infiniband=false
	fi
    else
	pmda_infiniband=false
    fi
    LIBS=$savedLIBS
    AC_SUBST(IB_LIBS)
])
AC_SUBST(PMDA_INFINIBAND, $pmda_infiniband)

pmda_papi=false
AS_IF([test "x$do_papi" != "xno"], [
    enable_papi=true

    saved_LDFLAGS="$LDFLAGS"
    saved_CFLAGS="$CFLAGS"
    savedLIBS=$LIBS

    dnl potentially from the environment, else empty
    CFLAGS="$papi_CFLAGS"
    LDFLAGS="$papi_LIBS"

    case "$do_papi" in
	yes|no|check)
	    ;;
	*/*)
	    PAPI_ROOTDIR=$do_papi;
	    CFLAGS="-I$PAPI_ROOTDIR/include"
	    LDFLAGS="-L$PAPI_ROOTDIR/lib"
	    ;;
	 *)
	    PKG_CHECK_MODULES(papi,[$do_papi],[
		  CFLAGS="$papi_CFLAGS"
		  LDFLAGS="$papi_LIBS"])
	    ;;
    esac

    # $papi_LIBS may include -lpapi harmlessly redudantly with AC_CHECK_LIB
    AC_CHECK_LIB([papi], [PAPI_library_init],
		 [papi_libs="$LDFLAGS -lpapi"],
		 [enable_papi=false])
    AC_CHECK_HEADERS([papi.h], [papi_cflags="$CFLAGS"], [enable_papi=false])
    if test "$do_papi" != "check" -a "$enable_papi" != "true"
    then
	AC_MSG_ERROR([PAPI is not installed, cannot enable the PAPI PMDA])
    else
	pmda_papi=$enable_papi
    fi
    LIBS=$savedLIBS
    CFLAGS="$saved_CFLAGS"
    LDFLAGS="$saved_LDFLAGS"
    AC_SUBST(PAPI_CFLAGS, $papi_cflags)
    AC_SUBST(PAPI_LIBS, $papi_libs)
])
AC_SUBST(PMDA_PAPI, $pmda_papi)

AS_IF([test "x$pmda_papi" = "xtrue"], [
    	dnl check if struct PAPI_component_info_t has a disabled field
	AC_MSG_CHECKING([whether struct PAPI_component_info_t has a disabled field])
cat <<End-of-File >conftest.c
#include <papi.h>
int main() { PAPI_component_info_t *c; c->disabled = 0; }
End-of-File
	(eval $ac_compile) 2>conftest.out
	cat conftest.out >&5
    	if test -s conftest.out
    	then
	   AC_MSG_RESULT(no)
    	else
    	   AC_DEFINE(HAVE_PAPI_DISABLED_COMP, [1], [PAPI_component_info_t field])
    	   AC_MSG_RESULT(yes)

    	fi
    	rm -f conftest.*
])

pmda_perfevent=false
AS_IF([test "x$do_perfevent" != "xno" ], [
    enable_perfevent=true

    savedLIBS=$LIBS
    AC_CHECK_LIB([pfm], [pfm_get_os_event_encoding],
		 [pfm_libs="-lpfm"],
		 [enable_perfevent=false])
    AC_CHECK_HEADERS([perfmon/pfmlib_perf_event.h], [], [enable_perfevent=false])
    if test "$do_perfevent" != "check" -a "$enable_papi" != "true"
    then
	AC_MSG_ERROR([libpfm is not installed, cannot enable the perfevent PMDA])
    else
	pmda_perfevent=$enable_perfevent
    fi
    LIBS=$savedLIBS
    AC_SUBST(PFM_LIBS, $pfm_libs)
])
AC_SUBST(PMDA_PERFEVENT, $pmda_perfevent)

dnl Do you have RPM Package Manager libraries for pmdarpm?
pmda_rpm=false
savedLIBS=$LIBS
AC_MSG_CHECKING([for rpmlib > 4.4.2])
AC_COMPILE_IFELSE(
   [AC_LANG_PROGRAM([[#include <rpm/header.h>]],
                    [[(void)HEADERGET_EXT;]])],
   [AC_MSG_RESULT([yes])
    pmda_rpm=true],
   [AC_MSG_RESULT([no])])
AC_SUBST(PMDA_RPM,[$pmda_rpm])
LIBS=$savedLIBS

dnl Do you have system microhttpd libraries for pmwebapi?
PKG_CHECK_MODULES([libmicrohttpd], [libmicrohttpd > 0.9.9], [have_libmicrohttpd=true], [have_libmicrohttpd=false])
AC_SUBST(HAVE_LIBMICROHTTPD, [$have_libmicrohttpd])

dnl Do you have graphics libraries for pmwebapi?
PKG_CHECK_MODULES([cairo], [cairo >= 1.2, cairo-ft >= 1.2, cairo-png >= 1.2], [have_cairo=true], [have_cairo=false])
AC_SUBST(HAVE_CAIRO, [$have_cairo])

enable_webapi=false
AS_IF([test "x$do_webapi" != "xno"], [
    enable_webapi=true

    if test "$have_libmicrohttpd" != "true"
    then
	enable_webapi=false
	test "$do_webapi" != "check" && \
	AC_MSG_ERROR([libmicrohttpd is not installed, cannot enable the web API])
    elif test "$do_webapi" != "check" -a "x$cxx" = "x"
    then
	enable_webapi=false
	test "$do_webapi" != "check" && \
	AC_MSG_ERROR([C++ compiler unavailable, cannot enable the web API])
    fi
])
AC_SUBST(enable_webapi)

enable_manager=false
AS_IF([test "x$do_manager" != "xno"], [
    enable_manager=true

    if test "$target_os" = "mingw"
    then
	enable_manager=false
	test "$do_manager" != "check" && \
	AC_MSG_ERROR([MinGW build, cannot enable daemon manager])
    elif test "x$cxx" = "x"
    then
	enable_manager=false
	test "$do_manager" != "check" && \
	AC_MSG_ERROR([C++ compiler unavailable, cannot enable daemon manager])
    fi
    AC_MSG_CHECKING([compilation features for daemon manager])
    AC_TRY_COMPILE(
	[ #ifndef _XOPEN_SOURCE
	  #define _XOPEN_SOURCE 600
	  #endif
	  #include <stdio.h>
	], [],
	AC_MSG_RESULT(yes),
	enable_manager=false
	AC_MSG_RESULT(no))
])
AC_SUBST(enable_manager)

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_FUNC_WAIT3
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(mktime nanosleep usleep unsetenv)
AC_CHECK_FUNCS(select socket gethostname getpeerucred getpeereid)
AC_CHECK_FUNCS(uname syslog __clone pipe2 fcntl ioctl)
AC_CHECK_FUNCS(prctl setlinebuf waitpid atexit kill)
AC_CHECK_FUNCS(chown fchmod getcwd scandir mkstemp)
AC_CHECK_FUNCS(brk sbrk posix_memalign memalign valloc)
AC_CHECK_FUNCS(signal sighold sigrelse tcgetattr)
AC_CHECK_FUNCS(regex regcmp regexec regcomp)
AC_CHECK_FUNCS(strtod strtol strtoll strtoull strndup strchrnul)
AC_CHECK_FUNCS(getuid getgid)
AC_CHECK_FUNCS(getgrent getgrent_r getgrnam getgrnam_r getgrgid getgrgid_r)
AC_CHECK_FUNCS(getpwent getpwent_r getpwnam getpwnam_r getpwuid getpwuid_r)
AC_CHECK_FUNCS(sysinfo trace_back_stack backtrace)
AC_CHECK_FUNCS(sendmsg recvmsg setns)

dnl only define readdir64 on non-linux platforms that support it
if test $target_os != linux -a $target_os != freebsd -a $target_os != kfreebsd -a $target_os != netbsd; then
    AC_CHECK_FUNCS(readdir64)
fi

dnl typedefs missing from sys/types.h, stdlib.h or stddef.h
if test $target_os = solaris
then
   AC_CHECK_TYPE(__int32_t, int32_t)
   AC_CHECK_TYPE(__uint32_t, uint32_t)
   AC_CHECK_TYPE(__int64_t, int64_t)
   AC_CHECK_TYPE(__uint64_t, uint64_t)
   AC_CHECK_TYPE(uint_t, u_int32_t)
else
    AC_CHECK_TYPE(__int32_t, int)
    AC_CHECK_TYPE(__uint32_t, unsigned int)
    AC_CHECK_TYPE(__int64_t, long long)
    AC_CHECK_TYPE(__uint64_t, unsigned long long)
    AC_CHECK_TYPE(uint_t, unsigned int)
fi

dnl check if we have a type for the pointer's size integer (__psint_t)
AC_MSG_CHECKING([for __psint_t ])
AC_TRY_COMPILE(
[  
    #include <sys/types.h>
    #include <stdlib.h> 
    #include <stddef.h>
], [ __psint_t psint; ],
AC_DEFINE(HAVE___PSINT_T, [1], [__psint_t type]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl check if we have a type for pointer difference (ptrdiff_t)
AC_MSG_CHECKING([for ptrdiff_t ])
AC_TRY_COMPILE([
    #include <stddef.h>
    #ifdef HAVE_MALLOC_H
    #include <malloc.h>
    #endif
], [ ptrdiff_t ptrdiff; ],
AC_DEFINE(HAVE_PTRDIFF_T, [1], [ptrdiff_t type]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl check if we have types for uid_t, gid_t (POSIX) or SID (Win32)
AC_MSG_CHECKING([for uid_t ])
AC_TRY_COMPILE([
    #include <sys/types.h>
    #ifdef HAVE_PWD_H
    #include <pwd.h>
    #endif
], [ uid_t uid; ],
AC_DEFINE(HAVE_UID_T, [1], [uid_t type]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))
AC_MSG_CHECKING([for gid_t ])
AC_TRY_COMPILE([
    #include <sys/types.h>
    #ifdef HAVE_GRP_H
    #include <grp.h>
    #endif
], [ gid_t gid; ],
AC_DEFINE(HAVE_GID_T, [1], [gid_t type]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))
AC_MSG_CHECKING([for SID ])
AC_TRY_COMPILE([
    #ifdef HAVE_WINDOWS_H
    #include <windows.h>
    #endif
], [ SID sid; ],
AC_DEFINE(HAVE_SID, [1], [sid type]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl check if we have a type for socklen_t
AC_MSG_CHECKING([for socklen_t ])
AC_TRY_COMPILE([
    #include <sys/types.h>
    #ifdef HAVE_SYS_SOCKET_H
    #include <sys/socket.h>
    #endif
], [ socklen_t len; ],
AC_DEFINE(HAVE_SOCKLEN_T, [1], [socklen_t type]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl check if LL suffix on constants is supported
AC_TRY_COMPILE([
    #include <stdio.h>
], [ long long x = 0LL; ],
AC_DEFINE(HAVE_CONST_LONGLONG, [1], [LL suffix on constants]))

dnl check if _environ is declared globally
AC_TRY_LINK([
    #include <stdlib.h>
    #include <unistd.h>
], [ char **x = _environ; ],
AC_DEFINE(HAVE_UNDERBAR_ENVIRON, [1], [_environ declared globally]))

dnl check for PR_TERMCHILD and PR_SET_PDEATHSIG in <sys/prctl.h>
AC_MSG_CHECKING([for PR_TERMCHILD constant in sys/prctl.h])
AC_TRY_COMPILE([
    #ifdef HAVE_SYS_PRCTL_H
    #include <sys/prctl.h>
    #endif
], [ int i = PR_TERMCHILD; ],
AC_DEFINE(HAVE_PR_TERMCHILD, [1], [PR_TERMCHILD constant]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

AC_MSG_CHECKING([for PR_SET_PDEATHSIG constant in sys/prctl.h])
AC_TRY_COMPILE([
    #ifdef HAVE_SYS_PRCTL_H
    #include <sys/prctl.h>
    #endif
], [ int i = PR_SET_PDEATHSIG; ],
AC_DEFINE(HAVE_PR_SET_PDEATHSIG, [1], [PR_SET_PDEATHSIG constant]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

AC_HEADER_TIOCGWINSZ

dnl check if linker needs -rdynamic for dynamically loaded shared
dnl libraries to see the symbols in the process loading them.
dnl This is e.g., needed on linux for pmcd loading pmda_pmcd.so.
AC_MSG_CHECKING([if linker supports -rdynamic])
save_CFLAGS="$CFLAGS"
CFLAGS="-rdynamic"
AC_LINK_IFELSE([AC_LANG_PROGRAM([])],
    [AC_MSG_RESULT([yes])
     rdynamic_flag=-rdynamic],
    [AC_MSG_RESULT([no])
     rdynamic_flag=]
)
CFLAGS="$save_CFLAGS"
AC_SUBST([rdynamic_flag])


dnl check if argument to user's select() method in scandir call is const
AC_MSG_CHECKING([whether const arg for scandir() select method])
cat <<End-of-File >conftest.c
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
static int
my_select(const struct dirent *foo) { return 0; }
int main() { struct dirent **list; return scandir(".", &list, my_select, NULL); }
End-of-File
(eval $ac_compile) 2>conftest.out
cat conftest.out >&5
if test -s conftest.out
then
    AC_MSG_RESULT(no)
else
    AC_DEFINE(HAVE_CONST_DIRENT, [1], [const arg for scandir() select method])
    AC_MSG_RESULT(yes)
fi
rm -f conftest.*

dnl check if struct dirent has a d_off (directory offset) field
AC_MSG_CHECKING([whether struct dirent has a d_off field])
cat <<End-of-File >conftest.c
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
int main() { struct dirent d; d.d_off = 0; }
End-of-File
(eval $ac_compile) 2>conftest.out
cat conftest.out >&5
if test -s conftest.out
then
    AC_MSG_RESULT(no)
else
    AC_DEFINE(HAVE_DIRENT_D_OFF, [1], [struct dirent d_off field])
    AC_MSG_RESULT(yes)
fi
rm -f conftest.*

dnl check if printf %p has 0x prefix
AC_MSG_CHECKING([if printf %p produces 0x prefix])
if test "$cross_compiling" = "yes"; then
    ans=$printf_p_prefix
    echo "cross-compile -> \"$ans\"" >&5
else
    cat <<End-of-File >conftest.c
#include <stdio.h>
int main(int argc,  char **argv) { printf("%p", argv); return(0); }
End-of-File
    (eval $ac_compile) 2>&5
    (eval $ac_link) 2>&5
    ans=`./conftest`
    echo "./conftest -> \"$ans\"" >&5
fi
case "$ans"
in
    0x*)
	AC_DEFINE(HAVE_PRINTF_P_PFX, [1], [printf %p produces 0x])
	AC_MSG_RESULT(yes)
	;;
    *)
	AC_MSG_RESULT(no)
	;;
esac
rm -rf conftest conftest.*

_do_type()
{
    # need to deal with this sort of cpp output ...
    #   typedef long pid_t;
    #   typedef unsigned int pid_t;
    #   typedef unsigned int pid_t __attribute__ ((__mode__ (__SI__)));
    #   __extension__ typedef int pid_t;
    #   typedef foo_t
    #                  pid_t;
    #   typedef struct {
    #         ....
    #   } pid_t;
    #   typedef ... *pid;
    # and chaining of the form
    #   typedef long __pid_t;
    #   typedef __pid_t pid_t;
    #
    _raw=`$CPP conftest.c \
    | $SED \
	-e 's/[[ 	]]__attribute__ ((.*));/;/' \
	-e 's/__extension__[[ 	]][[ 	]]*//' \
    | $AWK '
/bozo/		{ print; next }
$1 == "typedef"	{ printf "%s",$0
		  if ($NF ~ /;$/) {
		      print ""
		      next
		  }
		  wantsemi = 1
		  if ($0 ~ /{/) depth = 1
		  next
		}
wantsemi == 1	{ printf " %s",$0
		  if ($0 ~ /{/) depth++
		  if (depth) {
		      if ($0 ~ /}/) depth--
		      if (depth > 0) next
		  }
		  if ($NF ~ /;$/) {
		      print ""
		      wantsemi = 0
		      next
		  }
		}' \
    | $SED \
	-e 's/\*/* /g' \
	-e 's/^[[ 	]]*//' \
        -e 's/;[[ 	]]*$//' \
    | $AWK '
$1 == "typedef"	{ map[[$NF]] = ""
		  for (i = 2; i < NF; i++) {
		    if (i == 2)
			map[[$NF]] = $i
		    else
			map[[$NF]] = map[[$NF]] " " $i
		  }
		  print $NF " -> " map[[$NF]] >"conftest.debug"
		  next
		}
$2 == "bozo"	{ t = $1
		  printf "best guess: %s",t >"conftest.debug"
		  while (map[[t]] != "") {
		    t = map[[t]]
		    printf " -> %s",t >"conftest.debug"
		  }
		  print "" >"conftest.debug"
		  print t
		  exit
		}'`
    case "$_raw"
    in
    int)
    	_fmt='"d"'
	;;
    unsigned|'unsigned int')
    	_fmt='"u"'
	;;
    long|'long int')
	_fmt='"ld"'
	;;
    'long long'|'long long int')
        if test $target_os = mingw; then
            _fmt='"I64d"'
        else
	    _fmt='"lld"'
        fi
	;;
    'unsigned long'|'unsigned long int'|'long unsigned int')
	_fmt='"lu"'
	;;
    'unsigned long long'|'unsigned long long int')
        if test $target_os = mingw; then
            _fmt='"I64u"'
        else
	    _fmt='"llu"'
        fi
	;;
    *\*)	# pointer to a something
	_fmt='"p"'
	;;
    struct\ *)	# not much can be done here ...
	_fmt='"p"'
	;;
    *)
	echo
	echo "FATAL ERROR: don't know what to do with type \"$_raw\""
	echo "... typedef mapping ..."
	cat conftest.debug
	rm -rf conftest conftest.*
	exit 1
	;;
    esac
}

dnl printf type for pid_t
AC_MSG_CHECKING([printf type for pid_t])
cat <<End-of-File >conftest.c
#include <sys/types.h>
#include <unistd.h>
pid_t bozo;
End-of-File
_do_type
fmt_pid="$_fmt"
AC_MSG_RESULT($fmt_pid)
AC_SUBST(fmt_pid)
rm -rf conftest.c conftest.debug

dnl printf type for int64_t
AC_MSG_CHECKING([printf type for int64_t ])
if test "$cross_compiling" = "yes"; then
    ans=$printf_fmt_int64
    echo "cross-compile -> \"$ans\"" >&5
else
    cat <<End-of-File >conftest.c
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
int main()
{
    printf("\"%s\"\n", __PRI64_PREFIX "d");
    return(0);
}
End-of-File
    ans=''
    (eval $ac_link) 2>&5
    if test -x ./conftest
    then
	ans=`./conftest`
	echo "./conftest -> \"$ans\"" >&5
    fi
    if test -z "$ans"
    then
	dnl have to try the hardway ...
	cat <<End-of-File >conftest.c
#include <stdio.h>
#include <stdint.h>
#include <string.h>
int main()
{
    char buf[[17]];
    int64_t bozo = 0x12345678abcdLL;
    sprintf(buf, "%lx", bozo);
    if (strcmp(buf, "12345678abcd") == 0)
	printf("\"ld\"\n");
    else {
	sprintf(buf, "%llx", bozo);
	if (strcmp(buf, "12345678abcd") == 0)
	    printf("\"lld\"\n");
    }
    return(0);
}
End-of-File
	(eval $ac_link) 2>&5
	if test -x ./conftest
	then
	    ans=`./conftest`
	    echo "./conftest -> \"$ans\"" >&5
	fi
    fi
fi
if test -z "$ans"
then
    echo 'FATAL ERROR: Cannot determine printf format for int64_t type'
    rm -rf conftest conftest.*
    exit 1
fi
fmt_int64="$ans"
AC_MSG_RESULT("$fmt_int64")
AC_SUBST(fmt_int64)
rm -rf conftest.* conftest

if test "$do_threads" = "check" -o "$do_threads" = "yes"
then
    AC_CHECK_HEADERS(
	pthread.h,
	[],
	[
	    if test "$do_threads" = "yes"
	    then
		AC_MSG_ERROR([cannot enable multi-threaded mode - no pthread.h])
	    fi
	])

    dnl Check if pthread_mutex_t is defined in pthread.h
    dnl Ignore the fact that pthread.h could be missing - we don't
    dnl really care if this test fails because of missing pthread_mutex_t
    dnl or because of missing headers.
    AC_MSG_CHECKING([for pthread_mutex_t in pthread.h])
    AC_TRY_COMPILE(
	[#include <pthread.h>], [pthread_mutex_t mymutex;],
	AC_DEFINE(HAVE_PTHREAD_MUTEX_T, [1], [pthread_mutex_t type]) AC_MSG_RESULT(yes),
	[
	    if test "$do_threads" = "yes"
	    then
		AC_MSG_ERROR([cannot enable multi-threaded mode - no mutexes])
	    else
		AC_MSG_RESULT(no)
	    fi
	])

    dnl Check which library provide pthread stuff
    AC_MSG_CHECKING([where pthread_create() is defined])
    for cand in "" pthreads pthread ; do
	savedLIBS=$LIBS
	if test -n "$cand" 
	then
	    LIBS=`echo $LIBS -l$cand`
	fi
	AC_TRY_LINK(
	[
	    #include <pthread.h>
	],
	[
	    pthread_create(NULL, NULL, NULL, NULL);
	],  AC_MSG_RESULT(lib${cand:-c})
	    if test -z "$cand"
	    then
		lib_for_pthreads="$cand"
	    else
		lib_for_pthreads="-l$cand"
	    fi
	    LIBS=$savedLIBS
	    break )
	LIBS=$savedLIBS
    done
    AC_SUBST(lib_for_pthreads)

    if test "$ac_cv_header_pthread_h" = "yes"
    then
	dnl printf type for pthread_t
	AC_MSG_CHECKING([printf type for pthread_t])
	cat <<End-of-File >conftest.c
#include <pthread.h>
pthread_t bozo;
End-of-File
	_do_type
	fmt_pthread="$_fmt"
	AC_MSG_RESULT($fmt_pthread)
	AC_SUBST(fmt_pthread)
	rm -rf conftest.c conftest.debug

	dnl check if gcc supports __thread for thread private data
	AC_MSG_CHECKING([if compiler supports __thread])
	dnl __thread support is broken in some places
	if test $target_os = netbsd
	then
	    AC_TRY_COMPILE([#include <pthread.h>
#if __GNUC__ < 4 || ( __GNUC__ == 4 && __GNUC_MINOR__ < 5 )
broken!
#else
__thread int x;
#endif], [],
		AC_DEFINE(HAVE___THREAD, [1], [__thread private data]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))
	else
	    AC_TRY_COMPILE([#include <pthread.h>
__thread int x;], [],
		AC_DEFINE(HAVE___THREAD, [1], [__thread private data]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))
	fi

	dnl Check if pthread_barrier_t is defined in pthread.h
	AC_MSG_CHECKING([for pthread_barrier_t in pthread.h])
	AC_TRY_COMPILE([#include <pthread.h>], [pthread_barrier_t mybarrier;],
	    AC_DEFINE(HAVE_PTHREAD_BARRIER_T, [1], [pthread_barrier_t type]) AC_MSG_RESULT(yes),
	    AC_MSG_RESULT(no))
    fi
fi

dnl check sizeof long
AC_MSG_CHECKING([sizeof long])
if test "$cross_compiling" = "yes"; then
    ans=$sizeof_long
    echo "cross-compile -> \"$ans\"" >&5
else
    cat <<End-of-File >conftest.c
#include <stdio.h>
main() { printf("%d", (int)sizeof(long)); }
End-of-File
    (eval $ac_compile) 2>&5
    (eval $ac_link) 2>&5
    ans=`./conftest`
    echo "./conftest -> \"$ans\"" >&5
fi
AC_MSG_RESULT($ans)
if test "$ans" = 4; then
    AC_DEFINE(HAVE_32BIT_LONG, [1], [sizeof long])
elif test "$ans" = 8; then
    AC_DEFINE(HAVE_64BIT_LONG, [1], [sizeof long])
else
    echo
    echo "FATAL ERROR: size of long is not 32 or 64 bits, cannot proceed."
    echo "sizeof(char *) -> $ans"
    rm -rf conftest conftest.*
    exit 1
fi
rm -rf conftest conftest.*

dnl check sizeof pointer
AC_MSG_CHECKING([sizeof pointer])
if test "$cross_compiling" = "yes"; then
    ans=$sizeof_pointer
    echo "cross-compile -> \"$ans\"" >&5
else
    cat <<End-of-File >conftest.c
#include <stdio.h>
main() { printf("%d", (int)sizeof(char *)); }
End-of-File
    (eval $ac_compile) 2>&5
    (eval $ac_link) 2>&5
    ans=`./conftest`
    echo "./conftest -> \"$ans\"" >&5
fi
AC_MSG_RESULT($ans)
if test "$ans" = 4; then
    AC_DEFINE(HAVE_32BIT_PTR, [1], [pointer size])
    PCP_PTR_SIZE=32
elif test "$ans" = 8; then
    AC_DEFINE(HAVE_64BIT_PTR, [1], [pointer size])
    PCP_PTR_SIZE=64
else
    echo
    echo "FATAL ERROR: size of pointer is not 32 or 64 bits, cannot proceed."
    echo "sizeof(char *) -> $ans"
    rm -rf conftest conftest.*
    exit 1
fi
rm -rf conftest conftest.*

dnl check sizeof int. If not 32, we die
AC_MSG_CHECKING([sizeof int])
if test "$cross_compiling" = "yes"; then
    ans=$sizeof_int
    echo "cross-compile -> \"$ans\"" >&5
else
    cat <<End-of-File >conftest.c
#include <stdio.h>
main() { printf("%d", (int)sizeof(int)); }
End-of-File
    (eval $ac_compile) 2>&5
    (eval $ac_link) 2>&5
    ans=`./conftest`
    echo "./conftest -> \"$ans\"" >&5
fi
AC_MSG_RESULT($ans)
if test "$ans" != 4
then
    echo
    echo "FATAL ERROR: sizeof(int) is not 32 bits, cannot proceed."
    echo "Note: 32-bit ints are assumed in the PCP external file formats and"
    echo "      the over-the-wire PDU formats"
    rm -rf conftest conftest.*
    exit 1
fi
rm -rf conftest conftest.*

dnl check bit field allocation order within a word
AC_MSG_CHECKING([if bit fields allocated left-to-right])
if test "$cross_compiling" = "yes"; then
    ans=$bit_field_scheme
    echo "cross-compile -> \"$ans\"" >&5
else
    cat <<End-of-File >conftest.c
union { struct { unsigned int b:4; unsigned int c:4; } a; int p; } u;
main() { u.a.b = 1; u.a.c = 2; printf("%0*x", 2*sizeof(int), u.p); }
End-of-File
    (eval $ac_compile) 2>&5
    (eval $ac_link) 2>&5
    ans=`./conftest`
    echo "./conftest -> \"$ans\"" >&5
fi
case "$ans"
in
    1200*|*0012)
	# left-to-right starting from MSB (SGI cc on MIPS), or
	# left-to-right ending at LSB
	AC_DEFINE(HAVE_BITFIELDS_LTOR, [1], [left-to-right bitfield ordering])
	AC_MSG_RESULT(yes)
	;;
    2100*|*0021)
	# right-to-left ending at MSB, or
	# right-to-left starting from LSB (gcc in Intel)
	AC_MSG_RESULT(no)
	;;
    *)
	AC_MSG_RESULT(unknown)
	echo "FATAL ERROR: could not fathom your compiler's bit field allocation scheme"
	rm -f conftest conftest.*
	exit 1
	;;
esac
rm -rf conftest conftest.*

dnl check if compiler can cast __uint64_t to double
AC_TRY_LINK(
[
    #include <stdlib.h>
    #include <unistd.h>
],
[
    __uint64_t x = 0;
    double y = (double)x;
], AC_DEFINE(HAVE_CAST_U64_DOUBLE, [1], [compiler casts u64 to double]))

dnl check if basename and dirname need -lgen, -lpcp or nothing to work
dnl (assume both go together)
AC_CHECK_FUNCS(basename)
if test $ac_cv_func_basename = yes
then
    AC_DEFINE(HAVE_BASENAME, [1], [basename API])
    AC_DEFINE(HAVE_DIRNAME, [1], [dirname API])
    lib_for_basename=""
else
    AC_CHECK_LIB(gen, basename)
    if test $ac_cv_lib_gen_basename = yes
    then
	AC_DEFINE(HAVE_BASENAME, [1], [basename API])
	AC_DEFINE(HAVE_DIRNAME, [1], [dirname API])
	lib_for_basename="-lgen"
    else
	lib_for_basename="-lpcp"
    fi
fi
AC_SUBST(lib_for_basename)

dnl check if clock_gettime needs -lrt to work
lib_for_clock_gettime=
AC_CHECK_FUNCS(clock_gettime)
if test $ac_cv_func_clock_gettime = no
then
    AC_CHECK_LIB(rt, clock_gettime)
    if test $ac_cv_lib_rt_clock_gettime = yes
    then
	AC_DEFINE(HAVE_CLOCK_GETTIME, [1], [clock_gettime API])
	lib_for_rt=-lrt
    fi
fi
AC_SUBST(lib_for_rt)

dnl check if dlopen et al need -ldl to work
lib_for_dlopen=
AC_CHECK_FUNCS(dlopen)
if test $ac_cv_func_dlopen = no
then
    AC_CHECK_LIB(dl, dlopen)
    if test $ac_cv_lib_dl_dlopen = yes
    then
	AC_DEFINE(HAVE_DLOPEN, [1], [dlopen API])
	lib_for_dlopen=-ldl
    fi
fi
AC_SUBST(lib_for_dlopen)

dnl check if flog10, pow, fpclassify and isnanf are available
dnl in the maths library
lib_for_math=
AC_CHECK_FUNCS(flog10)
if test $ac_cv_func_flog10 = no
then
    AC_CHECK_LIB(m, flog10)
    if test $ac_cv_lib_m_flog10 = yes
    then
	AC_DEFINE(HAVE_FLOG10, [1], [flog10 math API])
	lib_for_math=-lm
    fi
else
    AC_DEFINE(HAVE_FLOG10, [1], [flog10 math API])
fi
AC_CHECK_FUNCS(pow)
if test $ac_cv_func_pow = no
then
    AC_CHECK_LIB(m, pow)
    if test $ac_cv_lib_m_pow = yes
    then
	AC_DEFINE(HAVE_POW, [1], [pow math API])
	lib_for_math=-lm
    fi
else
    AC_DEFINE(HAVE_POW, [1], [pow math API])
fi
AC_MSG_CHECKING([for fpclassify()])
ac_cv_func_fpclassify=no
AC_TRY_LINK(
[
    #include <math.h>
],
[
    double x = 123.456;
    if (fpclassify(x) == FP_NAN) exit(1);
], ac_cv_func_fpclassify=yes)
AC_MSG_RESULT($ac_cv_func_fpclassify)
if test $ac_cv_func_fpclassify = no
then
    dnl try with -lm
    AC_MSG_CHECKING([for fpclassify() with -lm])
    savedLIBS=$LIBS
    LIBS=-lm
    AC_TRY_LINK(
[
    #include <math.h>
],
[
    double x = 123.456;
    if (fpclassify(x) == FP_NAN) exit(1);
], ac_cv_func_fpclassify=yes)
    AC_MSG_RESULT($ac_cv_func_fpclassify)
    if test $ac_cv_func_fpclassify = yes
    then
	lib_for_math=-lm
    fi
    LIBS=$savedLIBS
fi
if test $ac_cv_func_fpclassify = yes
then
    AC_DEFINE(HAVE_FPCLASSIFY, [1], [fpclassify math API])
else
    dnl prefer fpclassify() but will take isnan() and isnanf() as
    dnl possible alternates
    AC_CHECK_FUNCS(isnan)
    if test $ac_cv_func_isnan = no
    then
	AC_CHECK_LIB(m, isnan)
	if test $ac_cv_lib_m_isnan = yes
	then
	    AC_DEFINE(HAVE_ISNAN, [1], [isnan math API])
	    lib_for_math=-lm
	fi
    fi
    AC_CHECK_FUNCS(isnanf)
    if test $ac_cv_func_isnanf = no
    then
	AC_CHECK_LIB(m, isnanf)
	if test $ac_cv_lib_m_isnanf = yes
	then
	    AC_DEFINE(HAVE_ISNANF, [1], [isnanf math API])
	    lib_for_math=-lm
	fi
    fi
fi
AC_SUBST(lib_for_math)

dnl check if we have the SIG_PF typedef
AC_TRY_LINK([#include <signal.h>], [SIG_PF x;],
    AC_DEFINE(HAVE_SIGPF, [1], [SIG_PF typedef]))

dnl check if we have the SA_SIGINFO #define
AC_TRY_LINK([#include <signal.h>], [int x = SA_SIGINFO;],
    AC_DEFINE(HAVE_SA_SIGINFO, [1], [SA_SIGINFO macro]))

dnl check if we support the SIGPIPE signal
AC_TRY_LINK([#include <signal.h>], [int x = SIGPIPE;],
    AC_DEFINE(HAVE_SIGPIPE, [1], [SIGPIPE signal]))

dnl check if we support the SIGHUP signal
AC_TRY_LINK([#include <signal.h>], [int x = SIGHUP;],
    AC_DEFINE(HAVE_SIGHUP, [1], [SIGHUP signal]))

dnl check if we support the SIGBUS signal
AC_TRY_LINK([#include <signal.h>], [int x = SIGBUS;],
    AC_DEFINE(HAVE_SIGBUS, [1], [SIGBUS signal]))

dnl check if we need to explicitly include signal.h
AC_TRY_LINK([#include <sys/wait.h>],
[   typedef void (*SIGRET)(int);
    SIGRET x = SIG_IGN;
], AC_DEFINE(HAVE_WAIT_INCLUDES_SIGNAL, [1], [indirect signal.h]))

dnl check for name and type of time fields in struct stat
dnl IRIX example	timespec_t st_mtim;	
dnl Linux example 	struct timespec st_mtim;
dnl Darwin example 	struct timespec st_mtimespec;
dnl Solaris example	timestruc_t st_mtim;
dnl FreeBSD (6.1)	struct timespec st_mtimespec;
dnl			struct timespec {
dnl				time_t tv_sec;
dnl				long tv_nsec;
dnl			};
dnl
have_stat_type=false
have_stat_name=false
if test $have_stat_name = false
then
    AC_EGREP_HEADER(
    changequote(<<, >>)<<[ 	]st_mtimespec>>changequote([, ]),
    sys/stat.h, [ have_stat_name=true;
    AC_DEFINE(HAVE_ST_MTIME_WITH_SPEC, [1], [st_mtimespec stat field]) ])
fi
if test $have_stat_name = false -a $target_os != darwin -a $target_os != linux -a $target_os != kfreebsd -a $target_os != netbsd
then
    AC_EGREP_HEADER(
    changequote(<<, >>)<<[ 	]st_mtime>>changequote([, ]),
    sys/stat.h, [ have_stat_name=true;
    AC_DEFINE(HAVE_ST_MTIME_WITH_E, [1], [st_mtime stat field]) ])
fi
if test $have_stat_type = false
then
    AC_EGREP_HEADER(
    changequote(<<, >>)<<timestruc_t[ 	][ 	]*st_mtim>>changequote([, ]),
    sys/stat.h, [ have_stat_type=true;
    AC_DEFINE(HAVE_STAT_TIMESTRUC, [1], [timestruc_t type]) ])
fi
if test $have_stat_type = false
then
    AC_EGREP_HEADER(
    changequote(<<, >>)<<timespec_t[ 	][ 	]*st_mtim>>changequote([, ]),
    sys/stat.h, [ have_stat_type=true;
    AC_DEFINE(HAVE_STAT_TIMESPEC_T, [1], [timespec_t type]) ])
fi
if test $have_stat_type = false
then
    AC_EGREP_HEADER(
    changequote(<<, >>)<<timespec[ 	][ 	]*st_mtim>>changequote([, ]),
    sys/stat.h, [ have_stat_type=true;
    AC_DEFINE(HAVE_STAT_TIMESPEC, [1], [timespec type]) ])
fi
if test $have_stat_type = false
then
    AC_EGREP_HEADER(
    changequote(<<, >>)<<time_t[ 	][ 	]*st_mtim>>changequote([, ]),
    sys/stat.h, [ have_stat_type=true;
    AC_DEFINE(HAVE_STAT_TIME_T, [1], [time_t type]) ])
fi
if test $have_stat_type = false
then
    echo 'FATAL ERROR: Cannot determine struct stat time types.'
    rm -rf conftest conftest.*
    exit 1
fi

dnl
dnl Work out where to install stuff for this package
dnl (and where to find stuff at run-time for add-on packages).
dnl

dnl
dnl Predictable directory containing pcp.conf, or overridden
dnl by the $PCP_CONF environment variable. If this is not set
dnl (default /etc/pcp.conf), then $PCP_CONF must be set in
dnl the environment.
dnl
dnl The eval echo stuff is used to get around unfavourable
dnl elements of the GNU standards, which cause embedding of
dnl ${prefix} within many of these various $dirs.
dnl http://www.gnu.org/software/autoconf/manual/autoconf.html#Installation-Directory-Variables
dnl
pcp_etc_dir=`eval echo $sysconfdir`
pcp_etc_dir=`eval echo $pcp_etc_dir`
AC_SUBST(pcp_etc_dir)
pcp_saslconf_dir=`eval echo $sysconfdir/sasl2`
pcp_saslconf_dir=`eval echo $pcp_saslconf_dir`
AC_SUBST(pcp_saslconf_dir)
pcp_sysconf_dir=`eval echo $sysconfdir/pcp`
pcp_sysconf_dir=`eval echo $pcp_sysconf_dir`
AC_SUBST(pcp_sysconf_dir)
pcp_pmcdconf_path=$pcp_sysconf_dir/pmcd/pmcd.conf
pcp_pmcdrclocal_path=$pcp_sysconf_dir/pmcd/rc.local
pcp_pmcdoptions_path=$pcp_sysconf_dir/pmcd/pmcd.options
pcp_pmwebdoptions_path=$pcp_sysconf_dir/pmwebd/pmwebd.options
pcp_pmmgroptions_path=$pcp_sysconf_dir/pmmgr/pmmgr.options
pcp_pmproxyoptions_path=$pcp_sysconf_dir/pmproxy/pmproxy.options
pcp_pmiecontrol_path=$pcp_sysconf_dir/pmie/control
pcp_pmsnapcontrol_path=$pcp_sysconf_dir/pmsnap/control
pcp_pmloggercontrol_path=$pcp_sysconf_dir/pmlogger/control
AC_SUBST(pcp_pmcdconf_path)
AC_SUBST(pcp_pmcdoptions_path)
AC_SUBST(pcp_pmcdrclocal_path)
AC_SUBST(pcp_pmwebdoptions_path)
AC_SUBST(pcp_pmmgroptions_path)
AC_SUBST(pcp_pmproxyoptions_path)
AC_SUBST(pcp_pmiecontrol_path)
AC_SUBST(pcp_pmsnapcontrol_path)
AC_SUBST(pcp_pmloggercontrol_path)

dnl shared PCP files (shareable for diskless)
pcp_share_dir=`eval echo $datarootdir/pcp`
pcp_share_dir=`eval echo $pcp_share_dir`
AC_SUBST(pcp_share_dir)

dnl private PCP executables
pcp_binadm_dir=`eval echo $libexecdir/pcp/bin`
pcp_binadm_dir=`eval echo $pcp_binadm_dir`
AC_SUBST(pcp_binadm_dir)

dnl non-shared (i.e. system local) PCP files
pcp_var_dir=`eval echo $localstatedir/lib/pcp`
pcp_var_dir=`eval echo $pcp_var_dir`
AC_SUBST(pcp_var_dir)

dnl pmcd control and options files
AC_ARG_WITH(configdir,[AC_HELP_STRING([--with-configdir],[configuration directory [LOCALSTATEDIR/pcp/config]])],
                      [pcp_config_dir=$withval],
                      [pcp_config_dir=$pcp_var_dir/config])

pcp_pmdas_dir=$pcp_var_dir/pmdas
AC_SUBST(pcp_pmdas_dir)

dnl runtime shared libraries
pcp_lib_dir=`eval echo $libdir`
pcp_lib_dir=`eval echo $pcp_lib_dir`
pcp_lib32_dir=`echo $pcp_lib_dir | sed -e s,64,, -e s,//,/,`
AC_SUBST(pcp_lib_dir)
AC_SUBST(pcp_lib32_dir)

dnl perl modules
AC_ARG_WITH(perl_installdirs,[AC_HELP_STRING([--with-perl_installdirs],[perl installdirs [vendor]])],
                      [perl_installdirs=$withval],
                      [perl_installdirs=vendor])
perl_installdirs=`eval echo $perl_installdirs`
perl_installdirs=`eval echo $perl_installdirs`
AC_SUBST(perl_installdirs)
AC_ARG_WITH(perl_install_base,[AC_HELP_STRING([--with-perl_install_base],[perl install_base [PREFIX]])],
                      [perl_install_base=$withval],
                      [perl_install_base=$prefix])
perl_install_base=`eval echo $perl_install_base`
perl_install_base=`eval echo $perl_install_base`
AC_SUBST(perl_install_base)

AC_ARG_WITH(python_prefix,[AC_HELP_STRING([--with-python_prefix],[python setup.py prefix [PREFIX]])],
                      [python_prefix=$withval],
                      [python_prefix=$prefix])
python_prefix=`eval echo $python_prefix`
python_prefix=`eval echo $python_prefix`
AC_SUBST(python_prefix)

AC_PATH_XTRA
pcp_x11_incflags=$X_CFLAGS
AC_SUBST(pcp_x11_incflags)
pcp_x11_libflags=$X_LIBS
AC_SUBST(pcp_x11_libflags)
pcp_x11_extra=$X_EXTRA_LIBS
AC_SUBST(pcp_x11_extra)
pcp_x11_pre=$X_PRE_LIBS
AC_SUBST(pcp_x11_pre)

dnl man pages (source)
have_gzipped_manpages=false
have_bzip2ed_manpages=false
have_lzmaed_manpages=false
have_xzed_manpages=false
need_old_tbl_header=false
man_header=
pcp_man_dir=`eval echo $mandir`
pcp_man_dir=`eval echo $pcp_man_dir`

dnl guess compression in use
for d in /usr/man /usr/share/man $pcp_man_dir
do
    for sd in man1 sman1
    do
	if test -f $d/$sd/man.1.gz
	then
	    have_gzipped_manpages=true
	    man_header=`$ZIP -d < $d/$sd/man.1.gz | head -1`
	    break
	elif test -f $d/$sd/man.1.bz2
	then
	    have_bzip2ed_manpages=true
	    man_header=`$BZIP2 -d < $d/$sd/man.1.bz2 | head -1`
	    break
	elif test -f $d/$sd/man.1.lzma
	then
	    have_lzmaed_manpages=true
	    man_header=`$LZMA -d < $d/$sd/man.1.lzma | head -1`
	    break
	elif test -f $d/$sd/man.1.xz
	then
	    have_xzed_manpages=true
	    man_header=`$XZ -d < $d/$sd/man.1.xz | head -1`
	    break
	elif test -f $d/$sd/man.1
	then
	    man_header=`head -1 $d/$sd/man.1`
	    break
	fi
    done
done
if test x"$man_header" = "x'\\\" t" -o x"$man_header" = "x'\\\" te" ; then
    need_old_tbl_header=true
fi
AC_SUBST(pcp_man_dir)
AC_SUBST(have_gzipped_manpages)
AC_SUBST(have_bzip2ed_manpages)
AC_SUBST(have_lzmaed_manpages)
AC_SUBST(have_xzed_manpages)
AC_SUBST(need_old_tbl_header)

dnl public binaries
pcp_bin_dir=`eval echo $bindir`
pcp_bin_dir=`eval echo $pcp_bin_dir`
AC_SUBST(pcp_bin_dir)

pcp_sbin_dir=`eval echo $sbindir`
pcp_sbin_dir=`eval echo $pcp_sbin_dir`
AC_SUBST(pcp_sbin_dir)

dnl include files
pcp_inc_dir=`eval echo $includedir/pcp`
pcp_inc_dir=`eval echo $pcp_inc_dir`
AC_SUBST(pcp_inc_dir)

dnl books - pdfs, html
if test $target_os = linux; then
    pcp_books_dir=`eval echo $datarootdir/doc/pcp-doc`
else
    pcp_books_dir=`eval echo $datarootdir/doc/pcp`
fi
pcp_books_dir=`eval echo $pcp_books_dir`
pcp_html_dir=`eval echo $pcp_books_dir/html`
AC_SUBST(pcp_books_dir)
AC_SUBST(pcp_html_dir)

dnl icon pixmap files
if test $target_os = linux; then
    pcp_icons_dir=`eval echo $datarootdir/pcp-gui/pixmaps`
else
    pcp_icons_dir=`eval echo $datarootdir/pcp/pixmaps`
fi
pcp_icons_dir=`eval echo $pcp_icons_dir`
AC_SUBST(pcp_icons_dir)

dnl desktop application entries
pcp_desktop_dir=`eval echo $datarootdir/applications`
pcp_desktop_dir=`eval echo $pcp_desktop_dir`
AC_SUBST(pcp_desktop_dir)

dnl rc/startup files
AC_ARG_WITH(rcdir,[AC_HELP_STRING([--with-rcdir],[rc directory [SYSCONFDIR/rc.d]])],
                  [pcp_rc_dir=$withval], [
if $enable_systemd
then
    pcp_rc_dir="$pcp_share_dir/lib"
else
    pcp_rc_dir="$pcp_etc_dir/init.d"
fi])
AC_SUBST(pcp_rc_dir)

dnl rc sysconfig dir
AC_ARG_WITH(sysconfigdir,[AC_HELP_STRING([--with-sysconfigdir],[sysconfig directory [SYSCONFDIR/sysconfig]])],
                  [pcp_sysconfig_dir=$withval],
                  [pcp_sysconfig_dir=$pcp_etc_dir/sysconfig])
AC_SUBST(pcp_sysconfig_dir)

dnl logs
AC_ARG_WITH(logdir,[AC_HELP_STRING([--with-logdir],[log directory [LOCALSTATEDIR/log/pcp]])],
                   [pcp_log_dir=$withval],
                   [pcp_log_dir=$localstatedir/log/pcp])
pcp_log_dir=`eval echo $pcp_log_dir`
pcp_log_dir=`eval echo $pcp_log_dir`
AC_SUBST(pcp_log_dir)

AC_ARG_WITH(rundir,[AC_HELP_STRING([--with-rundir],[run directory [LOCALSTATEDIR/run/pcp]])],
                   [pcp_run_dir=$withval],
                   [pcp_run_dir=$localstatedir/run/pcp])
pcp_run_dir=`eval echo $pcp_run_dir`
pcp_run_dir=`eval echo $pcp_run_dir`
AC_SUBST(pcp_run_dir)

dnl world-writeable temporary files directory
AC_ARG_WITH(tmpdir,[AC_HELP_STRING([--with-tmpdir],[tmp directory [LOCALSTATEDIR/tmp]])],
                   [pcp_tmpfile_dir=$withval],
                   [pcp_tmpfile_dir=$localstatedir/tmp])
pcp_tmpfile_dir=`eval echo $pcp_tmpfile_dir`
pcp_tmpfile_dir=`eval echo $pcp_tmpfile_dir`
AC_SUBST(pcp_tmpfile_dir)

dnl non-world-writeable status files directory
pcp_tmp_dir=`eval echo $pcp_var_dir/tmp`
AC_SUBST(pcp_tmp_dir)

dnl doc directory
AC_ARG_WITH(docdir,[AC_HELP_STRING([--with-docdir],[docs directory [DOCDIR/pcp-VERSION]])],
                   [pcp_doc_dir=$withval],
                   [PACKAGE=pcp-${PACKAGE_VERSION}; pcp_doc_dir=$docdir])
pcp_doc_dir=`eval echo $pcp_doc_dir`
pcp_doc_dir=`eval echo $pcp_doc_dir`
AC_SUBST(pcp_doc_dir)

dnl demos directory
AC_ARG_WITH(demosdir,[AC_HELP_STRING([--with-demosdir],[run directory [DATADIR/pcp/demos]])],
                   [pcp_demos_dir=$withval],
                   [pcp_demos_dir=$pcp_share_dir/demos])
AC_SUBST(pcp_demos_dir)

if test -z "$XCONFIRM" 
then
    AC_PATH_PROG(ac_xconfirm_prog, xconfirm, $pcp_bin_dir/pmconfirm)
else
    ac_xconfirm_prog=$XCONFIRM
fi
AC_SUBST(ac_xconfirm_prog)

dnl Check for FNDELAY defined in <fcntl.h>
if test "$ac_cv_header_fcntl_h" = "yes"
then
    AC_MSG_CHECKING([for FNDELAY in fcntl.h])
    AC_TRY_COMPILE([#include <fcntl.h>],
	[ int i = FNDELAY; ],
	AC_DEFINE(HAVE_FNDELAY, [1], [FNDELAY macro])
	AC_MSG_RESULT(yes), AC_MSG_RESULT(no))
fi

dnl check for zpool_vdev_name API style from <libzfs.h>
if test "$ac_cv_header_libzfs_h" = "yes"
then
    AC_MSG_CHECKING([for 5-argument style zpool_vdev_name])
    AC_TRY_COMPILE([#include <libzfs.h>],
	[ libzfs_handle_t *zh;
	  zpool_handle_t *zp;
	  nvlist_t *np;
	  (void) zpool_vdev_name(zh, zp, np, B_FALSE, B_FALSE);
	],
	AC_DEFINE(HAVE_ZPOOL_VDEV_NAME_5ARG, [1], [5-arg zpool_vdev_name])
	AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

    AC_MSG_CHECKING([for 4-argument style zpool_vdev_name])
    AC_TRY_COMPILE([#include <libzfs.h>],
	[ libzfs_handle_t *zh;
	  zpool_handle_t *zp;
	  nvlist_t *np;
	  (void) zpool_vdev_name(zh, zp, np, B_FALSE);
	],
	AC_DEFINE(HAVE_ZPOOL_VDEV_NAME_4ARG, [1], [4-arg zpool_vdev_name])
	AC_MSG_RESULT(yes), AC_MSG_RESULT(no))
fi

dnl Check for Network Security Services (NSS) and
dnl Simple Authentication and Security Layer (SASL)
if test "$do_secure" = "check" -o "$do_secure" = "yes"
then
    enable_secure=true

    saved_CFLAGS="$CFLAGS"
    saved_CPPFLAGS="$CPPFLAGS"
    NSS_INC_DIRS="-I/usr/include/nss -I/usr/include/nss3"
    NSPR_INC_DIRS="-I/usr/include/nspr -I/usr/include/nspr4"
    CFLAGS="$CFLAGS $NSS_INC_DIRS $NSPR_INC_DIRS"
    CPPFLAGS="$CPPFLAGS $NSS_INC_DIRS $NSPR_INC_DIRS"

    AC_CHECK_HEADERS([nss/nss.h], [NSSCFLAGS=-I/usr/include/nss], [
	AC_CHECK_HEADERS([nss3/nss.h], [NSSCFLAGS=-I/usr/include/nss3], [
	    enable_secure=false
	    if test "$do_secure" = "yes"
	    then
		AC_MSG_ERROR([cannot enable secure sockets mode - no NSS header])
	    fi
	])
    ])
    AC_SUBST(NSSCFLAGS)

    AC_CHECK_HEADERS([nspr/nspr.h], [NSPRCFLAGS=-I/usr/include/nspr], [
	AC_CHECK_HEADERS([nspr4/nspr.h], [NSPRCFLAGS=-I/usr/include/nspr4], [
	    enable_secure=false
	    if test "$do_secure" = "yes"
	    then
		AC_MSG_ERROR([cannot enable secure sockets mode - no NSPR header])
	    fi
	])
    ])
    AC_SUBST(NSPRCFLAGS)

    AC_CHECK_HEADERS([sasl/sasl.h], [SASLCFLAGS=-I/usr/include/sasl], [
	enable_secure=false
	if test "$do_secure" = "yes"
	then
	    AC_MSG_ERROR([cannot enable secure sockets mode - no SASL header])
	fi
    ])
    AC_SUBST(SASLCFLAGS)

    AC_CHECK_LIB(sasl2, sasl_server_init, [lib_for_sasl="-lsasl2"], [
	enable_secure=false
	if test "$do_secure" = "yes"
	then
	    AC_MSG_ERROR([cannot enable secure sockets mode - no SASL library])
	fi
    ])
    AC_SUBST(lib_for_sasl)

    AC_CHECK_LIB(ssl, SSL_ImportFD, [lib_for_ssl="-lssl"], [
	AC_CHECK_LIB(ssl3, SSL_ImportFD, [lib_for_ssl="-lssl3"], [
	    enable_secure=false
	    if test "$do_secure" = "yes"
	    then
		AC_MSG_ERROR([cannot enable secure sockets mode - no SSL library])
	    fi
	])
    ])
    AC_SUBST(lib_for_ssl)

    AC_CHECK_LIB(nss, NSS_Init, [lib_for_nss="-lnss"], [
	AC_CHECK_LIB(nss3, NSS_Init, [lib_for_nss="-lnss3"], [
	    enable_secure=false
	    if test "$do_secure" = "yes"
	    then
		AC_MSG_ERROR([cannot enable secure sockets mode - no NSS library])
	    fi
	])
    ])
    AC_SUBST(lib_for_nss)

    AC_CHECK_LIB(nspr, PR_Init, [lib_for_nspr="-lnspr"], [
	AC_CHECK_LIB(nspr4, PR_Init, [lib_for_nspr="-lnspr4"], [
	    enable_secure=false
	    if test "$do_secure" = "yes"
	    then
		AC_MSG_ERROR([cannot enable secure sockets mode - no NSPR library])
	    fi
	])
    ])
    AC_SUBST(lib_for_nspr)

    if test "$enable_secure" = "true"
    then
	AC_DEFINE(HAVE_SECURE_SOCKETS, [1], [Encrypted sockets])
    fi
    AC_SUBST(enable_secure)

    CPPFLAGS="$saved_CPPFLAGS"
    CFLAGS="$saved_CFLAGS"
fi

dnl check for -latomic, needed for some -march=i386 builds with i686 c++ distro headers
AC_CHECK_LIB(atomic, __atomic_fetch_add_4, [lib_for_atomic="-latomic"])
AC_SUBST(lib_for_atomic)

dnl check for array sessions
if test -f /usr/include/sn/arsess.h
then
    pcp_mpi_dirs=libpcp_mpi\ libpcp_mpiread
else
    pcp_mpi_dirs=
fi
AC_SUBST(pcp_mpi_dirs)

dnl check for Unix Domain socket family structure
AC_MSG_CHECKING([for struct sockaddr_un in sys/un.h])
AC_TRY_COMPILE([
    #include <sys/types.h>
    #ifdef HAVE_SYS_SOCKET_H
    #include <sys/socket.h>
    #endif
    #ifdef HAVE_SYS_UN_H
    #include <sys/un.h>
    #endif
], [ struct sockaddr_un sa; sa.sun_family = AF_UNIX; ],
AC_DEFINE(HAVE_STRUCT_SOCKADDR_UN, [1], [sockaddr_un type]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl check for Linux Unix Domain socket credential structure
AC_MSG_CHECKING([for struct ucred in sys/socket.h])
AC_TRY_COMPILE([
    #include <sys/types.h>
    #ifdef HAVE_SYS_SOCKET_H
    #include <sys/socket.h>
    #endif
    #ifdef HAVE_SYS_UN_H
    #include <sys/un.h>
    #endif
], [ struct ucred ucred; ucred.uid = 0; ],
AC_DEFINE(HAVE_STRUCT_UCRED, [1], [ucred type]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl check for struct timespec in <time.h>
AC_MSG_CHECKING([for struct timespec in time.h])
AC_TRY_COMPILE([
    #include <time.h>
], [ struct timespec foo; ],
AC_DEFINE(HAVE_STRUCT_TIMESPEC, [1], [timespec type]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl check if we have IRIX style altzone 
AC_MSG_CHECKING([for altzone in time.h])
AC_TRY_COMPILE([
    #include <time.h>
], [ time_t az = altzone; ],
AC_DEFINE(HAVE_ALTZONE, [1], [altzone global]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl If there is no altzone, check if strftime can handle %z
AC_MSG_CHECKING([if strftime knows about %z])
AC_TRY_RUN(
[
#include <time.h>
int main () {
    char b[32]="";
    time_t t = time(NULL);
    struct tm * t1 = localtime (&t);
    if (strftime (b, 32, "%z", t1) < 3)
       return (1);
    if (strcmp(b, "%z") == 0)
       return(1);
    return (0);
}
], AC_DEFINE(HAVE_STRFTIME_z, [1], [strftime with %z]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no), AC_MSG_RESULT(no))

dnl does strerror_r return char * a la GNU
dnl (as opposed to the int return for XSI-compliant variants)
AC_MSG_CHECKING([if strerror_r returns char *])
AC_TRY_COMPILE(
    [#include <string.h>], [strerror_r(0, NULL, 0)[0];],
    AC_DEFINE(HAVE_STRERROR_R_PTR, [1], [strerror_r return]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl first check for readline on its own then with curses
savedLIBS=$LIBS
LIBS=
lib_for_curses=
lib_for_readline=
AC_CHECK_FUNC(readline,,
    [
	AC_CHECK_LIB(readline, readline,,
	    [
		dnl AC seems to cache lib/func results
		dnl so use another readline func here
		AC_CHECK_LIB(readline, add_history,,,[-lcurses])
	    ])
    ])
if test $ac_cv_func_readline = yes
then
    AC_DEFINE(HAVE_READLINE, [1], [readline API])
elif test $ac_cv_lib_readline_readline = yes
then
    AC_DEFINE(HAVE_READLINE, [1], [readline API])
    lib_for_readline=-lreadline
elif test $ac_cv_lib_readline_add_history = yes
then
    AC_DEFINE(HAVE_READLINE, [1], [readline API])
    lib_for_curses=-lcurses
    lib_for_readline=-lreadline
fi
AC_SUBST(lib_for_readline)
AC_SUBST(lib_for_curses)
LIBS=$savedLIBS

dnl Check if we have AI_ADDRCONFIG
AC_MSG_CHECKING([for AI_ADDRCONFIG])
AC_TRY_COMPILE(
[  
    #include <netdb.h>
    int test = AI_ADDRCONFIG;
],
[
], AC_DEFINE(HAVE_AI_ADDRCONFIG, [1], [AI_ADDRCONFIG macro]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl Find external source tarballs for packaging

have_vector=false
if test -f ./build/tar/vector.tar.gz
then
    AC_MSG_NOTICE([adding Vector webapp to the build])
    have_vector=true
fi
AC_SUBST(have_vector)

have_webjs=false
if test -f ./build/tar/webjs.tar.gz
then
    AC_MSG_NOTICE([adding webjs webapps to the build])
    have_webjs=true
fi
AC_SUBST(have_webjs)

dnl Capture special options passed to configure
AC_SUBST(PACKAGE_CONFIGURE)

dnl
dnl output files
dnl

AC_OUTPUT(
dnl   Build definitions for use in Makefiles
    src/include/builddefs
dnl   PCP paths and other defs
    src/include/pcp.conf
dnl   Linux Software Map entry
    pcp.lsm
dnl   Preamble for deb install scripts
    debian/pcp.preinst.head
    debian/pcp.postinst.head
dnl   Preamble for tar install scripts
    build/tar/preinstall.head
    build/tar/postinstall.head
dnl   Build definitions for use in packaging
    build/GNUlocaldefs
    build/mac/uninstall-pcp
)

dnl verbose diagnostics
dnl
dnl echo config.status: settings dump begin
dnl grep '^S.".*=' config.status
dnl echo config.status: settings dump end
