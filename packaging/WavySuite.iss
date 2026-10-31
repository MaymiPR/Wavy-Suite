; Windows installer for the whole Wavy Suite (Inno Setup 7). Built by .github/workflows/release.yml:
;   pwsh packaging/make_iss_include.ps1 build      (writes packaging/plugins.iss.inc from plugins.txt)
;   iscc /DAppVersion=<version> packaging\WavySuite.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{3E7A9C41-6B2D-4F58-9A1E-5C0D7B2E8F64}
AppName=Wavy Suite
AppVersion={#AppVersion}
AppPublisher=Wavy Suite
AppPublisherURL=https://github.com/MaymiPR/Wavy-Suite
AppSupportURL=https://github.com/MaymiPR/Wavy-Suite/issues
DefaultDirName={autopf}\Wavy Suite
DefaultGroupName=Wavy Suite
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=..\dist
OutputBaseFilename=WavySuite-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Inno Setup 7: a 64-bit Setup.exe (the default would still be 32-bit); AppVerName and TimeStampsInUTC are
; stated because their defaults changed in 7 (name plus version with no localized word; UTC time stamps).
SetupArchitecture=x64
AppVerName=Wavy Suite {#AppVersion}
TimeStampsInUTC=yes
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Wavy Suite

[Types]
Name: "full"; Description: "All plugins, VST3 and CLAP"
Name: "custom"; Description: "Choose plugins and formats"; Flags: iscustom

; [Components], [Files] and [UninstallDelete] for every plugin in plugins.txt
#include "plugins.iss.inc"

[Files]
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "THIRD-PARTY-NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Uninstall Wavy Suite"; Filename: "{uninstallexe}"
