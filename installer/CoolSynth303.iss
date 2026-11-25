; CoolSynth 303's Windows installer (CoolSynth's D363, as CoolSynth's own, its D209), built with Inno Setup 6.5 or newer:
;   ISCC /DAppVersion=0.1.0 "/DBuildDir=..\build\CoolSynth303_artefacts\Release" installer\CoolSynth303.iss
; scripts\ci\PackageRelease.ps1 -Installer passes the version from CMakeLists.txt. It installs the standalone app with
; a Start menu entry, and the VST3 where hosts look (Common Files\VST3). Uninstalling removes what it installed and
; nothing else: the player's own library (%APPDATA%\CoolSynth303\Library) and the app's settings beside it stay.
; No firewall rule: Windows asks once, on the app's first start, for the beat sync (the user's pick, D363).

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\build\CoolSynth303_artefacts\Release"
#endif

[Setup]
; Fixed for every version, so a new installer updates the one already there.
AppId={{E5134CB7-ABB3-42A5-9845-7E696899FBEA}
AppName=CoolSynth 303
AppVersion={#AppVersion}
AppVerName=CoolSynth 303 {#AppVersion}
AppPublisher=tomcoolpxl
AppPublisherURL=https://github.com/tomcoolpxl/CoolSynth303
AppSupportURL=https://github.com/tomcoolpxl/CoolSynth303/issues
AppUpdatesURL=https://github.com/tomcoolpxl/CoolSynth303/releases
DefaultDirName={autopf}\CoolSynth 303
DisableProgramGroupPage=yes
; The licence page shows the app's own MIT licence (the user's pick, D363); it ships beside the app as LICENSE.txt.
LicenseFile=..\LICENSE
OutputDir=..\build\installer
OutputBaseFilename=CoolSynth303-Setup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
UninstallDisplayName=CoolSynth 303
; The icon is still to be chosen (the user's): once installer\CoolSynth303.ico is there (16 to 256 px), the
; installer and the Installed apps list use it; until then Inno Setup's own and the app's.
#if FileExists(AddBackslash(SourcePath) + "CoolSynth303.ico")
SetupIconFile=CoolSynth303.ico
UninstallDisplayIcon={app}\CoolSynth303.ico
#else
UninstallDisplayIcon={app}\CoolSynth 303.exe
#endif
VersionInfoVersion={#AppVersion}
VersionInfoProductName=CoolSynth 303

[Types]
Name: "full"; Description: "Everything: the app and the VST3 plugin"
Name: "app"; Description: "The app only"
Name: "vst3"; Description: "The VST3 plugin only"

[Components]
Name: "app"; Description: "Standalone app"; Types: full app
Name: "vst3"; Description: "VST3 plugin, to play CoolSynth 303 in your music software (DAW)"; Types: full vst3

[Files]
Source: "{#BuildDir}\Standalone\CoolSynth 303.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#BuildDir}\VST3\CoolSynth 303.vst3\*"; DestDir: "{commoncf64}\VST3\CoolSynth 303.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
#if FileExists(AddBackslash(SourcePath) + "CoolSynth303.ico")
Source: "CoolSynth303.ico"; DestDir: "{app}"; Flags: ignoreversion
#endif

[Icons]
Name: "{autoprograms}\CoolSynth 303"; Filename: "{app}\CoolSynth 303.exe"; Components: app

[Run]
Filename: "{app}\CoolSynth 303.exe"; Description: "Start CoolSynth 303"; Components: app; Flags: nowait postinstall skipifsilent
; The guide on the web, where it shows as a page.
Filename: "https://github.com/tomcoolpxl/CoolSynth303#readme"; Description: "Open CoolSynth 303's guide on the web"; Flags: shellexec nowait postinstall skipifsilent unchecked
