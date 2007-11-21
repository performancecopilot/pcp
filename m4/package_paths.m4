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
    pkg_icon_dir=`eval echo "${datadir}"/icons/${pkg_name}`
    pkg_icon_dir=`eval echo "${pkg_icon_dir}"`
    AC_SUBST(pkg_icon_dir)
  ])
