; IOcore installer (Inno Setup). Built by release.py, or by hand:
;   ISCC.exe /DAppVersion=0.1.0 /DSourceDir=..\release installer\IOcore.iss
; Installs the contents of release/ (GUI, firmware hex, simulator, avrdude, examples, presets)
; to Program Files\IOcore. Board files live in Documents\IOcore and are never touched.

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
AppName=IOcore
AppVersion={#AppVersion}
AppVerName=IOcore {#AppVersion}
AppPublisher=Open-Source-Model-Railway-Electronics
AppPublisherURL=https://github.com/Open-Source-Model-Railway-Electronics/IOcore
DefaultDirName={autopf}\IOcore
DefaultGroupName=IOcore
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=IOcore-v{#AppVersion}-setup
UninstallDisplayIcon={app}\IOcoreGUI.exe
SetupIconFile=..\software\gui\ui\oshw.ico
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
Name: "{autoprograms}\IOcore"; Filename: "{app}\IOcoreGUI.exe"
Name: "{autodesktop}\IOcore"; Filename: "{app}\IOcoreGUI.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\IOcoreGUI.exe"; Description: "Start IOcore"; Flags: nowait postinstall skipifsilent
