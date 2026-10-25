; Linux System Audit for Windows - installer (NSIS 3)
; Clean-room implementation of AIDA64 Business features. AIDA64 is by FinalWire Ltd.; not affiliated.
Unicode true
!ifndef VERSION
  !define VERSION "0.2.0"
!endif
!ifndef DIST
  !define DIST "dist/win"
!endif
!define APPNAME "Linux System Audit"
!define REGKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\LinuxSystemAudit"

Name "${APPNAME} ${VERSION}"
OutFile "${DIST}/lsa-sysinfo-${VERSION}-setup.exe"
InstallDir "$LOCALAPPDATA\Programs\${APPNAME}"
InstallDirRegKey HKCU "Software\LinuxSystemAudit" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
Icon "lsa.ico"
UninstallIcon "lsa.ico"
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "FileDescription" "${APPNAME} ${VERSION} Setup"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "GPL-2.0-or-later"

!include "MUI2.nsh"
!define MUI_ICON "lsa.ico"
!define MUI_UNICON "lsa.ico"
!define MUI_WELCOMEPAGE_TEXT "This installs ${APPNAME} ${VERSION} for Windows: the Cache & Memory Benchmark (Read / Write / Copy / Latency of memory and L1-L3 caches) and its user guide.$\r$\n$\r$\n${APPNAME} is a clean-room implementation of AIDA64 Business features. AIDA64 is by FinalWire Ltd.; this project is not affiliated with FinalWire.$\r$\n$\r$\nNo administrator rights are needed."
!define MUI_FINISHPAGE_RUN "$INSTDIR\lsa-gui.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Start the Cache && Memory Benchmark"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Linux System Audit (required)" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"
  File "${DIST}/lsa-gui.exe"
  File "${DIST}/lsa.exe"
  File /oname=README.txt "README.md"
  File /oname=CHANGELOG.txt "CHANGELOG.md"
  File /oname=LICENSE.txt "LICENSE"
  File "${DIST}/LSA-User-Guide.html"
  File "lsa.ico"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\LSA Cache & Memory Benchmark.lnk" "$INSTDIR\lsa-gui.exe" "" "$INSTDIR\lsa.ico"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\LSA User Guide.lnk" "$INSTDIR\LSA-User-Guide.html"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\Uninstall ${APPNAME}.lnk" "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "Software\LinuxSystemAudit" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "${REGKEY}" "DisplayName" "${APPNAME} ${VERSION}"
  WriteRegStr HKCU "${REGKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${REGKEY}" "Publisher" "Eugene Flanagin"
  WriteRegStr HKCU "${REGKEY}" "URLInfoAbout" "https://github.com/eugenesflanagin-spec/lsa-sysinfo"
  WriteRegStr HKCU "${REGKEY}" "DisplayIcon" "$INSTDIR\lsa.ico"
  WriteRegStr HKCU "${REGKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${REGKEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKCU "${REGKEY}" "NoModify" 1
  WriteRegDWORD HKCU "${REGKEY}" "NoRepair" 1
SectionEnd

Section "Desktop shortcut" SecDesktop
  CreateShortcut "$DESKTOP\LSA Cache & Memory Benchmark.lnk" "$INSTDIR\lsa-gui.exe" "" "$INSTDIR\lsa.ico"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\lsa-gui.exe"
  Delete "$INSTDIR\lsa.exe"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\CHANGELOG.txt"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\LSA-User-Guide.html"
  Delete "$INSTDIR\lsa.ico"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${APPNAME}\LSA Cache & Memory Benchmark.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\LSA User Guide.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\Uninstall ${APPNAME}.lnk"
  RMDir "$SMPROGRAMS\${APPNAME}"
  Delete "$DESKTOP\LSA Cache & Memory Benchmark.lnk"
  DeleteRegKey HKCU "${REGKEY}"
  DeleteRegKey HKCU "Software\LinuxSystemAudit"
SectionEnd
