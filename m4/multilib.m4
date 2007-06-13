# The AC_MULTILIB macro was extracted and modified from 
# gettext-0.15's AC_LIB_PREPARE_MULTILIB macro in the lib-prefix.m4 file
# so that the correct paths can be used for 64-bit libraries.
#
dnl Copyright (C) 2001-2005 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl From Bruno Haible.

dnl AC_MULTILIB creates a variable libdirsuffix, containing
dnl the suffix of the libdir, either "" or "64".
dnl Only do this if the given enable parameter is "yes".
AC_DEFUN([AC_MULTILIB],
[
  dnl There is no formal standard regarding lib and lib64. The current
  dnl practice is that on a system supporting 32-bit and 64-bit instruction
  dnl sets or ABIs, 64-bit libraries go under $prefix/lib64 and 32-bit
  dnl libraries go under $prefix/lib. We determine the compiler's default
  dnl mode by looking at the compiler's library search path. If at least
  dnl of its elements ends in /lib64 or points to a directory whose absolute
  dnl pathname ends in /lib64, we assume a 64-bit ABI. Otherwise we use the
  dnl default, namely "lib".
  enable_lib64="$1"
  libdirsuffix=""
  searchpath=`(LC_ALL=C $CC -print-search-dirs) 2>/dev/null | sed -n -e 's,^libraries: ,,p' | sed -e 's,^=,,'`
  if test "$enable_lib64" = "yes" -a -n "$searchpath"; then
    save_IFS="${IFS= 	}"; IFS=":"
    for searchdir in $searchpath; do
      if test -d "$searchdir"; then
        case "$searchdir" in
          */lib64/ | */lib64 ) libdirsuffix=64 ;;
          *) searchdir=`cd "$searchdir" && pwd`
             case "$searchdir" in
               */lib64 ) libdirsuffix=64 ;;
             esac ;;
        esac
      fi
    done
    IFS="$save_IFS"
  fi
  AC_SUBST(libdirsuffix)
])
