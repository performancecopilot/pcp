AC_DEFUN([CHECK_PLIBC],
[
    # On windows machines, check if PlibC is available. First try without -plibc
    AC_TRY_LINK(
    [
        #include <plibc.h>
    ],[
        plibc_init("", "");
    ],[
        AC_MSG_RESULT(yes)
        PLIBC_CPPFLAGS=
        PLIBC_LDFLAGS=
        PLIBC_LIBS=
    ],[

        # now with -plibc
        AC_CHECK_LIB(plibc,plibc_init,
        [
            PLIBC_CPPFLAGS=
            PLIBC_LDFLAGS=
            PLIBC_LIBS=-lplibc
        ],[
            AC_MSG_CHECKING(if PlibC is installed)
            save_CPPFLAGS="$CPPFLAGS"
            CPPFLAGS="$CPPFLAGS -lplibc"
            AC_TRY_LINK(
            [
                #include <plibc.h>
            ],[
                plibc_init("", "");
            ],[
                AC_MSG_RESULT(yes)
                PLIBC_CPPFLAGS=-lplibc
                PLIBC_LDFLAGS=-lplibc
                PLIBC_LIBS=
            ],[
                AC_MSG_ERROR([PlibC is not available on your windows machine!])
            ])
        ])
    ])
    CPPFLAGS="$save_CPPFLAGS"
])

# See: http://gcc.gnu.org/ml/gcc/2000-05/msg01141.html
AC_DEFUN([CHECK_PTHREAD],
[
    # first try without -pthread
    AC_TRY_LINK(
    [
        #include <pthread.h>
    ],[
        pthread_create(0,0,0,0);
    ],[
        AC_MSG_RESULT(yes)
        PTHREAD_CPPFLAGS=
        PTHREAD_LDFLAGS=
        PTHREAD_LIBS=
    ],[

        # now with -pthread
        AC_CHECK_LIB(pthread,pthread_create,
        [
            PTHREAD_CPPFLAGS=
            PTHREAD_LDFLAGS=
            PTHREAD_LIBS=-lpthread
        ],[
            AC_MSG_CHECKING(if compiler supports -pthread)
            save_CPPFLAGS="$CPPFLAGS"
            CPPFLAGS="$CPPFLAGS -pthread"
            AC_TRY_LINK(
            [
                #include <pthread.h>
            ],[
                pthread_create(0,0,0,0);
            ],[
                AC_MSG_RESULT(yes)
                PTHREAD_CPPFLAGS=-pthread
                PTHREAD_LDFLAGS=-pthread
                PTHREAD_LIBS=
            ],[
                AC_MSG_RESULT(no)
                AC_MSG_CHECKING(if compiler supports -pthreads)
                save_CPPFLAGS="$CPPFLAGS"
                CPPFLAGS="$save_CPPFLAGS -pthreads"
                AC_TRY_LINK(
                [
                    #include <pthread.h>
                ],[
                    pthread_create(0,0,0,0);
                ],[
                    AC_MSG_RESULT(yes)
                    PTHREAD_CPPFLAGS=-pthreads
                    PTHREAD_LDFLAGS=-pthreads
                    PTHREAD_LIBS=
                ],[
                    AC_MSG_RESULT(no)
                    AC_MSG_CHECKING(if compiler supports -threads)
                    save_CPPFLAGS="$CPPFLAGS"
                    CPPFLAGS="$save_CPPFLAGS -threads"
                    AC_TRY_LINK(
                    [
                        #include <pthread.h>
                    ],[
                        pthread_create(0,0,0,0);
                    ],[
                        AC_MSG_RESULT(yes)
                        PTHREAD_CPPFLAGS=-threads
                        PTHREAD_LDFLAGS=-threads
                        PTHREAD_LIBS=
                    ],[
                        AC_MSG_ERROR([Your system is not supporting pthreads!])
                    ])
                ])
            ])
            CPPFLAGS="$save_CPPFLAGS"
        ])
    ])
])
