!include "config.nsh"
!include "common.nsh"
!include "MUI2.nsh"

Name "Performance Co-Pilot ${VERSION}"
OutFile "pcp-${VERSION}-setup.exe"

InstallDir "C:\PCP"
InstallDirRegKey HKCU "Software\PCP" ""
RequestExecutionLevel admin

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "aboutpcp.bmp"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_LICENSE "${TOPDIR}/COPYING"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

Section "PCP"
  SetOutPath "$INSTDIR"
  CreateDirectory "$INSTDIR\${INSTPCP}"
  File "/oname=$INSTDIR\${INSTPCP}\pcp-dirs" "pcp-dirs"
  File "/oname=$INSTDIR\${INSTPCP}\pcp-files" "pcp-files"
  !include "nsi-dirs"
  !include "nsi-files"
  WriteRegStr HKCU "Software\PCP" "" $INSTDIR
  WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Uninstall"
  ${CheckUnInstallFile} "INSTDIR\${INSTPCP}\pcp-files" \
	"Installed PCP files list missing.$r$\nUninstall cannot proceed."
  ${CheckUnInstallFile} "INSTDIR\${INSTPCP}\pcp-dirs" \
	"Installed PCP folders list missing.$r$\nUninstall cannot proceed."
  ${CheckNoInstallFile} "INSTDIR\${INSTPCP}\kmchart-dirs" \
	"Installed kmchart folders list exists$\r$\nUninstall cannot proceed."
  ${CheckNoInstallFile} "INSTDIR\${INSTPCP}\kmchart-files" \
	"Installed kmchart files list exists.$\r$\nUninstall cannot proceed."
  Delete "$INSTDIR\Uninstall.exe"
  ${DeleteAllFiles} "$INSTDIR\${INSTPCP}\pcp-files"
  ${DeleteAllDirs} "$INSTDIR\${INSTPCP}\pcp-dirs"
  Delete "$INSTDIR\${INSTPCP}\pcp-dirs"
  Delete "$INSTDIR\${INSTPCP}\pcp-files"
  RMDir "$INSTDIR\${INSTPCP}"
  RMDir "$INSTDIR"
  DeleteRegKey /ifempty HKCU "Software\PCP"
SectionEnd
