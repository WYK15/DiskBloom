#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef StageDir
  #error StageDir is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif
#ifndef IconPath
  #error IconPath is required
#endif

[Setup]
AppId={{8DD36945-084D-4BDB-B871-AB457D25DFF9}
AppName=DiskBloom
AppVersion={#AppVersion}
AppPublisher=DiskBloom Contributors
DefaultDirName={autopf}\DiskBloom
DefaultGroupName=DiskBloom
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir={#OutputDir}
OutputBaseFilename=DiskBloom-{#AppVersion}-windows-x64-setup
SetupIconFile={#IconPath}
UninstallDisplayIcon={app}\DiskBloom.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=no
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "languages\ChineseSimplified.isl"

[CustomMessages]
english.CreateDesktopShortcut=Create a desktop shortcut
chinesesimplified.CreateDesktopShortcut=是否在桌面建立快捷方式

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopShortcut}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\DiskBloom"; Filename: "{app}\DiskBloom.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\DiskBloom"; Filename: "{app}\DiskBloom.exe"; WorkingDir: "{app}"; Tasks: desktopicon
