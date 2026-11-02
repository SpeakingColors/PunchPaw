; Windows installer + uninstaller (Inno Setup). Called as: ISCC /DName=PunchPaw "/DProduct=PunchPaw" /DVersion=0.1.0 packaging/windows.iss
; Files are staged in ..\stage by the release workflow. Works offline.
; The uninstaller removes everything it installed. Your saved presets (%APPDATA%\Studio 87\PunchPaw) are kept
; unless you answer Yes when it asks. A silent uninstall (/SILENT, /VERYSILENT) always keeps them.
[Setup]
AppId={{B5C1D2A4-7E3F-4A61-9C58-2D7F0E9A1B63}
AppName={#Product}
AppVersion={#Version}
AppPublisher=Studio 87
AppPublisherURL=https://github.com/SpeakingColors/PunchPaw
DefaultDirName={autopf}\Studio 87\{#Product}
DefaultGroupName=Studio 87
OutputDir=..\Output
OutputBaseFilename={#Name}-Setup
LicenseFile=..\LICENSE
SetupIconFile=..\assets\icon.ico
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2
SolidCompression=yes
UninstallDisplayName={#Product}
UninstallDisplayIcon={app}\icon.ico
CloseApplications=yes
DisableProgramGroupPage=yes

[Messages]
SelectDirDesc=Where should the manual and the uninstaller go?
SelectDirLabel3=The plugin itself always goes to the common VST3 folder (C:\Program Files\Common Files\VST3). This folder only holds the manual, the icon and the uninstaller.
SelectDirBrowseLabel=To continue, click Next. Choose another folder only if you want the manual somewhere else.

[Files]
Source: "..\stage\vst3\*"; DestDir: "{commoncf64}\VST3\{#Product}.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\stage\{#Name}-manual.pdf"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\icon.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#Product} manual"; Filename: "{app}\{#Name}-manual.pdf"; IconFilename: "{app}\icon.ico"
Name: "{group}\Uninstall {#Product}"; Filename: "{uninstallexe}"; IconFilename: "{app}\icon.ico"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Product}.vst3"
Type: filesandordirs; Name: "{app}"

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  presets, vendor: String;
begin
  if CurUninstallStep <> usPostUninstall then exit;
  presets := ExpandConstant('{userappdata}\Studio 87\{#Product}');
  vendor := ExpandConstant('{userappdata}\Studio 87');
  if DirExists(presets) and (not UninstallSilent) then
    if MsgBox('Also delete your saved {#Product} presets?' + #13#10 + #13#10 + presets + #13#10 + #13#10 +
              'Choose No to keep them (recommended if you plan to reinstall).', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
      DelTree(presets, True, True, True);
  RemoveDir(vendor);   { only succeeds when empty }
end;
