#define MyAppVersion "1.0.0"
#define MyAppPublisher "XNA"
#define MyAppName "PrintDriver"
#define MyAppURL "https://github.com/xna00/windevtest"
#define MyAppExeName "PrintDriver.exe"

[Setup]
AppId={{PrintDriver-2024-1.0.0}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
LicenseFile=
InfoBeforeFile=
InfoAfterFile=
OutputDir=..\output
OutputBaseFilename=PrintDriver-Setup
SetupIconFile=
Compression=lzma
SolidCompression=yes
PrivilegesRequired=admin
MinVersion=6.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "autostart"; Description: "Start with Windows"; GroupDescription: "Startup options:"; Flags: checkedonce

[Files]
Source: "..\build\PrintDriver.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\cert\PrintDriver.cer"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{userdesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "PrintDriver"; ValueData: """{app}\{#MyAppExeName}"""; Flags: uninsdeletevalue; Tasks: autostart

[Run]
Filename: "certutil.exe"; Parameters: "-addstore ""TrustedPublisher"" ""{tmp}\PrintDriver.cer"""; Flags: runhidden waituntilterminated; StatusMsg: "Configuring security settings..."
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
