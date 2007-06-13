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
#  CC MAKE LIBTOOL TAR ZIP MAKEDEPEND AWK SED ECHO SORT
#  MSGFMT MSGMERGE XGETTEXT RPM
#
AC_DEFUN([AC_PACKAGE_UTILITIES],
  [ AC_PROG_CC
    cc="$CC"
    AC_SUBST(cc)
    AC_PACKAGE_NEED_UTILITY($1, "$cc", cc, [C compiler])

    if test -z "$MAKE"; then
        AC_PATH_PROG(MAKE, gmake,, /usr/bin:/usr/local/bin:/usr/freeware/bin)
    fi
    if test -z "$MAKE"; then
        AC_PATH_PROG(MAKE, make,, /usr/bin)
    fi
    make=$MAKE
    AC_SUBST(make)
    AC_PACKAGE_NEED_UTILITY($1, "$make", make, [GNU make])

    if test -z "$LIBTOOL"; then
	AC_PATH_PROG(LIBTOOL, glibtool,, /usr/bin)
    fi
    if test -z "$LIBTOOL"; then
	AC_PATH_PROG(LIBTOOL, libtool,, /usr/bin:/usr/local/bin:/usr/freeware/bin)
    fi
    libtool=$LIBTOOL
    AC_SUBST(libtool)
    AC_PACKAGE_NEED_UTILITY($1, "$libtool", libtool, [GNU libtool])

    if test -z "$TAR"; then
        AC_PATH_PROG(TAR, tar,, /usr/freeware/bin:/bin:/usr/local/bin:/usr/bin)
    fi
    tar=$TAR
    AC_SUBST(tar)
    if test -z "$ZIP"; then
        AC_PATH_PROG(ZIP, gzip,, /bin:/usr/bin:/usr/local/bin:/usr/freeware/bin)
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

    if test "$enable_gettext" = yes; then
        if test -z "$MSGFMT"; then
                AC_PATH_PROG(MSGFMT, msgfmt,, /usr/bin:/usr/local/bin:/usr/freeware/bin)
        fi
        msgfmt=$MSGFMT
        AC_SUBST(msgfmt)
        AC_PACKAGE_NEED_UTILITY($1, "$msgfmt", msgfmt, gettext)

        if test -z "$MSGMERGE"; then
                AC_PATH_PROG(MSGMERGE, msgmerge,, /usr/bin:/usr/local/bin:/usr/freeware/bin)
        fi
        msgmerge=$MSGMERGE
        AC_SUBST(msgmerge)
        AC_PACKAGE_NEED_UTILITY($1, "$msgmerge", msgmerge, gettext)

        if test -z "$XGETTEXT"; then
                AC_PATH_PROG(XGETTEXT, xgettext,, /usr/bin:/usr/local/bin:/usr/freeware/bin)
        fi
        xgettext=$XGETTEXT
        AC_SUBST(xgettext)
        AC_PACKAGE_NEED_UTILITY($1, "$xgettext", xgettext, gettext)
    fi

    if test -z "$RPM"; then
        AC_PATH_PROG(RPM, rpm,, /bin:/usr/bin:/usr/freeware/bin)
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
  ])
