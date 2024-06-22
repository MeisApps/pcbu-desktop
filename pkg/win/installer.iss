[Setup]
AppName=PC Bio Unlock
AppVerName=PC Bio Unlock
WizardStyle=modern
DefaultDirName={autopf}\PCBioUnlock
DefaultGroupName=PC Bio Unlock
UninstallDisplayIcon={app}\pcbu_desktop.exe
Compression=lzma2
SolidCompression=yes
OutputDir=..\build\
PrivilegesRequired=admin
ArchitecturesAllowed={#GetEnv('ARCH')}
ArchitecturesInstallIn64BitMode={#GetEnv('ARCH')}
;ArchitecturesAllowed=arm64
;ArchitecturesInstallIn64BitMode=arm64

[Files]
Source: "..\build\installer_dir\*"; DestDir: "{app}"; Flags: recursesubdirs

[UninstallDelete]
Type: files; Name: "{win}\System32\win-pcbiounlock.dll"

[Icons]
Name: "{group}\PC Bio Unlock"; Filename: "{app}\pcbu_desktop.exe"

[Run]
Filename: "{app}\pcbu_desktop.exe"; Description: "Launch PC Bio Unlock"; Verb: runas; Flags: postinstall nowait skipifsilent runascurrentuser shellexec

[Code]
const
  VCRedistURL_x64 = 'https://aka.ms/vs/17/release/vc_redist.x64.exe';
  VCRedistURL_arm64 = 'https://aka.ms/vs/17/release/vc_redist.arm64.exe';
var
  DownloadPage: TDownloadWizardPage;

function InstallX64: Boolean;
begin
  Result := Is64BitInstallMode and (ProcessorArchitecture = paX64);
end;

function InstallARM64: Boolean;
begin
  Result := Is64BitInstallMode and (ProcessorArchitecture = paARM64);
end;

procedure InstallVCRedist(const FileName: string);
var
  ResultCode: Integer;
begin
  if ShellExec('runas', FileName, '/install /quiet /norestart', '', SW_SHOW, ewWaitUntilTerminated, ResultCode) then
  begin
    Log('vcredist installed successfully.');
  end
  else
  begin
    RaiseException('Failed to install vcredist. Error code: ' + IntToStr(ResultCode));
  end;
end;

function OnDownloadProgress(const Url, FileName: String; const Progress, ProgressMax: Int64): Boolean;
begin
  if Progress = ProgressMax then begin
    Log(Format('Successfully downloaded file to %s', [Format('%s\%s', [ExpandConstant('{tmp}'), FileName])]));
  end;
  Result := True;
end;

procedure InitializeWizard;
begin
  DownloadPage := CreateDownloadPage(SetupMessage(msgWizardPreparing), SetupMessage(msgPreparingDesc), @OnDownloadProgress);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  if CurPageID = wpReady then begin
    DownloadPage.Clear;
    if InstallX64() then begin
      DownloadPage.Add(VCRedistURL_x64, 'vcredist.exe', '');
    end else begin
      DownloadPage.Add(VCRedistURL_arm64, 'vcredist.exe', '');
    end;
    DownloadPage.Show;
    try
      try
        DownloadPage.Download;
        InstallVCRedist(Format('%s\vcredist.exe', [ExpandConstant('{tmp}')]));
        Result := True;
      except
        SuppressibleMsgBox(AddPeriod(GetExceptionMessage), mbCriticalError, MB_OK, IDOK);
        Result := False;
      end;
    finally
      DownloadPage.Hide;
    end;
  end else
    Result := True;
end;
