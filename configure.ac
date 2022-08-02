dnl
dnl Copyright (c) 2012-2022 Red Hat.
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

dnl need 2.63 for AS_VAR_IF and AS_VAR_COPY
AC_PREREQ([2.63])dnl

AC_ARG_WITH([64bit],
    [AS_HELP_STRING([--with-64bit],
		    [turn on 64 bit compilation mode (default is platform dependent)])],
    [use_64bit=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-64bit=$withval"])

AC_ARG_WITH([optimization],
    [AS_HELP_STRING([--with-optimization],
		    [enable optimization for C/C++ code (default is yes)])],
    [use_optimization=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-optimization=$withval"],
    [use_optimizaton=yes])

AC_ARG_WITH([gperftools],
    [AS_HELP_STRING([--with-gperftools],
		    [enable gperftools for CPU and heap profiling (default is no)])],
    [use_gperftools=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-gperftools=$withval"],
    [use_gperftools=no])

AC_ARG_WITH([sanitizer],
    [AS_HELP_STRING([--with-sanitizer],
		    [enable sanitizers (default is no, common values are 'address', 'thread' or 'leak')])],
    [use_sanitizer=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-sanitizer=$withval"],
    [use_sanitizer=no])

AC_ARG_WITH([threads],
    [AC_HELP_STRING([--with-threads],
		    [enable support for multiple threads (default is on)])],
    [do_threads=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-threads=$withval"],
    [do_threads=check])

AC_ARG_WITH([secure-sockets],
    [AC_HELP_STRING([--with-secure-sockets],
		    [enable support for secure sockets (default is on)])],
    [do_secure=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-secure-sockets=$withval"],
    [do_secure=check])

AC_ARG_WITH([static-probes],
    [AC_HELP_STRING([--with-static-probes],
		    [enable support for static probes (default is on)])],
    [do_probes=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-static-probes=$withval"],
    [do_probes=check])

AC_ARG_WITH([infiniband],
    [AC_HELP_STRING([--with-infiniband],
		    [enable support for Infiniband metrics (default is on)])],
    [do_infiniband=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-infiniband=$withval"],
    [do_infiniband=check])

AC_ARG_WITH([user],
    [AS_HELP_STRING([--with-user],
		    [user account under which daemons run (default is pcp)])],
    [pcp_user=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-user-account=$withval"],
    [pcp_user=pcp])
AC_SUBST(pcp_user)

AC_ARG_WITH([group],
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

AC_ARG_WITH([qt3d],
    [AC_HELP_STRING([--with-qt3d],
                    [enable support for 3D scene graph API (default is on)])],
    [do_qt3d=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-qt3d=$withval"],
    [do_qt3d=check])

AC_ARG_WITH([perl],
    [AC_HELP_STRING([--with-perl],
                    [enable support for tools requiring Perl (default is on)])],
    [do_perl=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-perl=$withval"],
    [do_perl=check])

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

AC_ARG_WITH([dstat-symlink],
    [AC_HELP_STRING([--with-dstat-symlink],
		    [enable support for /usr/bin/dstat symlink (default is on)])],
    [do_dstat_symlink=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-dstat-symlink=$withval"],
    [do_dstat_symlink=check])

AC_ARG_WITH([perfevent],
    [AC_HELP_STRING([--with-perfevent],
		   [enable perfevent pmda (default is on)])],
    [do_perfevent=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-perfevent=$withval"],
    [do_perfevent=check])

AC_ARG_WITH([pmdastatsd],
    [AC_HELP_STRING([--with-pmdastatsd],
		   [enable statds pmda (default is on)])],
    [do_pmdastatsd=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdastatsd=$withval"],
    [do_pmdastatsd=check])

AC_ARG_WITH([pmdapodman],
    [AC_HELP_STRING([--with-pmdapodman],
		   [enable podman pmda (default is on)])],
    [do_pmdapodman=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdapodman=$withval"],
    [do_pmdapodman=check])

AC_ARG_WITH([pmdabcc],
    [AC_HELP_STRING([--with-pmdabcc],
		   [enable BCC pmda (default is on)])],
    [do_pmdabcc=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdabcc=$withval"],
    [do_pmdabcc=check])

AC_ARG_WITH([pmdabpf],
    [AC_HELP_STRING([--with-pmdabpf],
		   [enable BPF pmda (default is on)])],
    [do_pmdabpf=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdabpf=$withval"],
    [do_pmdabpf=check])

AC_ARG_WITH([pmdabpf-clang],
    [AC_HELP_STRING([--with-pmdabpf-clang],
                    [path to clang 10+ (default is empty for auto discovery)])],
    [PMDABPF_CLANG=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdabpf-clang=$withval"],
    [PMDABPF_CLANG=''])

AC_ARG_WITH([pmdabpf-llvm-strip],
    [AC_HELP_STRING([--with-pmdabpf-llvm-strip],
                    [path to llvm-strip 10+ (default is empty for auto discovery)])],
    [PMDABPF_LLVM_STRIP=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdabpf-llvm-strip=$withval"],
    [PMDABPF_LLVM_STRIP=''])

AC_ARG_WITH([pmdabpf-bpftool],
    [AC_HELP_STRING([--with-pmdabpf-bpftool],
                    [path to bpftool (default is empty for auto discovery)])],
    [PMDABPF_BPFTOOL=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdabpf-bpftool=$withval"],
    [PMDABPF_BPFTOOL=''])

AC_ARG_WITH([pmdabpf-btf],
    [AC_HELP_STRING([--with-pmdabpf-btf],
                    [path to kernel BTF sources (default is empty to use vendored kernel type information)])],
    [PMDABPF_BTF=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdabpf-btf=$withval"],
    [PMDABPF_BTF=''])

AC_ARG_WITH([pmdabpftrace],
    [AC_HELP_STRING([--with-pmdabpftrace],
		   [enable bpftrace pmda (default is on)])],
    [do_pmdabpftrace=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdabpftrace=$withval"],
    [do_pmdabpftrace=check])

AC_ARG_WITH([pmdajson],
    [AC_HELP_STRING([--with-pmdajson],
		   [enable JSON pmda (default is on)])],
    [do_pmdajson=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdajson=$withval"],
    [do_pmdajson=check])

AC_ARG_WITH([pmdanutcracker],
    [AC_HELP_STRING([--with-pmdanutcracker],
		   [enable nutcracker pmda (default is on)])],
    [do_pmdanutcracker=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdanutcracker=$withval"],
    [do_pmdanutcracker=check])

AC_ARG_WITH([pmdasnmp],
    [AC_HELP_STRING([--with-pmdasnmp],
		   [enable SNMP pmda (default is on)])],
    [do_pmdasnmp=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-pmdasnmp=$withval"],
    [do_pmdasnmp=check])

AC_ARG_WITH([selinux],
    [AC_HELP_STRING([--with-selinux],
                    [enable building of selinux package (default is on)])],
    [do_selinux=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-selinux=$withval"],
    [do_selinux=check])
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

AC_ARG_WITH(
    [transparent-decompression],
    [AC_HELP_STRING([--with-transparent-decompression],
		    [enable support for transparent decompression of archives (default is on)])],
    [do_decompression=$withval; PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --with-transparent-decompression=$withval"],
    [do_decompression=check])


dnl
dnl Note: the following environment variables may be set to override defaults:
dnl MAKE CC CPP LD LEX YACC INSTALL AWK SED ECHO
dnl

dnl Guess target platfrom
AC_CANONICAL_SYSTEM
if test -z "$target"
then
    echo '
FATAL ERROR: Cannot guess your target, try explicit specification
             using --target or mailto: pcp@groups.io to ask.'
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

gcc_debug=true
target_distro=$target_os
if test $target_os = linux
then
    AC_DEFINE(IS_LINUX, [1], [Platform is Linux])
    test -f /etc/SuSE-release && target_distro=suse
    target_distro_variant=''
    if test -f /etc/os-release
    then
	grep -q 'ID_LIKE="suse"' /etc/os-release 2>/dev/null && target_distro=suse
	grep -qi 'opensuse"' /etc/os-release 2>/dev/null && target_distro_variant=opensuse
    fi
    test -f /etc/fedora-release && target_distro=fedora
    test -f /etc/redhat-release && target_distro=redhat
    test -f /etc/debian_version && target_distro=debian
    test -f /etc/slackware-version && target_distro=slackware
    test -f /etc/gentoo-release && target_distro=gentoo
    test -f /etc/mandriva-release && target_distro=mandriva
    test -f /etc/arch-release && target_distro=arch
    export CFLAGS="-fPIC -fno-strict-aliasing -D_GNU_SOURCE"
    pcp_platform_paths='/usr/bin/X11:/usr/local/bin'
    pcp_ps_all_flags=-efw
elif test $target_os = darwin
then
    AC_DEFINE(IS_DARWIN, [1], [Platform is Darwin (Mac OS X)])
    target_distro=macosx
    version=`sw_vers -productVersion`
    CFLAGS="-fPIC -no-cpp-precomp -fno-strict-aliasing"
    case "$version"
    in
	10.[0-5].*)
	    dnl no knowledge before this ...
	    ;;
	10.[6-9].*|10.1[0-3].*)
	    target_distro=cocoa
	    CFLAGS="$CFLAGS -arch i386 -arch x86_64"
	    ;;
	*)
	    dnl at least 10.14 (Mojave) ...
	    target_distro=cocoa
	    CFLAGS="$CFLAGS -arch x86_64"
	    ;;
    esac
    export CFLAGS
    pcp_platform_paths='/usr/local/bin'
    pcp_ps_all_flags="-axw -o user,pid,ppid,cpu,stime,tty,time,command"
elif test $target_os = mingw
then
    AC_DEFINE(IS_MINGW, [1], [Platform is MinGW (Windows)])
    CFLAGS="-fno-strict-aliasing"
    pcp_platform_paths=''
    pcp_ps_all_flags=-efW
elif test $target_os = solaris
then
    AC_DEFINE(IS_SOLARIS, [1], [Platform is Solaris])
    export CFLAGS_IF_GCC="-fPIC -fno-strict-aliasing -D_XPG4_2 -D__EXTENSIONS__"
    export CFLAGS_IF_SUNCC="-fPIC -xalias_level=any -D_XPG4_2 -D__EXTENSIONS__"
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
    if test $target_os = kfreebsd
    then
	pcp_ps_all_flags=-efw
    else
	pcp_ps_all_flags=auxww
    fi
    pcp_platform_paths='/usr/bin/X11'
    test -d /usr/local/bin && pcp_platform_paths="$pcp_platform_paths:/usr/local/bin"
    test -d /usr/bsd && pcp_platform_paths="$pcp_platform_paths:/usr/bsd"
elif test $target_os = gnu
then
    AC_DEFINE(IS_GNU, [1], [Platform is GNU Hurd])
    test -f /etc/debian_version && target_distro=debian
    export CFLAGS="-fPIC -fno-strict-aliasing -D_GNU_SOURCE"
    pcp_platform_paths='/usr/bin/X11:/usr/local/bin'
    pcp_ps_all_flags=-efw
elif test $target_os = netbsdelf -o $target_os = netbsd
then
    target_os=netbsd
    AC_DEFINE(IS_NETBSD, [1], [Platform is NetBSD])
    export CFLAGS="-fPIC -fno-strict-aliasing -D_GNU_SOURCE -D_NETBSD_SOURCE"
    pcp_platform_paths='/usr/pkg/bin'
    pcp_ps_all_flags=auxww
elif test $target_os = openbsd
then
    target_os=openbsd
    AC_DEFINE(IS_OPENBSD, [1], [Platform is OpenBSD])
    export CFLAGS="-fPIC -fno-strict-aliasing -D_GNU_SOURCE"
    pcp_platform_paths='/usr/local/bin'
    pcp_ps_all_flags="-axw -o user,pid,ppid,%cpu,start,tty,time,args"
    if test "$target_cpu" = "i386" -a "$host_os" = "openbsd6.8"
    then
	dnl build with -g are broken here due to a gcc bug
	gcc_debug=false
    fi
else
    echo
    echo "FATAL ERROR: need platform-specific customization for \"$target_os\""
    rm -rf conftest conftest.*
    exit 1
fi
AC_SUBST(pcp_platform_paths)
AC_SUBST(pcp_ps_all_flags)

dnl default C and C++ compiler optimization to -O2 -g (latter depends on $gcc_debug)
if test "$use_optimization" = no
then
    optimization="-O0"
    $gcc_debug && optimization="$optimization -g"
else
    optimization="-O2"
    $gcc_debug && optimization="$optimization -g"
fi
AC_SUBST(optimization)

PKG_PROG_PKG_CONFIG
if test -z "$PKG_CONFIG"; then
    AC_MSG_ERROR([cannot find a 'pkg-config' command])
fi

dnl check if user wants their own C compiler
cflags_abi=
AC_PROG_CC(suncc egcc gcc cc clang)
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

dnl Add some security-related gcc flags
if echo "$CFLAGS" | grep stack-protector >/dev/null
then
    dnl already set, probably from the environment
    :
else
    AC_ARG_ENABLE([ssp],
    [AS_HELP_STRING([--disable-ssp], [disable gcc stack-protector])])
    AS_IF([test "x$cc_is_gcc" = xyes -a "x$enable_ssp" != xno -a "x$use_optimization" = xyes ],[
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
dnl Add other non-default gcc static checking flags
if ! echo "$CFLAGS" | grep -- -Wshadow >/dev/null
then
    AS_IF([test "x$cc_is_gcc" = xyes ],[PCFLAGS="$PCFLAGS -Wshadow"])
fi
dnl and we need to worry about off_t ... we need it to be 64-bits for V3 archives
AC_MSG_CHECKING([sizeof off_t])
cat <<End-of-File >conftest.c
#include <sys/types.h>
main() { printf("%d", (int)sizeof(off_t)); }
End-of-File
(eval $ac_compile) 2>&5
(eval $ac_link) 2>&5
ans=`./conftest`
echo "./conftest -> \"$ans\"" >&5
rm -rf conftest conftest.*
AC_MSG_RESULT($ans)
if test "$ans" = 4; then
    PCFLAGS="$PCFLAGS -D_FILE_OFFSET_BITS=64"
fi
AC_SUBST(PCFLAGS)
AC_SUBST(PLDFLAGS)

dnl Check for even more security-related gcc/linker flags, useful for daemons
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

dnl Check for support for symbol hiding via gcc flags
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

dnl Check for (opt-in) support for --std=c99
AC_MSG_CHECKING([for --std=c99 support])
AS_IF([test "x$cc_is_gcc" = xyes],[
    C99_CFLAGS="--std=c99"
    save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $C99_CFLAGS"
AC_COMPILE_IFELSE(
   [AC_LANG_PROGRAM([[#include <stdio.h>]],
		    [[for (int i=0; i<1; i++)]])],
   [AC_MSG_RESULT([yes])],
   [AC_MSG_RESULT([no])
    C99_CFLAGS=""])
   CFLAGS="$save_CFLAGS"
])
AC_SUBST(C99_CFLAGS)

dnl Check for a C++ compiler
AC_PROG_CXX(eg++ g++ c++ clang++)
cxx=$CXX
AC_SUBST(cxx)
AC_MSG_CHECKING([for working C++ compiler])
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE(
   [AC_LANG_PROGRAM([[#include <cstdio>]],
                    [[const char gday[] = "G'day, world\n";]])],
   [AC_MSG_RESULT([yes])],
   [AC_MSG_RESULT([no])
    cxx=""])
AC_LANG_POP([C++])

dnl Prevent shared libraries from being built for libpcp and other core libraries
AC_ARG_ENABLE([shared],
  [AS_HELP_STRING([--disable-shared], [disable core shared libary generation])],
  [PACKAGE_CONFIGURE="$PACKAGE_CONFIGURE --disable-shared=$withval"],
  [enable_shared=false])
AC_SUBST(enable_shared)

dnl check if user wants to use any of their own commands;
dnl ordering is important: some tests use earlier results

test -z "$AWK" && AC_PATH_PROGS(AWK, gawk awk, /usr/bin/awk)
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
if test ! -x "$SED"; then
    AC_MSG_ERROR([cannot find a valid 'sed' command.])
fi
sed=$SED
AC_SUBST(sed)

if test -z "$ECHO"; then
    AC_PATH_PROG(ECHO, echo, /bin/echo)
fi
if test ! -x "$ECHO"; then
    AC_MSG_ERROR([cannot find a valid 'echo' command.])
fi
echo=$ECHO
AC_SUBST(echo)

if test -z "$FIND"; then
    AC_PATH_PROG(FIND, find, /usr/bin/find)
fi
if test ! -x "$FIND"; then
    AC_MSG_ERROR([cannot find a valid 'find' command.])
fi
find=$FIND
AC_SUBST(find)

if test -z "$WHICH"; then
    AC_PATH_PROG(WHICH, which, /usr/bin/which)
fi
if test ! -x "$WHICH"; then
    AC_MSG_ERROR([cannot find a valid 'which' command.])
fi
which=$WHICH
AC_SUBST(which)

if test -z "$AR"; then
    AC_PATH_PROGS(AR, [gcc-ar ar], /usr/bin/ar)
fi
if test ! -x "$AR"; then
    AC_MSG_ERROR([cannot find a valid 'ar' command.])
fi
ar=$AR
AC_SUBST(ar)

AC_PROG_LEX
lex=`$echo $LEX | awk '{print $1}'`
lex=`$which "$lex"`
if test ! -x "$lex"; then
    AC_MSG_ERROR([cannot find a valid 'lex'/'flex' command.])
fi
lex=$LEX
AC_SUBST(lex)

AC_PROG_YACC
yacc=`$echo $YACC | awk '{print $1}'`
yacc=`$which "$yacc"`
if test ! -x "$yacc"; then
    AC_MSG_ERROR([cannot find a valid 'yacc'/'bison' command.])
fi
dnl if this is bison, don't use the -y option
yacc=`echo $YACC | sed -e '/^bison /{
s/$/ /
s/ -y / /
s/ $//
}'`
AC_SUBST(yacc)

AC_CHECK_PROGS(RAGEL, ragel, [])
AC_SUBST(RAGEL)

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
    sort=`$which sort`
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
  echo_n= echo_c='\\c'
  AC_MSG_RESULT([backslash-c])
fi
AC_SUBST(echo_n)
AC_SUBST(echo_c)

dnl subsequent configure checks rely on $EGREP being set
AC_PROG_EGREP

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

dnl Check for gperftools
AS_IF([test "x$use_gperftools" != "xno"], [
    PKG_CHECK_MODULES([gperftools], [libprofiler, libtcmalloc])
])

dnl Check for sanitizers
sanitizer=""
if test "x$use_sanitizer" != "xno"; then
    sanitizer="-fsanitize=$use_sanitizer"
fi
AC_SUBST(sanitizer)

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
AC_MSG_CHECKING([if systemd should be used])
AS_IF([test "x$do_systemd" != "xno"], [
    enable_systemd=true

    PKG_CHECK_VAR([SYSTEMD_TMPFILESDIR], [systemd], [tmpfilesdir],
	[pcp_systemdtmpfiles_dir=$SYSTEMD_TMPFILESDIR],
	[pcp_systemdtmpfiles_dir=$pcp_lib32_dir/tmpfiles.d])
    AC_SUBST(pcp_systemdtmpfiles_dir)
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
if $enable_systemd
then
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi
AC_SUBST(enable_systemd)

if $enable_systemd
then
    dnl sd_notify() is needed in __pmServerNotifySystemd(), but only if systemd
    dnl is being used
    AC_CHECK_HEADERS([systemd/sd-daemon.h], [have_sd_daemon=true], [have_sd_daemon=false])
    $have_sd_daemon || echo WARNING: using systemd, but header systemd/sd-daemon.h not installed

fi

sd_service_type="simple"
if $enable_systemd
then
    dnl if systemd version is 239 or newer, use type=exec for logutil scripts
    dnl else fallback to type=simple (e.g. on centos7 and some other platforms)
    systemd_version=`systemd-analyze --version | $AWK '/^systemd/ {print $2}'`
    if test "$systemd_version" -ge 239
    then
    	sd_service_type="exec"
    fi
fi
AC_SUBST(sd_service_type)

dnl NB: No AC_PREFIX_DEFAULT is needed, as the default configure invocation
dnl targets a build for non-system directories such as /usr/local.
dnl AC_PREFIX_DEFAULT([])

. ./VERSION.pcp
PACKAGE_VERSION=${PACKAGE_MAJOR}.${PACKAGE_MINOR}.${PACKAGE_REVISION}
AC_SUBST(PACKAGE_VERSION)
AC_SUBST(PACKAGE_MAJOR)
AC_SUBST(PACKAGE_MINOR)
AC_SUBST(PACKAGE_REVISION)
AC_SUBST(PACKAGE_BUILD)

if test -z "$PACKAGE_BUILD_DATE" ; then
    # use SOURCE_DATE_EPOCH for a deterministic builddate (reproducible builds)
    PACKAGE_BUILD_DATE=`TZ=UTC date -d "@${SOURCE_DATE_EPOCH:-$(date +%s)}" +%Y-%m-%d 2>/dev/null`
    test $? -eq 0 || PACKAGE_BUILD_DATE=`date +%Y-%m-%d`
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
AC_OUTPUT(src/include/pcp/platform_defs.h src/include/pcp/platformsz.h)

AC_CHECK_PROGS(GIT, git)
AC_SUBST(GIT)

AC_CHECK_PROGS(PYLINT, pylint, /bin/true)
AC_SUBST(PYLINT)

dnl check if python available for the build and runtime
AC_CHECK_PROGS(PYTHON, [python2 python2.7 python])
AC_SUBST(PYTHON)

dnl check if python3 available for the build and runtime
AC_CHECK_PROGS(PYTHON3, [python3 python3.7 python3.6 python3.4])
AC_SUBST(PYTHON3)

dnl check availability of Python header files
dnl usage PCP_CHECK_PYTHON_HEADER(major, minor)
dnl
AC_DEFUN([PCP_CHECK_PYTHON_HEADER],
  [ PY_MAJOR="$1"
    PY_MINOR="$2"
    PY_VERSION="python$PY_MAJOR.$PY_MINOR"
    PY_INCLUDE_DIRS="-I/usr/include/${PY_VERSION}m -I/usr/include/${PY_VERSION}"
    saved_CFLAGS="$CFLAGS"
    saved_CPPFLAGS="$CPPFLAGS"
    CFLAGS="$CFLAGS $PY_INCLUDE_DIRS"
    CPPFLAGS="$CPPFLAGS $PY_INCLUDE_DIRS"

    AC_CHECK_HEADERS([Python.h], [], [
	echo WARNING: Python version $PY_MAJOR.$PY_MINOR header file may be missing.
	echo Proceeding with the Python $PY_MAJOR installation found, regardless.])

    CPPFLAGS="$saved_CPPFLAGS"
    CFLAGS="$saved_CFLAGS"
  ])

dnl check if python tools/packages wanted (need python >= 2.7)
enable_python2=false
AS_IF([test "x$do_python" != "xno"], [
    enable_python2=true

    if test "$do_python" = "check" -a -f /etc/os-release
    then
    (
	. /etc/os-release
	case "$ID"
	in
	    debian)
		if test -z "$VERSION_ID" -o "$VERSION_ID" -ge 11; then
		    touch config.python2_eol
		fi
		;;
	    ubuntu)
		VERSION_NO=`echo "$VERSION_ID" | sed 's/[[^0-9]]//'`
		if test "$VERSION_NO" -ge 2004; then
		    touch config.python2_eol
		fi
		;;
	    fedora)
		if test -z "$VERSION_ID" -o "$VERSION_ID" -ge 35; then
		    touch config.python2_eol
		fi
		;;
	    rhel)
		VERSION_NO=`echo "$VERSION_ID" | sed 's/[[^0-9]]//'`
		if test "$VERSION_NO" -ge 90; then
		    touch config.python2_eol
		fi
		;;
	    centos)
		if test "$VERSION_ID" -ge 9; then
		    touch config.python2_eol
		fi
		;;
	esac
    )
    fi

    if test -f config.python2_eol
    then
	rm config.python2_eol
	echo "WARNING: Python 2 is not supported anymore (EOL since 2020), please use Python 3 instead."
	enable_python2=false
    fi

    if test "$enable_python2" = "true" -a -z "$PYTHON"
    then
	enable_python2=false
    fi

    if $enable_python2
    then
	AC_MSG_CHECKING([Python2 version])
	eval `$PYTHON -V 2>&1 | awk '/^Python/ { ver=2; print $ver }' | awk -F. '{ major=1; minor=2; point=3; printf "export PY_MAJOR=%d PY_MINOR=%d PY_POINT=%d\n",$major,$minor,$point }'`
	AC_MSG_RESULT([$PY_MAJOR.$PY_MINOR.$PY_POINT])
	if test "$PY_MAJOR" -lt 2; then
	    echo WARNING: Python version 2.7 or later does not seem to be installed.
	    enable_python2=false
	else
	    if test "$PY_MAJOR" -eq 2 -a "$PY_MINOR" -lt 7 ; then
		echo WARNING: Python version 2.$PY_MINOR is too old.
		echo Python version 2.7 or later is required for Python builds.
		enable_python2=false
	    else
		PCP_CHECK_PYTHON_HEADER([$PY_MAJOR], [$PY_MINOR])
	    fi
	fi
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
	    enable_python3=false
	else
	    if test "$PY_MAJOR" -eq 3 -a "$PY_MINOR" -lt 3 ; then
		echo WARNING: Python version 3.$PY_MINOR is too old.
		echo Python version 3.3 or later is required for Python builds.
		enable_python3=false
	    else
		PCP_CHECK_PYTHON_HEADER([$PY_MAJOR], [$PY_MINOR])
	    fi
	fi
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
if $have_python; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

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

dnl Enable for support for the /usr/bin/dstat compatibility symlink
AC_MSG_CHECKING(for dstat symlink)
enable_dstat=false
if test "$do_dstat_symlink" = "check" -o "$do_dstat_symlink" = "yes"
then
    if test "$have_python" = "false" -a "$do_dstat_symlink" = "yes"
    then
	AC_MSG_ERROR([cannot enable dstat symlink - no python found])
    else
	enable_dstat=true
    fi
fi
AC_SUBST(enable_dstat)
if $enable_dstat; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

dnl check availability of some Python modules
dnl usage PCP_CHECK_PYTHON_MODULE(module_name) or
dnl PCP_CHECK_PYTHON_MODULE(module_name, <import string>)
dnl default <import string> is "import module_name"
dnl
dnl sets $have_python_<module_name> as true or false
dnl
AC_DEFUN([PCP_CHECK_PYTHON_MODULE],
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
PCP_CHECK_PYTHON_MODULE(BPF, [from bcc import BPF])
AC_SUBST(have_python_BPF)
PCP_CHECK_PYTHON_MODULE(OrderedDict, [from collections import OrderedDict])
AC_SUBST(have_python_OrderedDict)
PCP_CHECK_PYTHON_MODULE(jsonpointer)
AC_SUBST(have_python_jsonpointer)
PCP_CHECK_PYTHON_MODULE(openpyxl)
AC_SUBST(have_python_openpyxl)
PCP_CHECK_PYTHON_MODULE(setuptools)
AC_SUBST(have_python_setuptools)
PCP_CHECK_PYTHON_MODULE(requests)
AC_SUBST(have_python_requests)
PCP_CHECK_PYTHON_MODULE(libvirt)
AC_SUBST(have_python_libvirt)
PCP_CHECK_PYTHON_MODULE(six)
AC_SUBST(have_python_six)

AC_MSG_CHECKING([if the python InfluxDB support should be included])
have_python_influxdb=false
if test "$have_python" = true
then
    $have_python_requests && $have_python_OrderedDict && have_python_influxdb=true
fi
AC_SUBST(have_python_influxdb)
if $have_python_influxdb; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the json PMDA should be included])
pmda_json=false
AS_IF([test "x$do_pmdajson" = "xyes"], [pmda_json=true])
AS_IF([test "x$do_pmdajson" = "xcheck"], [
    dnl JSON PMDA needs both the six and jsonpointer Python modules
    $have_python_jsonpointer && $have_python_six && \
	$have_python_OrderedDict && pmda_json=true
])
AC_SUBST(PMDA_JSON, $pmda_json)
if $pmda_json; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the bcc PMDA should be included])
pmda_bcc=false
AS_IF([test "x$do_pmdabcc" = "xyes"], [pmda_bcc=true])
AS_IF([test "x$do_pmdabcc" = "xcheck"], [
    dnl BCC PMDA needs python3, and OrderedDict and BPF Python modules
    $enable_python3 && $have_python_BPF && \
	$have_python_OrderedDict && pmda_bcc=true
])
AC_SUBST(PMDA_BCC, $pmda_bcc)
if $pmda_bcc; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([clang major version installed])
if which llvm-config >/dev/null 2>&1
then
    clang_major_version=`llvm-config --version | cut -f1 -d.`
else
    dnl version=0 => not installed
    clang_major_version=0
fi
AC_MSG_RESULT($clang_major_version)
AC_SUBST(CLANG_MAJOR_VERSION, [$clang_major_version])

PKG_CHECK_MODULES([libbpf], [libbpf >= 0.4.0], [have_libbpf=true], [have_libbpf=false])
AC_SUBST(HAVE_LIBBPF, [$have_libbpf])
AS_IF([$have_libbpf], [
    AC_MSG_CHECKING([libbpf version])
    libbpf_version=`$PKG_CONFIG --modversion libbpf`
    AC_SUBST(libbpf_version)
    AC_MSG_RESULT($libbpf_version)
])

PKG_CHECK_MODULES([libelf], [libelf], [have_libelf=true], [have_libelf=false])
AC_SUBST(HAVE_LIBELF, [$have_libelf])

dnl Auto-discover clang if not provided by the user
if test -z "$PMDABPF_CLANG"; then
    AC_PATH_PROG(PMDABPF_CLANG, clang)
fi
pmdabpf_clang=$PMDABPF_CLANG
AC_SUBST(pmdabpf_clang)

dnl Auto-discover llvm-strip if not provided by the user
if test -z "$PMDABPF_LLVM_STRIP"; then
    AC_PATH_PROG(PMDABPF_LLVM_STRIP, llvm-strip)
fi
pmdabpf_llvm_strip=$PMDABPF_LLVM_STRIP
AC_SUBST(pmdabpf_llvm_strip)

dnl Auto-discover bpftool if not provided by the user
if test -z "$PMDABPF_BPFTOOL"; then
    AC_PATH_PROG(PMDABPF_BPFTOOL, bpftool)
fi
pmdabpf_bpftool=$PMDABPF_BPFTOOL
AC_SUBST(pmdabpf_bpftool)

dnl Get BPF CO-RE arch identifier
AC_MSG_CHECKING([BPF CO-RE architecture identifier])
pmdabpf_arch=`uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/' | sed 's/ppc64le/powerpc/' | sed 's/mips.*/mips/'`
AC_SUBST(pmdabpf_arch)
AC_MSG_RESULT($pmdabpf_arch)

dnl Generate vmlinux.h if PMDABPF_BTF is provided by the user,
dnl otherwise fall back to vendored vmlinux.h
pmdabpf_vmlinuxh=""
if test -n "$PMDABPF_BTF"; then
    pmdabpf_vmlinuxh="`pwd`/src/pmdas/bpf/vmlinux.h"
    AC_MSG_NOTICE([generating $pmdabpf_vmlinuxh from $PMDABPF_BTF])
    $PMDABPF_BPFTOOL btf dump file "$PMDABPF_BTF" format c > $pmdabpf_vmlinuxh
else
    pmdabpf_arch_vmlinux="`pwd`/vendor/github.com/iovisor/bcc/libbpf-tools/$pmdabpf_arch/vmlinux.h"
    if test -f "$pmdabpf_arch_vmlinux"; then
	pmdabpf_vmlinuxh="$pmdabpf_arch_vmlinux"
    else
	AC_MSG_NOTICE([vmlinux.h for architecture $pmdabpf_arch is not vendored, please use --with-pmdabpf-btf to generate a vmlinux.h for $pmdabpf_arch])
    fi
fi
AC_SUBST(pmdabpf_vmlinuxh)
AC_MSG_CHECKING([for vmlinux.h])
if test -f "$pmdabpf_vmlinuxh"; then AC_MSG_RESULT($pmdabpf_vmlinuxh); else AC_MSG_RESULT(not found); fi

AC_MSG_CHECKING([if the bpf PMDA should be included])
pmda_bpf=false
AS_IF([test "x$do_pmdabpf" = "xyes"], [pmda_bpf=true])
AS_IF([test "x$do_pmdabpf" = "xcheck"], [
    dnl pmdabpf requires libbpf, libelf and bpftool
    $have_libbpf && $have_libelf && \
	test $clang_major_version -ge 10 && \
	test -x "$pmdabpf_bpftool" && \
	test -f "$pmdabpf_vmlinuxh" && \
	pmda_bpf=true
])
AC_SUBST(PMDA_BPF, $pmda_bpf)
if $pmda_bpf; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

if $pmda_bpf; then
    libbpf_version_major=`echo $libbpf_version | cut -d. -f1`
    libbpf_version_minor=`echo $libbpf_version | cut -d. -f2`
    pmdabpf_modules="biolatency.so runqlat.so"

    AC_MSG_CHECKING([if the bpf PMDA execsnoop module should be included])
    if test "$libbpf_version_major" -eq 0 -a "$libbpf_version_minor" -lt 7; then
	AC_MSG_RESULT([no (libbpf version required: 0.7.0, installed: $libbpf_version)])
    else
	pmdabpf_modules="${pmdabpf_modules} execsnoop.so"
	AC_MSG_RESULT(yes)
    fi

    AC_SUBST(pmdabpf_modules)
    AC_MSG_CHECKING([which bpf PMDA modules should be included])
    AC_MSG_RESULT($pmdabpf_modules)
fi

AC_MSG_CHECKING([if the bpftrace PMDA should be included])
pmda_bpftrace=false
AS_IF([test "x$do_pmdabpftrace" = "xyes"], [pmda_bpftrace=true])
AS_IF([test "x$do_pmdabpftrace" = "xcheck"], [
    dnl bpftrace PMDA needs bpftrace executable, python 3.6+ and bpftrace
    dnl with JSON support
    have_bpftrace=false
    if which bpftrace >/dev/null 2>&1
    then
	if bpftrace --help 2>&1 | grep '\-f.*json' > /dev/null; then
	    have_bpftrace=true
	fi
    fi
    $enable_python3 && test "$PY_MAJOR" -eq 3 -a "$PY_MINOR" -ge 6 && \
    $have_bpftrace && pmda_bpftrace=true
])
AC_SUBST(PMDA_BPFTRACE, $pmda_bpftrace)
if $pmda_bpftrace; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

dnl check if perl available for the build and runtime
AC_CHECK_PROGS(PERL, perl)
AC_SUBST(PERL)

enable_perl=false
AS_IF([test "x$do_perl" != "xno"], [
    enable_perl=true
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
    AC_MSG_RESULT($pcp_perl_prog)
], [have_perl=false])
AC_SUBST(pcp_perl_prog)
AC_SUBST(enable_perl)
AC_SUBST(have_perl)

AC_MSG_CHECKING([if the SNMP PMDA should be included])
pmda_snmp=false
AS_IF([test "x$do_pmdasnmp" = "xyes"], [pmda_snmp=true])
AS_IF([test "x$do_pmdasnmp" = "xcheck"], [
    dnl SNMP PMDA needs perl and the Net::SNMP module
    if test "$have_perl" = true
    then
	if $pcp_perl_prog -e "use Net::SNMP" >/dev/null 2>&1
	then
	    pmda_snmp=true
	fi
    fi
])
AC_SUBST(PMDA_SNMP, $pmda_snmp)
if $pmda_snmp; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the mysql PMDA should be included])
pmda_mysql=false
if test "$have_perl" = true
then
    dnl rpm build for mysql PMDA needs Perl DBI and DBD::mysql
    if $pcp_perl_prog -e "use DBI" >/dev/null 2>&1
    then
	if $pcp_perl_prog -e "use DBD::mysql" >/dev/null 2>&1
	then
	    pmda_mysql=true
	fi
    fi
fi
AC_SUBST(PMDA_MYSQL, $pmda_mysql)
if $pmda_mysql; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the mongodb PMDA should be included])
pmda_mongodb=false
if test "$have_python" = true
then
    dnl rpm build for mongodb PMDA needs python pymongo module
    if $pcp_python_prog -c "import pymongo" >/dev/null 2>&1
    then
	pmda_mongodb=true
    fi
fi
AC_SUBST(PMDA_MONGODB, $pmda_mongodb)
if $pmda_mongodb; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the postgresql PMDA should be included])
pmda_postgresql=false
if test "$have_python" = true
then
    dnl rpm build for postgresql PMDA needs python psycopg2 module
    if $pcp_python_prog -c "import psycopg2" >/dev/null 2>&1
    then
	pmda_postgresql=true
    fi
fi
AC_SUBST(PMDA_POSTGRESQL, $pmda_postgresql)
if $pmda_postgresql; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the oracle PMDA should be included])
pmda_oracle=false
if test "$have_perl" = true
then
    dnl rpm build for oracle PMDA needs Perl DBI
    if $pcp_perl_prog -e "use DBI" >/dev/null 2>&1
    then
	pmda_oracle=true
    fi
fi
AC_SUBST(PMDA_ORACLE, $pmda_oracle)
if $pmda_oracle; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the elasticsearch PMDA should be included])
pmda_elasticsearch=false
if test "$have_python" = true
then
    dnl rpm build for elasticsearch PMDA needs Python urllib module
    if $pcp_python_prog -c "import urllib" >/dev/null 2>&1
    then
	pmda_elasticsearch=true
    fi
    if $pcp_python_prog -c "import urllib2" >/dev/null 2>&1
    then
	pmda_elasticsearch=true
    fi

fi
AC_SUBST(PMDA_ELASTICSEARCH, $pmda_elasticsearch)
if $pmda_elasticsearch; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the nginx PMDA should be included])
pmda_nginx=false
if test "$have_perl" = true
then
    dnl rpm build for nginx PMDA needs Perl LWP::UserAgent
    if $pcp_perl_prog -e "use LWP::UserAgent" >/dev/null 2>&1
    then
	pmda_nginx=true
    fi
fi
AC_SUBST(PMDA_NGINX, $pmda_nginx)
if $pmda_nginx; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the activemq PMDA should be included])
pmda_activemq=false
if test "$have_perl" = true
then
    dnl rpm build for activemq PMDA needs Perl modules LWP::UserAgent
    dnl and JSON
    pmda_activemq=true
    for module in 'LWP::UserAgent' JSON
    do
	if $pcp_perl_prog -e "use $module" >/dev/null 2>&1
	then
	    :
	else
	    pmda_activemq=false
	    break
	fi
    done
fi
AC_SUBST(PMDA_ACTIVEMQ, $pmda_activemq)
if $pmda_activemq; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the bind2 PMDA should be included])
pmda_bind2=false
if test "$have_perl" = true
then
    dnl rpm build for bind2 PMDA needs Perl LWP::UserAgent, XML::LibXML
    if $pcp_perl_prog -e "use LWP::UserAgent" >/dev/null 2>&1
    then
	if $pcp_perl_prog -e "use XML::LibXML" >/dev/null 2>&1
	then
	    if $pcp_perl_prog -e "use File::Slurp" >/dev/null 2>&1
	    then
	        pmda_bind2=true
	    fi
	fi
    fi
fi
AC_SUBST(PMDA_BIND2, $pmda_bind2)
if $pmda_bind2; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the nutcracker PMDA should be included])
pmda_nutcracker=false
AS_IF([test "x$do_pmdanutcracker" = "xyes"], [pmda_nutcracker=true])
AS_IF([test "x$do_pmdanutcracker" = "xcheck"], [
    dnl build for nutcracker PMDA needs perl JSON & YAML::XS modules
    if test "$have_perl" = true
    then
	if $pcp_perl_prog -e "use YAML::XS" >/dev/null 2>&1
	then
	    if $pcp_perl_prog -e "use JSON" >/dev/null 2>&1
	    then
		pmda_nutcracker=true
	    fi
	fi
    fi
])
AC_SUBST(PMDA_NUTCRACKER, $pmda_nutcracker)
if $pmda_nutcracker; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the LIO PMDA should be included])
pmda_lio=false
if test "$have_python" = true
then
    dnl rpm build for LIO PMDA needs Python rtslib module
    if $pcp_python_prog -c "import rtslib" >/dev/null 2>&1
    then
	pmda_lio=true
    fi
fi
AC_SUBST(PMDA_LIO, $pmda_lio)
if $pmda_lio; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the OpenMetrics PMDA should be included])
pmda_openmetrics=false
if test "$have_python" = true
then
    $have_python_requests && $have_python_OrderedDict && pmda_openmetrics=true
fi
AC_SUBST(PMDA_OPENMETRICS, $pmda_openmetrics)
if $pmda_openmetrics; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the libvirt PMDA should be included])
pmda_libvirt=false
if test "$have_python" = true
then
    dnl rpm build for libvirt PMDA needs Python libvirt and lxml modules
    if test "$have_python_libvirt" = true
    then
	if $pcp_python_prog -c "import lxml" >/dev/null 2>&1
	then
	    pmda_libvirt=true
	fi
    fi
fi
AC_SUBST(PMDA_LIBVIRT, $pmda_libvirt)
if $pmda_libvirt; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

qmake=$QMAKE
enable_qt=false
qt_release=release
qt_version=0
AS_IF([test "x$do_qt" != "xno"], [
    enable_qt=true

    # Allow specifying the QT directory using --with-qt=/the/qt/dir
    case "$do_qt" in
    /*) export QTDIR="$do_qt" ; do_qt="check" ;;
    esac

    if test -z "$QMAKE"
    then
	dnl default to qmake from qtchooser whenever it is available
	dnl ... unfortunately this is not always correct, as when more
	dnl than one Qt version is installed, the default may not be our
	dnl preferred version, which is Qt5
	AC_PATH_PROGS(QTCHOOSER, [qtchooser],, [$QTDIR/bin:/usr/lib/qtchooser:/usr/bin:/usr/lib64/qt5/bin:/usr/lib/qt5/bin:/usr/lib64/qt4/bin:/usr/lib/qt4/bin:/usr/local/bin])
	AC_PATH_PROGS(QMAKE, [qmake qmake-qt5 qmake-qt4],, [$QTDIR/bin:/usr/lib/qtchooser:/usr/bin:/usr/lib64/qt5/bin:/usr/lib/qt5/bin:/usr/lib64/qt4/bin:/usr/lib/qt4/bin:/usr/local/bin])
	test "x$cc_is_gcc" = xyes -a $target_os = solaris && QMAKE="$QMAKE -spec solaris-g++"
    fi
    qmake=$QMAKE
    if test -z "$QMAKE"
    then
	enable_qt=false
    else
	if test -n "$QT_SELECT"
	then
	    AC_MSG_CHECKING([Qt version, QT_SELECT=$QT_SELECT])
	    dnl QT_SELECT set, check and fallback to Qt4 if given
	    dnl preference is not successful
	    export QT_SELECT
	    $qmake --version 2>&1 >/dev/null
	    if test $? -eq 1; then
		export QT_SELECT=4
	    fi
	else
	    dnl QT_SELECT not set, if we have qtchooser use that to see
	    dnl if Qt5 is available, otherwise we fallthrough to the 
	    dnl qmake default version
	    if test -n "$QTCHOOSER"
	    then
		AC_MSG_CHECKING([Qt version, using qtchooser])
		dnl if Qt5 appears to be present, pick that one, else Qt4
		if $QTCHOOSER -l | grep '^5$' >/dev/null
		then
		    export QT_SELECT=5
		else
		    export QT_SELECT=4
		fi
	    else
		AC_MSG_CHECKING([Qt version])
		unset QT_SELECT
	    fi
	fi
	eval `$qmake --version 2>&1 | awk '/Using Qt version/ { ver=4; print $ver }' | awk -F. '{ major=1; minor=2; point=3; printf "export QT_MAJOR=%d QT_MINOR=%d QT_POINT=%d\n",$major,$minor,$point }'`
	AC_MSG_RESULT([$QT_MAJOR.$QT_MINOR.$QT_POINT])
	if test -z "$QT_MAJOR" -o -z "$QT_MINOR"; then
	    echo WARNING: cannot extract Qt version number from ...
	    $qmake --version
	    echo Cannot include the Qt components in the build.
	    enable_qt=false
	    qt_version=0
	elif test "$QT_MAJOR" -lt 5; then
	    echo WARNING: Qt version 5.6 or later does not seem to be installed.
	    echo Cannot proceed with the Qt $QT_MAJOR installation found.
	    enable_qt=false
	    qt_version=0
	elif test "$QT_MAJOR" -eq 5 -a "$QT_MINOR" -lt 6 ; then
	    echo WARNING: Qt version 5.$QT_MINOR is too old.
	    echo Qt version 5.6 or later is required for Qt builds.
	    enable_qt=false
	    qt_version=0
	else
	    qt_version=$QT_MAJOR
	fi
    fi

    if test "$do_qt" != "check" -a "$enable_qt" != "true"
    then
	AC_MSG_ERROR([cannot enable Qt - no supported Qt installation found])
    elif test "$enable_qt" = "true" -a "x$cxx" = "x"
    then
	enable_qt=false
	test "$do_qt" != "check" && \
	AC_MSG_ERROR([C++ compiler unavailable, cannot build Qt components])
    elif test "$do_qt" = "debug"
    then
	dnl Debug/release style build? (non-debug by default)
	qt_release=debug
	enable_qt=true
    fi
])
AC_SUBST(qt_release)
AC_SUBST(qt_version)
AC_SUBST(enable_qt)
AC_SUBST(qmake)

enable_qt3d=false
AS_IF([test "x$do_qt3d" != "xno"], [
    if test -f /usr/include/Coin3/Inventor/Qt/SoQt.h -o -f /usr/include/Inventor/Qt/SoQt.h
    then
	enable_qt3d=$enable_qt
    elif test x"$do_qt3d" = x"yes"
    then
	AC_MSG_ERROR([cannot enable support for 3D scene graph API])
    fi
])
AC_SUBST(enable_qt3d)

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
	darwin|freebsd|netbsd|openbsd)
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
AS_IF([test "$enable_perl" != "false"], [
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
])

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
dpkg=$DPKG
AC_SUBST(dpkg)

dnl check for the Slackware makepkg packaging tool
if test -z "$MAKEPKG"; then
   AC_PATH_PROG(MAKEPKG, makepkg)
fi
makepkg=$MAKEPKG
AC_SUBST(makepkg)

dnl check if symbolic links are supported
AC_PROG_LN_S
if test $target_os = mingw; then
    as_ln_s=/bin/true
fi

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

dnl Checks for C header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h limits.h malloc.h strings.h syslog.h)
AC_CHECK_HEADERS(stddef.h sched.h dlfcn.h dl.h)
AC_CHECK_HEADERS(sys/times.h sys/resource.h sys/prctl.h)
AC_CHECK_HEADERS(sys/sysinfo.h sys/systeminfo.h)
AC_CHECK_HEADERS(endian.h standards.h sys/byteorder.h)
AC_CHECK_HEADERS(libgen.h sys/mman.h sys/un.h)
AC_CHECK_HEADERS(values.h stdint.h ieeefp.h math.h)
AC_CHECK_HEADERS(pwd.h grp.h regex.h sys/wait.h)
AC_CHECK_HEADERS(termio.h termios.h sys/termios.h)
AC_CHECK_HEADERS(sys/ioctl.h sys/select.h sys/socket.h)
AC_CHECK_HEADERS(netdb.h poll.h)
if test $target_os = darwin -o $target_os = openbsd
then
    AC_CHECK_HEADERS(net/if.h, [], [], [#include <sys/types.h>
#include <sys/socket.h>])
else
    AC_CHECK_HEADERS(net/if.h)
fi
AC_CHECK_HEADERS(netinet/in.h netinet/tcp.h arpa/inet.h)
AC_CHECK_HEADERS(windows.h winsock2.h ws2tcpip.h)
AC_CHECK_HEADERS(execinfo.h bits/wordsize.h)
AC_CHECK_HEADERS(iptypes.h, [], [], [#include <windows.h>])
AC_CHECK_HEADERS(iphlpapi.h, [], [], [#include <winsock2.h>])
AC_CHECK_HEADERS(netioapi.h, [], [], [#include <winsock2.h>])
AC_CHECK_HEADERS(fts.h libzfs.h)
AC_CHECK_HEADERS(sys/statvfs.h sys/statfs.h sys/mount.h)
AC_CHECK_HEADERS(curses.h ncurses.h)
AC_CHECK_HEADERS(ncurses/curses.h ncurses/ncurses.h)
AC_CHECK_HEADERS(ncursesw/curses.h)
AC_CHECK_HEADERS(linux/perf_event.h)

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

PKG_CHECK_MODULES([openssl], [openssl >= 1.1.1], [have_openssl=true], [have_openssl=false])
if $have_openssl
then
    AC_MSG_CHECKING([for SSL_read_ex])
    savedLIBS=$LIBS
    LIBS="$openssl_LIBS"
    AC_TRY_LINK_FUNC(SSL_read_ex,
	have_openssl=true
	AC_MSG_RESULT(yes),
	have_openssl=false
	AC_MSG_RESULT(no))
    LIBS=$savedLIBS
fi
AC_SUBST(HAVE_OPENSSL, [$have_openssl])

PKG_CHECK_MODULES([libuv], [libuv >= 1.0], [have_libuv=true], [have_libuv=false])
AC_SUBST(HAVE_LIBUV, [$have_libuv])
AC_CHECK_LIB(uv, uv_pipe_chmod,
	     AC_DEFINE(HAVE_UV_PIPE_CHMOD, [1], [uv_pipe_chmod API.])
	     have_uv_pipe_chmod=true,
	     have_uv_pipe_chmod=false)

PKG_CHECK_MODULES([ncurses], [ncurses], [have_ncurses=true], [have_ncurses=false])
AC_SUBST(HAVE_NCURSES, [$have_ncurses])

PKG_CHECK_MODULES([ncursesw], [ncursesw], [have_ncursesw=true], [have_ncursesw=false])
AC_SUBST(HAVE_NCURSESW, [$have_ncursesw])
if $have_ncursesw
then
    AC_DEFINE(HAVE_GETMOUSE, [1], [ncursesw getmouse])
    AC_DEFINE(HAVE_SET_ESCDELAY, [1], [ncursesw set_escdelay])
fi

PKG_CHECK_MODULES([DEVMAPPER], [devmapper], [have_devmapper=true], [have_devmapper=false])
if $have_devmapper
then
    AC_MSG_CHECKING([for dm_stats_counter_t ])
    AC_TRY_COMPILE([
            #include <libdevmapper.h>
	],
	[ dm_stats_counter_t ctr; ],
	have_devmapper=true
	AC_MSG_RESULT(yes),
	have_devmapper=false
	AC_MSG_RESULT(no))
fi
AC_SUBST(HAVE_DEVMAPPER, [$have_devmapper])

PKG_CHECK_MODULES([SYSTEMD], [libsystemd], [pmda_systemd=true], [pmda_systemd=false])
if $pmda_systemd
then
    :
else
    dnl libsystemd-journal is an alternative for older distros
    PKG_CHECK_MODULES([SYSTEMD], [libsystemd-journal],
		      [pmda_systemd=true], [pmda_systemd=false])
fi
AC_MSG_CHECKING([if the systemd PMDA should be included])
AC_SUBST(PMDA_SYSTEMD, $pmda_systemd)
if $pmda_systemd; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

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
AC_MSG_CHECKING([if the infiniband PMDA should be included])
AC_SUBST(PMDA_INFINIBAND, $pmda_infiniband)
if $pmda_infiniband; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

pmda_perfevent=false
AS_IF([test "x$do_perfevent" = "xyes"], [
    pmda_perfevent=true
    pfm_libs="-lpfm"
    AC_SUBST(PFM_LIBS, $pfm_libs)
], [test "x$do_perfevent" != "xno" ], [
    enable_perfevent=true

    savedLIBS=$LIBS
    AC_CHECK_LIB([pfm], [pfm_get_os_event_encoding],
		 [pfm_libs="-lpfm"],
		 [enable_perfevent=false])
    AC_CHECK_HEADERS([perfmon/pfmlib_perf_event.h], [], [enable_perfevent=false])
    if test "$do_perfevent" != "check" -a "$enable_perfevent" != "true"
    then
	AC_MSG_ERROR([libpfm is not installed, cannot enable the perfevent PMDA])
    else
	pmda_perfevent=$enable_perfevent
    fi
    LIBS=$savedLIBS
    AC_SUBST(PFM_LIBS, $pfm_libs)
])
AC_MSG_CHECKING([if the perfevent PMDA should be included])
AC_SUBST(PMDA_PERFEVENT, $pmda_perfevent)
if $pmda_perfevent; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_CHECK_LIB(hdr_histogram, hdr_init, [lib_for_hdr_histogram="-lz -lhdr_histogram"])
AC_SUBST(lib_for_hdr_histogram)

AC_CHECK_LIB(chan, chan_init, [lib_for_chan="-lchan"])
AC_SUBST(lib_for_chan)

dnl Do you have ragel, hdr_histogram and chan libraries for pmdastatsd?
AC_MSG_CHECKING([if the statsd PMDA should be included])
pmda_statsd=false
AS_IF([test "x$do_pmdastatsd" = "xyes"], [pmda_statsd=true])
AS_IF([test "x$do_pmdastatsd" = "xcheck"], [
       if test "x$lib_for_hdr_histogram" != "x" -a "x$lib_for_chan" != "x"; then
	pmda_statsd=true
       fi])
AC_SUBST(PMDA_STATSD, $pmda_statsd)
if $pmda_statsd; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_MSG_CHECKING([if the podman PMDA should be included])
pmda_podman=true
AS_IF([test "x$do_pmdapodman" = "xno"], [pmda_podman=false])
AC_SUBST(PMDA_PODMAN, $pmda_podman)
if $pmda_podman; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

dnl Do you have the required qshape script for the postfix PMDA
dnl The logic here is copied from the PMDA's Install script
pmda_postfix=false
AC_CHECK_PROGS(QSHAPE, qshape, [], path=[$PATH:/usr/sbin])
AC_MSG_CHECKING([if the postfix PMDA should be included])
if test -n "$QSHAPE"
then
    pmda_postfix=true
else
    for script in /usr/share/doc/packages/postfix-doc/auxiliary/qshape/qshape.pl
    do
	if test -f "$script"
	then
	    pmda_postfix=true
	    break
	fi
    done
fi
AC_SUBST(PMDA_POSTFIX,$pmda_postfix)
if $pmda_postfix; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_CHECK_PROGS(SEINFO, seinfo)
AC_SUBST(SEINFO)
AC_MSG_CHECKING([if selinux policy should be included])
enable_selinux=false
if test $target_os = mingw
then
    :
else
    AS_IF([test "x$do_selinux" != "xno"], [
    for makefile in /usr/share/selinux/devel/Makefile
    do
	if test -f "$makefile" -a "x$SEINFO" != "x"
	then
	    enable_selinux=true
	    AC_MSG_RESULT(yes)
	fi
    done

    ])
fi
test "$enable_selinux" = "false" && AC_MSG_RESULT(no)
AC_SUBST(enable_selinux)
AC_SUBST(pcp_selinux_dir)

pcp_selinux_cap_userns_ptrace=false
pcp_selinux_container_runtime=false
pcp_selinux_nsfs=false
pcp_selinux_docker_var_lib=false
pcp_selinux_unreserved_port=false
pcp_selinux_tracefs=false
pcp_selinux_hostname_exec_map=false
pcp_selinux_unconfined=false
pcp_selinux_numad=false
pcp_selinux_bpf=false
pcp_selinux_dma_device=false
pcp_selinux_files_list_non_auth_dirs=false
pcp_selinux_container_var_run=false
pcp_selinux_rpcbind_var_run=false
pcp_selinux_virt_var_run=false
pcp_selinux_proc_security=false
pcp_selinux_sbd_exec=false
pcp_selinux_capability2_syslog=false
pcp_selinux_capability2_bpf=false
pcp_selinux_icmp_socket_class=false
pcp_selinux_glusterd_log=false

pcp_selinux_initrc_tmp=false
pcp_selinux_sudo_exec=false
pcp_selinux_security=false
pcp_selinux_chkpwd_exec=false
pcp_selinux_initrc=false
pcp_selinux_var_log=false
pcp_selinux_lastlog=false
pcp_selinux_xauth_exec=false
pcp_selinux_logrotate=false
pcp_selinux_faillog=false
pcp_selinux_kmod_exec=false
pcp_selinux_mdadm_conf=false

pcp_selinux_netlink_selinux_socket_class=false
pcp_selinux_netlink_audit_socket_class=false
pcp_selinux_sock_file_class=false
pcp_selinux_security_class=false
pcp_selinux_dir_class=false
pcp_selinux_rawip_socket_class=false
pcp_selinux_netlink_generic_socket_class=false
pcp_selinux_netlink_tcpdiag_socket_class=false

AS_IF([test "x$enable_selinux" != "xfalse"], [

    dnl new-style optional type tests ... seinfo once, then grep
    dnl note [[...]] needed to escape m4 handling of [...]
    dnl for type xyz_t we set pcp_selinux_xyz (note no trailing _t)

    seinfo -t >conftest.out

    dnl these ones are for pcpupstream.te

    grep '^[[ 	]][[ 	]]*container_runtime_t$' <conftest.out >/dev/null \
	&& pcp_selinux_container_runtime=true
    grep '^[[ 	]][[ 	]]*nsfs_t$' <conftest.out >/dev/null \
	&& pcp_selinux_nsfs=true
    grep '^[[ 	]][[ 	]]*docker_var_lib_t$' <conftest.out >/dev/null \
	&& pcp_selinux_docker_var_lib=true
    grep '^[[ 	]][[ 	]]*unreserved_port_t$' <conftest.out >/dev/null \
	&& pcp_selinux_unreserved_port=true
    grep '^[[ 	]][[ 	]]*tracefs_t$' <conftest.out >/dev/null \
	&& pcp_selinux_tracefs=true
    grep '^[[ 	]][[ 	]]*unconfined_service_t$' <conftest.out >/dev/null \
	&& pcp_selinux_unconfined=true
    grep '^[[ 	]][[ 	]]*numad_t$' <conftest.out >/dev/null \
	&& pcp_selinux_numad=true
    grep '^[[ 	]][[ 	]]*container_var_run_t$' <conftest.out >/dev/null \
	&& pcp_selinux_container_var_run=true
    grep '^[[ 	]][[ 	]]*rpcbind_var_run_t$' <conftest.out >/dev/null \
	&& pcp_selinux_rpcbind_var_run=true
    grep '^[[ 	]][[ 	]]*virt_var_run_t$' <conftest.out >/dev/null \
	&& pcp_selinux_virt_var_run=true
    grep '^[[ 	]][[ 	]]*proc_security_t$' <conftest.out >/dev/null \
	&& pcp_selinux_proc_security=true
    grep '^[[ 	]][[ 	]]*sbd_exec_t$' <conftest.out >/dev/null \
	&& pcp_selinux_sbd_exec=true
    grep '^[[ 	]][[ 	]]*kmod_exec_t$' <conftest.out >/dev/null \
	&& pcp_selinux_kmod_exec=true
    grep '^[[ 	]][[ 	]]*dma_device_t$' <conftest.out >/dev/null \
	&& pcp_selinux_dma_device=true
    grep '^[[ 	]][[ 	]]*glusterd_log_t$' <conftest.out >/dev/null \
	&& pcp_selinux_glusterd_log=true
    grep '^[[ 	]][[ 	]]*mdadm_conf_t$' <conftest.out >/dev/null \
	&& pcp_selinux_mdadm_conf=true

    dnl these ones are for pcpqa.te

    grep '^[[ 	]][[ 	]]*initrc_tmp_t$' <conftest.out >/dev/null \
	&& pcp_selinux_initrc_tmp=true
    grep '^[[ 	]][[ 	]]*sudo_exec_t$' <conftest.out >/dev/null \
	&& pcp_selinux_sudo_exec=true
    grep '^[[ 	]][[ 	]]*security_t$' <conftest.out >/dev/null \
	&& pcp_selinux_security=true
    grep '^[[ 	]][[ 	]]*chkpwd_exec_t$' <conftest.out >/dev/null \
	&& pcp_selinux_chkpwd_exec=true
    grep '^[[ 	]][[ 	]]*initrc_t$' <conftest.out >/dev/null \
	&& pcp_selinux_initrc=true
    grep '^[[ 	]][[ 	]]*var_log_t$' <conftest.out >/dev/null \
	&& pcp_selinux_var_log=true
    grep '^[[ 	]][[ 	]]*lastlog_t$' <conftest.out >/dev/null \
	&& pcp_selinux_lastlog=true
    grep '^[[ 	]][[ 	]]*xauth_exec_t$' <conftest.out >/dev/null \
	&& pcp_selinux_xauth_exec=true
    grep '^[[ 	]][[ 	]]*logrotate_t$' <conftest.out >/dev/null \
	&& pcp_selinux_logrotate=true
    grep '^[[ 	]][[ 	]]*faillog_t$' <conftest.out >/dev/null \
	&& pcp_selinux_faillog=true

    rm -f conftest.out

    seinfo --common >/dev/null 2>&1
    if test $? -eq 0
    then
        seinfo_common_flag="--common"
    else
        seinfo_common_flag=""
    fi

    dnl classes need to be done one at a time
    dnl
    dnl If we're just looking for the class xyz we only need to
    dnl look at the first line of output from seinfo -x --class=xyz,
    dnl then we set pcp_selinux_xyz or pcp_selinux_xyz_class.
    dnl But if we're looking for a particular permission within a
    dnl class we need to look further into the seinfo output and
    dnl we set a variable name that is constructed from (possibly part
    dnl of) the class name _and_ the permission name

    dnl pcpupstream.te first

    seinfo -x --class=cap_userns $seinfo_common_flag 2>/dev/null \
    | grep '^[[ 	]][[ 	]]*sys_ptrace$' >/dev/null \
    && pcp_selinux_cap_userns_ptrace=true

    dnl need to look in two places for this one?
    dnl kenj- second one looks bad because --common is not always allowed,
    dnl       which is what $seinfo_common is all about
    seinfo -x --class=file $seinfo_common_flag 2>/dev/null \
    | grep '^[[ 	]][[ 	]]*map$' >/dev/null \
    && pcp_selinux_hostname_exec_map=true
    seinfo -x --common file 2>/dev/null \
    | grep '^[[ 	]][[ 	]]*map$' >/dev/null \
    && pcp_selinux_hostname_exec_map=true

    seinfo -x --class=bpf $seinfo_common_flag 2>/dev/null \
    | egrep '^[[ 	]][[ 	]]*(class |)bpf$' >/dev/null \
    && pcp_selinux_bpf=true

    seinfo -a 2>/dev/null \
    | grep '^[[ 	]][[ 	]]*non_auth_file_type$' >/dev/null \
    && pcp_selinux_files_list_non_auth_dirs=true

    seinfo -x --class=capability2 $seinfo_common_flag 2>/dev/null \
    | grep '^[[ 	]][[ 	]]*syslog$' >/dev/null \
    && pcp_selinux_capability2_syslog=true

    seinfo -x --class=capability2 $seinfo_common_flag 2>/dev/null \
    | grep '^[[ 	]][[ 	]]*bpf$' >/dev/null \
    && pcp_selinux_capability2_bpf=true

    seinfo -x --class=icmp_socket $seinfo_common_flag 2>/dev/null \
    | egrep '^[[ 	]][[ 	]]*(class |)icmp_socket$' >/dev/null \
    && pcp_selinux_icmp_socket_class=true

    seinfo -x --class=lockdown $seinfo_common_flag 2>/dev/null \
    | egrep '^[[ 	]][[ 	]]*(class |)lockdown$' >/dev/null \
    && pcp_selinux_lockdown_class=true

    dnl these ones are for pcpqa.te

    seinfo -x --class=netlink_selinux_socket $seinfo_common_flag 2>/dev/null \
    | egrep '^[[ 	]][[ 	]]*(class |)netlink_selinux_socket$' >/dev/null \
    && pcp_selinux_netlink_selinux_socket_class=true

    seinfo -x --class=netlink_audit_socket $seinfo_common_flag 2>/dev/null \
    | egrep '^[[ 	]][[ 	]]*(class |)netlink_audit_socket$' >/dev/null \
    && pcp_selinux_netlink_audit_socket_class=true

    seinfo -x --class=sock_file $seinfo_common_flag 2>/dev/null \
    | egrep '^[[ 	]][[ 	]]*(class |)sock_file$' >/dev/null \
    && pcp_selinux_sock_file_class=true

    seinfo -x --class=security $seinfo_common_flag 2>/dev/null \
    | egrep '^[[ 	]][[ 	]]*(class |)security$' >/dev/null \
    && pcp_selinux_security_class=true

    dnl pcp_selinux_dir already used for something else, so name to
    dnl set is pcp_selinux_dir_class
    seinfo -x --class=dir $seinfo_common_flag 2>/dev/null \
    | egrep '^[[ 	]][[ 	]]*(class |)dir$' >/dev/null \
    && pcp_selinux_dir_class=true

    seinfo -x --class=rawip_socket $seinfo_common_flag 2>/dev/null \
    | egrep '^[[ 	]][[ 	]]*(class |)rawip_socket$' >/dev/null \
    && pcp_selinux_rawip_socket_class=true

    seinfo -x --class=netlink_generic_socket $seinfo_common_flag 2>/dev/null \
    | egrep '^[[ 	]][[ 	]]*(class |)netlink_generic_socket$' >/dev/null \
    && pcp_selinux_netlink_generic_socket_class=true

    seinfo -x --class=netlink_tcpdiag_socket $seinfo_common_flag 2>/dev/null \
    | egrep '^[[ 	]][[ 	]]*(class |)netlink_tcpdiag_socket$' >/dev/null \
    && pcp_selinux_netlink_tcpdiag_socket_class=true

])

dnl for pcpupstream.te
AC_SUBST(pcp_selinux_cap_userns_ptrace)
AC_SUBST(pcp_selinux_container_runtime)
AC_SUBST(pcp_selinux_nsfs)
AC_SUBST(pcp_selinux_docker_var_lib)
AC_SUBST(pcp_selinux_unreserved_port)
AC_SUBST(pcp_selinux_tracefs)
AC_SUBST(pcp_selinux_hostname_exec_map)
AC_SUBST(pcp_selinux_unconfined)
AC_SUBST(pcp_selinux_numad)
AC_SUBST(pcp_selinux_bpf)
AC_SUBST(pcp_selinux_dma_device)
AC_SUBST(pcp_selinux_files_list_non_auth_dirs)
AC_SUBST(pcp_selinux_container_var_run)
AC_SUBST(pcp_selinux_rpcbind_var_run)
AC_SUBST(pcp_selinux_virt_var_run)
AC_SUBST(pcp_selinux_proc_security)
AC_SUBST(pcp_selinux_sbd_exec)
AC_SUBST(pcp_selinux_capability2_syslog)
AC_SUBST(pcp_selinux_capability2_bpf)
AC_SUBST(pcp_selinux_icmp_socket_class)
AC_SUBST(pcp_selinux_lockdown_class)
AC_SUBST(pcp_selinux_glusterd_log)

dnl for pcpqa.te
AC_SUBST(pcp_selinux_initrc_tmp)
AC_SUBST(pcp_selinux_sudo_exec)
AC_SUBST(pcp_selinux_security)
AC_SUBST(pcp_selinux_chkpwd_exec)
AC_SUBST(pcp_selinux_initrc)
AC_SUBST(pcp_selinux_var_log)
AC_SUBST(pcp_selinux_lastlog)
AC_SUBST(pcp_selinux_xauth_exec)
AC_SUBST(pcp_selinux_logrotate)
AC_SUBST(pcp_selinux_faillog)
AC_SUBST(pcp_selinux_kmod_exec)
AC_SUBST(pcp_selinux_mdadm_conf)

AC_SUBST(pcp_selinux_netlink_selinux_socket_class)
AC_SUBST(pcp_selinux_netlink_audit_socket_class)
AC_SUBST(pcp_selinux_sock_file_class)
AC_SUBST(pcp_selinux_security_class)
AC_SUBST(pcp_selinux_dir_class)
AC_SUBST(pcp_selinux_rawip_socket_class)
AC_SUBST(pcp_selinux_netlink_generic_socket_class)
AC_SUBST(pcp_selinux_netlink_tcpdiag_socket_class)

dnl more work to do, these selinux macro are not always supported
pcp_selinux_files_mmap_all_files=false
pcp_selinux_logging_watch_all_log_dirs_path=false
if test "x$enable_selinux" != "xfalse"
then
    AC_MSG_CHECKING([for selinux files_mmap_all_files() macro])
    cat <<End-of-File >conftest.te
module conftest 1.0;
require {
	attribute file_type;
	attribute pcp_domain;
	class file { append create execute open read write unlink map };
}
files_mmap_all_files(pcp_domain);
End-of-File
    if make -f /usr/share/selinux/devel/Makefile conftest.pp >/dev/null 2>&1
    then
	pcp_selinux_files_mmap_all_files=true
	AC_MSG_RESULT(yes)
    else
	AC_MSG_RESULT(no)
    fi
    dnl cleanup turds from selinux Makefile
    rm -f tmp/all_interfaces.conf tmp/conftest.* tmp/iferror.m4

    AC_MSG_CHECKING([for selinux logging_watch_all_log_dirs_path() macro])
    cat <<End-of-File >conftest.te
module conftest 1.0;
require {
	attribute logfile;
	attribute pcp_domain;
	class dir { getattr search open watch };
}
logging_watch_all_log_dirs_path(pcp_domain);
End-of-File
    if make -f /usr/share/selinux/devel/Makefile conftest.pp >/dev/null 2>&1
    then
	pcp_selinux_logging_watch_all_log_dirs_path=true
	AC_MSG_RESULT(yes)
    else
	AC_MSG_RESULT(no)
    fi
    dnl cleanup turds from selinux Makefile
    rm -f tmp/all_interfaces.conf tmp/conftest.* tmp/iferror.m4
fi
AC_SUBST(pcp_selinux_files_mmap_all_files)
AC_SUBST(pcp_selinux_logging_watch_all_log_dirs_path)

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_FUNC_WAIT3
AC_FUNC_VPRINTF
AC_CHECK_FUNCS(mktime nanosleep usleep unsetenv)
AC_CHECK_FUNCS(select socket syslog sendmsg recvmsg setns)
AC_CHECK_FUNCS(getuid getgid getpeerucred getpeereid)
AC_CHECK_FUNCS(uname gethostname getdomainname getmachineid)
AC_CHECK_FUNCS(__clone pipe2 fcntl ioctl)
AC_CHECK_FUNCS(prctl setlinebuf waitpid atexit kill)
AC_CHECK_FUNCS(chown fchmod getcwd scandir mkstemp)
AC_CHECK_FUNCS(brk sbrk posix_memalign memalign valloc)
AC_CHECK_FUNCS(signal sighold sigrelse tcgetattr)
AC_CHECK_FUNCS(regex regcmp regexec regcomp)
AC_CHECK_FUNCS(strtod strtol strtoll strtoull strndup strchrnul)
AC_CHECK_FUNCS(getgrent getgrent_r getgrnam getgrnam_r getgrgid getgrgid_r)
AC_CHECK_FUNCS(getpwent getpwent_r getpwnam getpwnam_r getpwuid getpwuid_r)
AC_CHECK_FUNCS(sysinfo trace_back_stack)

dnl checking for backtrace() needs a little more care ..
dnl check if backtrace functions come from libexeinfo (OpenBSD 7.0)
AC_CHECK_LIB(execinfo, backtrace)
lib_for_backtrace=""
if test $ac_cv_lib_execinfo_backtrace = yes
then
    lib_for_backtrace="-lexecinfo"
fi
AC_SUBST(lib_for_backtrace)
dnl now we can check for backtrace()
savedLIBS=$LIBS
LIBS="$LIBS $lib_for_backtrace"
AC_CHECK_FUNCS(backtrace)
LIBS=$savedLIBS

dnl symbols needed for backtracing
if test "x$cc_is_gcc" = xyes 
then
    dnl if using gcc, some platforms make __executable_start go away when
    dnl compiled -fPIE -pie, e.g. CentOS 6, RHEL/CentOS 7 ... so need to
    dnl check for that here before declaring __executable_start is available
    savedCFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS -fPIE -pie"
fi
AC_MSG_CHECKING([if __executable_start defined by ld])
AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
extern char __executable_start;
int main()
{
    printf("%p\n", &__executable_start);
    return 0;
}
])],
[
AC_DEFINE(HAVE___EXECUTABLE_START, [1], [__executable_start defined])
AC_MSG_RESULT(yes)
], AC_MSG_RESULT(no))
AC_MSG_CHECKING([if __etext defined by ld])
AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
extern char __etext;
int main()
{
    printf("%p\n", &__etext);
    return 0;
}
])],
[
AC_DEFINE(HAVE___ETEXT, [1], [__etext defined])
AC_MSG_RESULT(yes)
], AC_MSG_RESULT(no))
AC_MSG_CHECKING([if _etext defined by ld])
AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
extern char _etext;
int main()
{
    printf("%p\n", &_etext);
    return 0;
}
])],
[
AC_DEFINE(HAVE__ETEXT, [1], [_etext defined])
AC_MSG_RESULT(yes)
], AC_MSG_RESULT(no))
AC_MSG_CHECKING([if etext defined by ld])
AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <stdio.h>
extern char etext;
int main()
{
    printf("%p\n", &etext);
    return 0;
}
])],
[
AC_DEFINE(HAVE_ETEXT, [1], [etext defined])
AC_MSG_RESULT(yes)
], AC_MSG_RESULT(no))
if test "x$cc_is_gcc" = xyes 
then
    dnl see above
    CFLAGS="$savedCFLAGS"
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
    if test $target_os = darwin
    then
	# Need to strip -arch foo args for gcc -E
	_myflags=`echo "$CPPFLAGS" "$CFLAGS" | sed -e 's/-arch [[^ ]]*//g'`
    else
	_myflags="$CPPFLAGS $CFLAGS"
    fi
    _raw=`$CPP $_myflags conftest.c \
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
    	_fmt='d'
	;;
    unsigned|'unsigned int')
    	_fmt='u'
	;;
    long|'long int')
	_fmt='ld'
	;;
    'long long'|'long long int')
        if test $target_os = mingw; then
            _fmt='I64d'
        else
	    _fmt='lld'
        fi
	;;
    'unsigned long'|'unsigned long int'|'long unsigned int')
	_fmt='lu'
	;;
    'unsigned long long'|'unsigned long long int')
        if test $target_os = mingw; then
            _fmt='I64u'
        else
	    _fmt='llu'
        fi
	;;
    *\*)	# pointer to a something
	_fmt='p'
	;;
    struct\ *)	# not much can be done here ...
	_fmt='p'
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
if test "$cross_compiling" = "yes"; then
    ans=$printf_fmt_pid
    echo "cross-compile -> \"$ans\"" >&5
    fmt_pid=$ans
else
    cat <<End-of-File >conftest.c
#include <sys/types.h>
#include <unistd.h>
#if defined pid_t	/* for NetBSD 8.0 */
#undef pid_t
#endif
pid_t bozo;
End-of-File
    _do_type
    fmt_pid=$_fmt
fi
AC_MSG_RESULT($fmt_pid)
AC_SUBST(fmt_pid)
rm -rf conftest.c conftest.debug

dnl printf type for int64_t
AC_MSG_CHECKING([printf type for int64_t ])
if test "$cross_compiling" = "yes"; then
    ans=$printf_fmt_int64
    echo "cross-compile -> \"$ans\"" >&5
else
    dnl first approach ... is there a #define that we can leverage?
    cat <<End-of-File >conftest.c
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
int main()
{
#if defined(__PRI64_PREFIX)
    /* most of the civilized universe */
    printf("%s\n", __PRI64_PREFIX "d");
#elif defined(__PRI_64_LENGTH_MODIFIER__)
    /* Darwin */
    printf("%s\n", __PRI_64_LENGTH_MODIFIER__ "d");
#else
    bozo!	/* fall through to the second approach below */
#endif
    return(0);
}
End-of-File
    ans=''
    (eval $ac_link) 2>&5
    if test -x ./conftest
    then
	ans=`./conftest`
	echo "./conftest (first approach) -> \"$ans\"" >&5
    fi
    if test -z "$ans"
    then
	dnl second approach _WORDSIZE may help ...
	cat <<End-of-File >conftest.c
#include <stdio.h>
#include <stdint.h>
#include <string.h>
int main()
{
#ifdef __WORDSIZE
/* logic copied from <inttypes.h> */
#if __WORDSIZE == 64
    printf("ld\n");
#else
    printf("lld\n");
#endif
#else
/*
 * on NetBSD and OpenBSD, for example, there is no __WORDSIZE
 * so fall through to the third approach below ...
 */
#endif
    return(0);
}
End-of-File
	(eval $ac_link) 2>&5
	if test -x ./conftest
	then
	    ans=`./conftest`
	    if test $target_os = mingw -a "$ans" = "lld"; then
		ans='I64d'
	    fi
	    echo "./conftest (second approach) -> \"$ans\"" >&5
	fi
    fi
    if test -z "$ans" -a "$GCC" = "yes"
    then
	dnl third approach, for gcc look for a printf format that
	dnl does not produce compiler warnings
	rm -f conftest.ans
	for fmt in lld ld d
	do
	    cat <<End-of-File >conftest.$ac_ext
#include <stdio.h>
#include <sys/types.h>
int main(void)
{
    int64_t	x = -1;
    printf("%$fmt\n", x);
    return 0;
}
End-of-File
	    $CC -Wall $CFLAGS $CPPFLAGS -c conftest.$ac_ext 2>conftest.err
	    if test -s conftest.err >/dev/null
	    then
		echo "-- not $fmt for fmt_64" >&5
		cat conftest.err >&5
	    else
		echo "$fmt" >conftest.ans
		echo "no warnings (third approach) -> \"$fmt\"" >&5
		break
	    fi
	done
	test -s conftest.ans && ans=`cat conftest.ans`
	rm -f conftest.ans conftest.err
    fi
fi
if test -z "$ans"
then
    echo 'FATAL ERROR: Cannot determine printf format for int64_t type'
    rm -rf conftest conftest.*
    exit 1
fi
fmt_int64=$ans
AC_MSG_RESULT($fmt_int64)
AC_SUBST(fmt_int64)
dnl and the unsigned version of this ...
fmt_uint64=`echo $fmt_int64 | sed -e 's/d$/u/'`
AC_SUBST(fmt_uint64)
rm -rf conftest.* conftest

AC_MSG_CHECKING([strtol or strtoll for int64_t])
if test "$cross_compiling" = "yes"; then
    ans="$strtoint64"
    echo "cross-compile -> \"$ans\"" >&5
else
    cat <<End-of-File >conftest.c
#include <stdio.h>
#include <stdlib.h>
main()
{
    long long int	value = -1;
    value = strtol("1234567812345678", NULL, 16);
    if (value == 0x1234567812345678LL) {
	printf("strotol\n");
	exit(0);
    }
    value = strtoll("1234567812345678", NULL, 16);
    if (value == 0x1234567812345678LL) {
	printf("strtoll\n");
	exit(0);
    }
    exit(1);
}
End-of-File
    (eval $ac_compile) 2>&5
    (eval $ac_link) 2>&5
    ans=`./conftest`
    echo "./conftest -> \"$ans\"" >&5
fi
case "$ans"
in
    strtol)
	AC_DEFINE(strtoint64, [strtol], [Define strtoint64 as strtol])
	AC_DEFINE(strtouint64, [strtoul], [Define strtoint64 as strtoul])
	AC_MSG_RESULT([strtol])
	;;
    strtoll)
	AC_DEFINE(strtoint64, [strtoll], [Define strtoint64 as strtoll])
	AC_DEFINE(strtouint64, [strtoull], [Define strtoint64 as strtoull])
	AC_MSG_RESULT([strtoll])
	;;
    *)
	AC_DEFINE(strtoint64, [strtoll], [Default strtoint64 to strtoll])
	AC_DEFINE(strtouint64, [strtoull], [Default strtouint64 to strtoull])
	AC_MSG_RESULT([assuming strtoll])
	;;
esac
rm -rf conftest conftest.*

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
	fmt_pthread=$_fmt
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

AC_MSG_CHECKING([sizeof suseconds_t])
if test "$cross_compiling" = "yes"; then
    ans=$sizeof_suseconds_t
    echo "cross-compile -> \"$ans\"" >&5
else
    cat <<End-of-File >conftest.c
#include <sys/time.h>
main() { printf("%d", (int)sizeof(suseconds_t)); }
End-of-File
    (eval $ac_compile) 2>&5
    (eval $ac_link) 2>&5
    ans=`./conftest`
    echo "./conftest -> \"$ans\"" >&5
    rm -rf conftest conftest.*
fi
AC_MSG_RESULT($ans)
if test -n "$ans"; then
    AC_DEFINE_UNQUOTED(PM_SIZEOF_SUSECONDS_T, $ans, [sizeof suseconds_t])
fi

AC_MSG_CHECKING([sizeof time_t])
if test "$cross_compiling" = "yes"; then
    ans=$sizeof_time_t
    echo "cross-compile -> \"$ans\"" >&5
else
    cat <<End-of-File >conftest.c
#include <sys/time.h>
main() { printf("%d", (int)sizeof(time_t)); }
End-of-File
    (eval $ac_compile) 2>&5
    (eval $ac_link) 2>&5
    ans=`./conftest`
    echo "./conftest -> \"$ans\"" >&5
    rm -rf conftest conftest.*
fi
AC_MSG_RESULT($ans)
if test -n "$ans"; then
    AC_DEFINE_UNQUOTED(PM_SIZEOF_TIME_T, $ans, [sizeof time_t])
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
pcp_pmproxyoptions_path=$pcp_sysconf_dir/pmproxy/pmproxy.options
pcp_pmiecontrol_path=$pcp_sysconf_dir/pmie/control
pcp_pmsnapcontrol_path=$pcp_sysconf_dir/pmsnap/control
pcp_pmloggercontrol_path=$pcp_sysconf_dir/pmlogger/control
AC_SUBST(pcp_pmcdconf_path)
AC_SUBST(pcp_pmcdoptions_path)
AC_SUBST(pcp_pmcdrclocal_path)
AC_SUBST(pcp_pmproxyoptions_path)
AC_SUBST(pcp_pmiecontrol_path)
AC_SUBST(pcp_pmsnapcontrol_path)
AC_SUBST(pcp_pmloggercontrol_path)

dnl shared PCP files (shareable for diskless)
pcp_share_dir=`eval echo $datarootdir/pcp`
pcp_share_dir=`eval echo $pcp_share_dir`
AC_SUBST(pcp_share_dir)

dnl shared bash command completions
pcp_bashshare_dir=`eval echo $datarootdir/bash-completion`
pcp_bashshare_dir=`eval echo $pcp_bashshare_dir`
AC_SUBST(pcp_bashshare_dir)

dnl private PCP executables
pcp_binadm_dir=`eval echo $libexecdir/pcp/bin`
pcp_binadm_dir=`eval echo $pcp_binadm_dir`
AC_SUBST(pcp_binadm_dir)

dnl private PCP libraries
pcp_libadm_dir=`eval echo $libexecdir/pcp/lib`
pcp_libadm_dir=`eval echo $pcp_libadm_dir`
AC_SUBST(pcp_libadm_dir)

dnl non-shared (i.e. system local) PCP files
pcp_var_dir=`eval echo $localstatedir/lib/pcp`
pcp_var_dir=`eval echo $pcp_var_dir`
AC_SUBST(pcp_var_dir)

pcp_pmns_dir=$pcp_var_dir/pmns
AC_SUBST(pcp_pmns_dir)
pcp_pmnsadm_dir=`eval echo $libexecdir/pcp/pmns`
pcp_pmnsadm_dir=`eval echo $pcp_pmnsadm_dir`
AC_SUBST(pcp_pmnsadm_dir)

pcp_pmdas_dir=$pcp_var_dir/pmdas
AC_SUBST(pcp_pmdas_dir)
pcp_pmdasadm_dir=`eval echo $libexecdir/pcp/pmdas`
pcp_pmdasadm_dir=`eval echo $pcp_pmdasadm_dir`
AC_SUBST(pcp_pmdasadm_dir)

pcp_selinux_dir=$pcp_var_dir/selinux
AC_SUBST(pcp_selinux_dir)
pcp_selinuxadm_dir=`eval echo $libexecdir/pcp/selinux`
pcp_selinuxadm_dir=`eval echo $pcp_selinuxadm_dir`
AC_SUBST(pcp_selinuxadm_dir)

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
AC_MSG_CHECKING([if man pages should packaged])
have_manpages=false
have_gzipped_manpages=false
have_bzip2ed_manpages=false
have_lzmaed_manpages=false
have_xzed_manpages=false
need_old_tbl_header=false
man_header=
pcp_man_dir=`eval echo $mandir`
pcp_man_dir=`eval echo $pcp_man_dir`

dnl guess compression in use ... if we can find at least one file
dnl with the designated extension in section 1 of the man pages then
dnl we're good to go
for d in /usr/man /usr/share/man $pcp_man_dir
do
    for sd in man1 sman1
    do
	ls $d/$sd 2>/dev/null >conftest
	test -s conftest || continue
	try=`grep '\.1\.gz$' conftest | head -1`
	if test -n "$try"
	then
	    have_gzipped_manpages=true
	    have_manpages=true
	    man_header=`$ZIP -d < "$d/$sd/$try" | head -1`
	    break
	fi
	try=`grep '\.1\.bz2$' conftest | head -1`
	if test -n "$try"
	then
	    have_bzip2ed_manpages=true
	    have_manpages=true
	    man_header=`$BZIP2 -d < "$d/$sd/$try" | head -1`
	    break
	fi
	try=`grep '\.1\.lzma$' conftest | head -1`
	if test -n "$try"
	then
	    have_lzmaed_manpages=true
	    have_manpages=true
	    man_header=`$LZMA -d < "$d/$sd/$try" | head -1`
	    break
	fi
	try=`grep '\.1\.xz$' conftest | head -1`
	if test -n "$try"
	then
	    have_xzed_manpages=true
	    have_manpages=true
	    man_header=`$XZ -d < "$d/$sd/$try" | head -1`
	    break
	fi
	try=`grep '\.1$' conftest | head -1`
	if test -n "$try"
	then
	    man_header=`head -1 $d/$sd/$try`
	    have_manpages=true
	    break
	fi
    done
    $have_manpages && break
done
rm -f conftest
if test x"$man_header" = "x'\\\" t" -o x"$man_header" = "x'\\\" te" ; then
    need_old_tbl_header=true
fi

if $have_manpages
then
    :
else
    dnl We can get here ($have_manpages == false) for container-based
    dnl builds.
    dnl For Debian packaging, we _must_ include the man pages, so 
    dnl take the known and good settings ...
    dnl
    if test $target_distro = debian
    then
	have_manpages=true
	have_gzipped_manpages=true
	need_old_tbl_header=true
    fi
fi

if $have_manpages; then AC_MSG_RESULT(yes); else AC_MSG_RESULT(no); fi

AC_SUBST(pcp_man_dir)
AC_SUBST(have_manpages)
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

dnl html files (GUI online help, tutorials)
if test $target_os = linux; then
    pcp_html_dir=`eval echo $datarootdir/doc/pcp-doc/html`
else
    pcp_html_dir=`eval echo $datarootdir/doc/pcp/html`
fi
pcp_html_dir=`eval echo $pcp_html_dir`
AC_SUBST(pcp_html_dir)

dnl icon pixmap files
if test $target_os = linux; then
    pcp_icons_dir=`eval echo $datarootdir/pcp-gui/pixmaps`
else
    pcp_icons_dir=`eval echo $datarootdir/pcp/pixmaps`
fi
pcp_icons_dir=`eval echo $pcp_icons_dir`
AC_SUBST(pcp_icons_dir)

test -z "$INKSCAPE" && AC_PATH_PROG(INKSCAPE, inkscape)
inkscape=$INKSCAPE
AC_SUBST(inkscape)

have_hicolor_icons=false
if test -d /usr/share/icons/hicolor -a -n "$INKSCAPE"
then
    AC_MSG_NOTICE([adding hicolor desktop icons to the build])
    have_hicolor_icons=true
fi
AC_SUBST(have_hicolor_icons)

dnl desktop application entries
pcp_desktop_dir=`eval echo $datarootdir/applications`
pcp_desktop_dir=`eval echo $pcp_desktop_dir`
AC_SUBST(pcp_desktop_dir)

dnl rc/startup files
AC_ARG_WITH(rcdir,[AC_HELP_STRING([--with-rcdir],[rc directory [SYSCONFDIR/rc.d]])],
                  [pcp_rc_dir=$withval], [
if $enable_systemd
then
    pcp_rc_dir="$pcp_libadm_dir"
elif test $target_os = freebsd
then
    pcp_rc_dir="/usr/local/etc/rc.d"
elif test $target_os = netbsd -o $target_os = openbsd
then
    pcp_rc_dir="/etc/rc.d"
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

pcp_archive_dir=$pcp_log_dir/pmlogger
AC_SUBST(pcp_archive_dir)

pcp_sa_dir=$pcp_log_dir/sa
AC_SUBST(pcp_sa_dir)

pcp_pacct_system_path="$localstatedir/account/pacct"
AC_SUBST(pcp_pacct_system_path)

AC_ARG_WITH(rundir,[AC_HELP_STRING([--with-rundir],[run directory [LOCALSTATEDIR/run/pcp]])],
                  [pcp_run_dir=$withval], [
if test -d /run
then
    pcp_run_dir="/run/pcp"
else
    pcp_run_dir="$localstatedir/run/pcp"
fi])
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

dnl check for zfs_iter_snapshots API style from <libzfs.h>
if test "$ac_cv_header_libzfs_h" = "yes"
then
    AC_MSG_CHECKING([for 4-argument style zfs_iter_snapshots])
    AC_TRY_COMPILE([#include <libzfs.h>],
	[ zfs_handle_t *zh;
	  zfs_iter_f callback;
	  (void)zfs_iter_snapshots(zh, B_FALSE, callback, NULL);
	],
	AC_DEFINE(HAVE_ZFS_ITER_SNAPSHOTS_4ARG, [1], [4-arg zfs_iter_snapshots])
	AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

    AC_MSG_CHECKING([for 3-argument style zfs_iter_snapshots])
    AC_TRY_COMPILE([#include <libzfs.h>],
	[ zfs_handle_t *zh;
	  zfs_iter_f callback;
	  (void)zfs_iter_snapshots(zh, callback, NULL);
	],
	AC_DEFINE(HAVE_ZFS_ITER_SNAPSHOTS_3ARG, [1], [3-arg zfs_iter_snapshots])
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

dnl Check for decompression libraries
enable_lzma=false
enable_decompression=false
AS_IF([test "x$do_decompression" != "xno"], [
    # Check for -llzma
    enable_lzma=true
    PKG_CHECK_MODULES([lzma], [liblzma],
        [AC_CHECK_LIB(lzma, lzma_index_stream_flags,
		      [lib_for_lzma="-llzma"],
		      [enable_lzma=false])
        ],[enable_lzma=false])

    AC_CHECK_HEADERS([lzma.h], [], [enable_lzma=false])

    if test "$enable_lzma" = "true"
    then
        AC_SUBST(lib_for_lzma)
	AC_SUBST(lzma_CFLAGS)
	AC_DEFINE(HAVE_LZMA_DECOMPRESSION, [1], [lzma decompression])
	enable_decompression=true
    fi

    if test "$do_decompression" != "check" -a "$enable_decompression" != "true"
    then
	AC_MSG_ERROR([cannot enable transparent decompression - no supported compression formats])
    fi

    if test "$enable_decompression" = "true"
    then
	AC_DEFINE(HAVE_TRANSPARENT_DECOMPRESSION, [1], [Transparent decompression])
    fi
])
AC_SUBST(enable_decompression)
AC_SUBST(enable_lzma)

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

dnl Look for zlib
PKG_CHECK_MODULES([zlib], [zlib >= 1.0.0], [have_zlib=true], [have_zlib=false])
AC_SUBST(HAVE_ZLIB, [$have_zlib])

dnl Look for cmocka
PKG_CHECK_MODULES([cmocka], [cmocka], [have_cmocka=true], [have_cmocka=false])
AC_SUBST(HAVE_CMOCKA, [$have_cmocka])

dnl Check if we have AI_ADDRCONFIG
AC_MSG_CHECKING([for AI_ADDRCONFIG])
AC_TRY_COMPILE(
[
    #include <netdb.h>
    int test = AI_ADDRCONFIG;
],
[
], AC_DEFINE(HAVE_AI_ADDRCONFIG, [1], [AI_ADDRCONFIG macro]) AC_MSG_RESULT(yes), AC_MSG_RESULT(no))

dnl Capture special options passed to configure
pcp_prefix=`eval echo $prefix`
AC_SUBST(pcp_prefix)
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
