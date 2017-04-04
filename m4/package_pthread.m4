AC_DEFUN([AC_PACKAGE_NEED_PTHREAD_H],
  [ AC_CHECK_HEADERS(pthread.h)
    if test $ac_cv_header_pthread_h = no; then
	AC_CHECK_HEADERS(pthread.h,, [
	echo
	echo 'FATAL ERROR: could not find a valid pthread header.'
	exit 1])
    fi
  ])

AC_DEFUN([AC_PACKAGE_NEED_PTHREADMUTEXINIT],
  [ AC_CHECK_LIB(pthread, pthread_mutex_init,, [
	echo
	echo 'FATAL ERROR: could not find a valid pthread library.'
	exit 1
    ])
    libpthread=-lpthread
    AC_SUBST(libpthread)
  ])
