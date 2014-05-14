AC_DEFUN([AC_PACKAGE_NEED_QT_QMAKE],
  [ if test -x "$QTDIR/bin/qmake.exe"; then
	QMAKE="$QTDIR/bin/qmake.exe"
    fi
    if test -z "$QMAKE"; then
	AC_PATH_PROGS(QMAKE, [qmake-qt4 qmake],, [$QTDIR/bin:/usr/bin:/usr/lib64/qt4/bin:/usr/lib/qt4/bin])
    fi
    qmake=$QMAKE
    AC_SUBST(qmake)
    AC_PACKAGE_NEED_UTILITY($1, "$qmake", qmake, [Qt make])
  ])

AC_DEFUN([AC_PACKAGE_NEED_QT_VERSION4],
  [ AC_MSG_CHECKING([Qt version])
    eval `$qmake --version | awk '/Using Qt version/ { ver=4; print $ver }' | awk -F. '{ major=1; minor=2; point=3; printf "export QT_MAJOR=%d QT_MINOR=%d QT_POINT=%d\n",$major,$minor,$point }'`
    if test "$QT_MAJOR" -lt 4 ; then
	echo
	echo FATAL ERROR: Qt version 4 does not seem to be installed.
	echo Cannot proceed with the Qt $QT_MAJOR installation found.
	exit 1
    fi
    if test "$QT_MAJOR" -eq 4 -a "$QT_MINOR" -lt 4 ; then
	echo
	echo FATAL ERROR: Qt version 4.$QT_MINOR is too old.
	echo Qt version 4.4 or later is required.
	exit 1
    fi
    AC_MSG_RESULT([$QT_MAJOR.$QT_MINOR.$QT_POINT])
  ])

AC_DEFUN([AC_PACKAGE_NEED_QT_UIC],
  [ if test -z "$UIC"; then
	AC_PATH_PROGS(UIC, [uic-qt4 uic],, [$QTDIR/bin:/usr/bin:/usr/lib64/qt4/bin:/usr/lib/qt4/bin])
    fi
    uic=$UIC
    AC_SUBST(uic)
    AC_PACKAGE_NEED_UTILITY($1, "$uic", uic, [Qt User Interface Compiler])
  ])

AC_DEFUN([AC_PACKAGE_NEED_QT_MOC],
  [ if test -z "$MOC"; then
	AC_PATH_PROGS(MOC, [moc-qt4 moc],, [$QTDIR/bin:/usr/bin:/usr/lib64/qt4/bin:/usr/lib/qt4/bin])
    fi
    moc=$MOC
    AC_SUBST(moc)
    AC_PACKAGE_NEED_UTILITY($1, "$uic", uic, [Qt Meta-Object Compiler])
  ])
