; docker/installer.nsi
; Nullsoft Scriptable Install System (NSIS) Script for OpenArch (x86_64)

!include "MUI2.nsh"
!include "x64.nsh"

; ------------------------------------------------------------
; General Settings
; ------------------------------------------------------------
Name "OpenArch"
Caption "OpenArch Setup"
OutFile "${OUTPUT_INSTALLER}"
Unicode True

; Default 64-bit Program Files installation directory
InstallDir "$PROGRAMFILES64\OpenArch"
InstallDirRegKey HKLM "Software\OpenArch" "InstallDir"
RequestExecutionLevel admin

; ------------------------------------------------------------
; Interface Settings
; ------------------------------------------------------------
!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_NODESC

; Installer Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

; Finish Page: Launch OpenArch option
!define MUI_FINISHPAGE_RUN "$INSTDIR\openarch-gui.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch OpenArch"
!insertmacro MUI_PAGE_FINISH

; Uninstaller Pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Languages
!insertmacro MUI_LANGUAGE "English"

; ------------------------------------------------------------
; Installer Section
; ------------------------------------------------------------
Section "OpenArch (Required)" SecCore
    SectionIn RO

    ${If} ${RunningX64}
        SetRegView 64
    ${EndIf}

    ; Target installation directory
    SetOutPath "$INSTDIR"

    ; Copy all packaged distribution files recursively
    File /r "${DIST_DIR}\*.*"

    ; Create uninstaller executable
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Register Install Directory in Registry
    WriteRegStr HKLM "Software\OpenArch" "InstallDir" "$INSTDIR"

    ; Register in Windows "Add or Remove Programs"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenArch" "DisplayName" "OpenArch - Architecture Modeling & Governance"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenArch" "DisplayVersion" "1.0.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenArch" "Publisher" "OpenArch Team"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenArch" "DisplayIcon" "$INSTDIR\openarch-gui.exe,0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenArch" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenArch" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenArch" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenArch" "NoRepair" 1

    ; Create Start Menu shortcuts
    CreateDirectory "$SMPROGRAMS\OpenArch"
    CreateShortcut "$SMPROGRAMS\OpenArch\OpenArch.lnk" "$INSTDIR\openarch-gui.exe" "" "$INSTDIR\openarch-gui.exe" 0
    CreateShortcut "$SMPROGRAMS\OpenArch\OpenArch Theme Editor.lnk" "$INSTDIR\openarch-theme-editor.exe" "" "$INSTDIR\openarch-theme-editor.exe" 0
    CreateShortcut "$SMPROGRAMS\OpenArch\Uninstall OpenArch.lnk" "$INSTDIR\Uninstall.exe" "" "$INSTDIR\Uninstall.exe" 0

    ; Create Desktop shortcut
    CreateShortcut "$DESKTOP\OpenArch.lnk" "$INSTDIR\openarch-gui.exe" "" "$INSTDIR\openarch-gui.exe" 0

SectionEnd

; ------------------------------------------------------------
; Uninstaller Section
; ------------------------------------------------------------
Section "Uninstall"
    ${If} ${RunningX64}
        SetRegView 64
    ${EndIf}

    ; Remove Shortcuts
    Delete "$DESKTOP\OpenArch.lnk"
    Delete "$SMPROGRAMS\OpenArch\OpenArch.lnk"
    Delete "$SMPROGRAMS\OpenArch\OpenArch Theme Editor.lnk"
    Delete "$SMPROGRAMS\OpenArch\Uninstall OpenArch.lnk"
    RMDir "$SMPROGRAMS\OpenArch"

    ; Remove Registry entries
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenArch"
    DeleteRegKey HKLM "Software\OpenArch"

    ; Remove installed files and directories
    RMDir /r "$INSTDIR\platforms"
    RMDir /r "$INSTDIR\imageformats"
    RMDir /r "$INSTDIR\iconengines"
    RMDir /r "$INSTDIR\styles"
    RMDir /r "$INSTDIR\themes"
    RMDir /r "$INSTDIR"
SectionEnd

