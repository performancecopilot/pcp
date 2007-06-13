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
