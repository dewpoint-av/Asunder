; Asunder — Inno Setup installer
; Packages the CLAP and the VST3 bundle to their standard system-wide
; locations. Run build.bat first so build\dist\ is populated.

#define MyAppName "Asunder"
#define MyAppVersion "1.1.0"
#define MyAppPublisher "Dewpoint AV"
#define MyAppURL "https://dewpoint.live"

[Setup]
AppId={{3F7A9C1B-2D4E-4B8A-9C6D-5E1F0A2B3C4D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\Dewpoint AV\{#MyAppName}
DefaultGroupName=Dewpoint AV\{#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=yes
OutputDir=output
OutputBaseFilename={#MyAppName}-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
SetupLogging=yes
UninstallDisplayName={#MyAppName} (Dewpoint AV)

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "..\build\dist\Asunder.vst3\*"; DestDir: "{commoncf}\VST3\{#MyAppName}.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\dist\Asunder.clap"; DestDir: "{commoncf}\CLAP"; Flags: ignoreversion

[InstallDelete]
Type: filesandordirs; Name: "{commoncf}\VST3\Asunder.vst3"
Type: files; Name: "{commoncf}\CLAP\Asunder.clap"

[Icons]
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"

[Messages]
WelcomeLabel1=Asunder Distortion
WelcomeLabel2=This will install Asunder (VST3 + CLAP) on your computer.%n%nClick Next to continue.
