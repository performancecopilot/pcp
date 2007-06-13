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
