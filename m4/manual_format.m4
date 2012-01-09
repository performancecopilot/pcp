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
