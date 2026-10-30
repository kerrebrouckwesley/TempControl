; Inno Setup-script voor TempMonitor.
; Bouwen: voer ..\build-installer.ps1 uit (publiceert de app, haalt PresentMon op en compileert dit script).

#define AppName "TempMonitor"
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{6F3C2A1E-8B4D-4C7A-9E2F-5D1B3A7C9E40}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=QPRINTS
AppComments=Stuurt CPU/GPU-temperatuur en FPS naar het ronde USB-scherm
DefaultDirName={autopf}\{#AppName}
DisableProgramGroupPage=yes
DisableDirPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\..\dist
OutputBaseFilename=TempMonitor-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\TempMonitor.exe
CloseApplications=no

[Languages]
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "pawnio"; Description: "PawnIO-driver installeren (nodig om de CPU-temperatuur te lezen)"; Check: not PawnIOInstalled

[Files]
Source: "publish\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "task.ps1"; DestDir: "{app}"; Flags: ignoreversion

[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoProfile -ExecutionPolicy Bypass -Command ""winget install --id namazso.PawnIO -e --silent --accept-package-agreements --accept-source-agreements"""; \
  StatusMsg: "PawnIO-driver installeren..."; Tasks: pawnio; Flags: runhidden waituntilterminated
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\task.ps1"" -Action Install -ExePath ""{app}\TempMonitor.exe"""; \
  StatusMsg: "Automatisch starten instellen..."; Flags: runhidden waituntilterminated

[UninstallRun]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; \
  Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\task.ps1"" -Action Uninstall"; \
  RunOnceId: "RemoveTask"; Flags: runhidden waituntilterminated

[UninstallDelete]
Type: filesandordirs; Name: "{commonappdata}\TempMonitor"

[Code]
function PawnIOInstalled: Boolean;
begin
  Result := RegKeyExists(HKLM, 'SYSTEM\CurrentControlSet\Services\PawnIO');
end;

// Een draaiende versie stoppen voordat de bestanden overschreven worden (bij een update).
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    '-NoProfile -Command "Stop-ScheduledTask -TaskName TempMonitor -EA SilentlyContinue; Stop-Process -Name TempMonitor,PresentMon -Force -EA SilentlyContinue"',
    '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  Result := '';
end;
