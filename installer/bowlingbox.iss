; Bowling Box — per-user Windows installer (Inno Setup 6.3 or newer).
;
; Built by scripts\build_windows_installer.ps1, which passes StageDir (the
; staged BowlingBox install component), ArtDir (wizard BMPs generated from
; docs\Neon-Bowling-phone.png), OutputDir and MyAppVersion (from
; the configured CMAKE_PROJECT_VERSION, i.e. the nearest vX.Y.Z git tag).
;
; Bowling Box keeps no user data in its folder: an upgrade overwrites the
; program files and an uninstall removes them.

; x64compatible and the multi-resolution wizard images need Inno Setup 6.3.
#if Ver < EncodeVer(6,3,0)
  #error Inno Setup 6.3 or newer is required
#endif

#ifndef StageDir
  #define StageDir "..\build\windows\installer\stage\bin"
#endif
#ifndef ArtDir
  #define ArtDir "..\build\windows\installer\art"
#endif
#ifndef OutputDir
  #define OutputDir "..\build\windows\dist"
#endif
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif

#define MyAppName "Bowling Box"
#define MyAppExeName "BowlingGL.exe"

; Bowling Box's own AppId. Never change it: it is how this installer
; recognises a copy it installed earlier and upgrades it in place.
#define MyAppId "{35C7CD0F-A6C2-4FD0-B3A1-E896DFA6B142}"

[Setup]
AppId={{#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppName}
VersionInfoVersion={#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
; Always per-user: no UAC prompt. {autopf} is {userpf} in this mode.
PrivilegesRequired=lowest
OutputDir={#OutputDir}
OutputBaseFilename=BowlingBox_Setup_{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
WizardImageFile={#ArtDir}\wizard-164.bmp,{#ArtDir}\wizard-246.bmp,{#ArtDir}\wizard-328.bmp
WizardSmallImageFile={#ArtDir}\small-55.bmp,{#ArtDir}\small-83.bmp,{#ArtDir}\small-110.bmp
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}
; A running BowlingGL.exe holds its DLLs open; let the Restart Manager close it.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
