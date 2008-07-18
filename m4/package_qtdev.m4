AC_DEFUN([AC_PACKAGE_NEED_QT_QMAKE],
  [ if test -x "$QTDIR\bin\qmake.exe"; then
	QMAKE="$QTDIR\bin\qmake.exe"
    fi
    if test -z "$QMAKE"; then
	AC_PATH_PROGS(QMAKE, [qmake-qt4 qmake],, [$QTDIR/bin:/usr/bin:/usr/lib64/qt4/bin:/usr/lib/qt4/bin])
    fi
    qmake=$QMAKE
    AC_SUBST(qmake)
    AC_PACKAGE_NEED_UTILITY($1, "$qmake", qmake, [Qt make])
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
