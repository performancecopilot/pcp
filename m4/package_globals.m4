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
  ])
