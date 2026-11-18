; GPIOcore installer (Inno Setup). Built by release.py, or by hand:
;   ISCC.exe /DAppVersion=0.1.0 /DSourceDir=..\release installer\GPIOcore.iss
; Installs the contents of release/ (GUI, firmware hex, simulator, avrdude, examples, presets)
; to Program Files\GPIOcore. Board files live in Documents\GPIOcore and are never touched.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\release"
#endif
#ifndef OutputDir
  #define OutputDir "..\installer\output"
#endif

[Setup]
AppId={{6F3B2C1E-9A4D-4C7B-8E21-5D0A7C3F9B42}
AppName=GPIOcore
AppVersion={#AppVersion}
AppVerName=GPIOcore {#AppVersion}
AppPublisher=Open-Source-Model-Railway-Electronics
AppPublisherURL=https://github.com/Open-Source-Model-Railway-Electronics/GPIOcore
DefaultDirName={autopf}\GPIOcore
DefaultGroupName=GPIOcore
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=GPIOcore-v{#AppVersion}-setup
UninstallDisplayIcon={app}\GPIOcoreGUI.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
; everything build.py put in release/; a new version replaces the old files (presets and examples included)
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; files of an older version that are no longer shipped
Type: filesandordirs; Name: "{app}\presets"
Type: filesandordirs; Name: "{app}\examples"

[Icons]
Name: "{autoprograms}\GPIOcore"; Filename: "{app}\GPIOcoreGUI.exe"
Name: "{autodesktop}\GPIOcore"; Filename: "{app}\GPIOcoreGUI.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\GPIOcoreGUI.exe"; Description: "Start GPIOcore"; Flags: nowait postinstall skipifsilent
