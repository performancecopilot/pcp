!macro DeleteAllFiles Path
  ClearErrors
  FileOpen $0 ${Path} r
  IfErrors doneDelFiles
  moreDelFiles:
    ClearErrors
    FileRead $0 $1
    IfErrors doneDelFiles
    Delete $1
    Goto moreDelFiles
  doneDelFiles:
  FileClose $0
!macroend
!define DeleteAllFiles "!insertmacro DeleteAllFiles"

!macro DeleteAllDirs Path
  ClearErrors
  FileOpen $0 ${Path} r
  IfErrors doneDelDirs
  moreDelDirs:
    ClearErrors
    FileRead $0 $1
    IfErrors doneDelDirs
    RMDir $1
    Goto moreDelDirs
  doneDelDirs:
  FileClose $0
!macroend
!define DeleteAllDirs "!insertmacro DeleteAllDirs"

!macro CheckUnInstallFile Path Message
  IfFileExists ${Path} 0 +3
    MessageBox MB_OK|MB_ICONSTOP "${Message}"
    Abort
!macroend
!define CheckUnInstallFile "!insertmacro CheckUnInstallFile"

!macro CheckNoInstallFile Path Message
  IfFileExists ${Path} +3 0
    MessageBox MB_OK|MB_ICONSTOP "${Message}"
    Abort
!macroend
!define CheckNoInstallFile "!insertmacro CheckNoInstallFile"
