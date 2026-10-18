# Writes packaging/plugins.iss.inc (Inno Setup [Components], [Files], [UninstallDelete]) for every plugin in
# packaging/plugins.txt.   Usage: pwsh packaging/make_iss_include.ps1 <build dir>
# Artefacts are read from <build>/plugins/<Target>/<Target>_artefacts/Release.
param([string]$BuildDir = "build")
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$build = (Resolve-Path $BuildDir).Path
$plugins = Get-Content "$here/plugins.txt" | Where-Object { $_.Trim() -and -not $_.StartsWith('#') } | ForEach-Object {
    $parts = $_.Split([char]0x7C); @{ Target = $parts[0].Trim(); Product = $parts[1].Trim() } }
$c = @('[Components]',
       'Name: "vst3"; Description: "VST3 plugins (C:\Program Files\Common Files\VST3\Wavy Suite)"; Types: full custom',
       'Name: "clap"; Description: "CLAP plugins (C:\Program Files\Common Files\CLAP\Wavy Suite)"; Types: full custom')
$f = @('[Files]')
$u = @('[UninstallDelete]')
foreach ($pl in $plugins) {
    $id = $pl.Target.ToLower()
    $a = Join-Path $build "plugins\$($pl.Target)\$($pl.Target)_artefacts\Release"
    $c += "Name: ""vst3\$id""; Description: ""$($pl.Product)""; Types: full custom"
    $c += "Name: ""clap\$id""; Description: ""$($pl.Product)""; Types: full custom"
    $f += "Source: ""$a\VST3\$($pl.Product).vst3\*""; DestDir: ""{commoncf64}\VST3\Wavy Suite\$($pl.Product).vst3""; Components: vst3\$id; Flags: ignoreversion recursesubdirs createallsubdirs"
    $f += "Source: ""$a\CLAP\$($pl.Product).clap""; DestDir: ""{commoncf64}\CLAP\Wavy Suite""; Components: clap\$id; Flags: ignoreversion"
    $u += "Type: filesandordirs; Name: ""{commoncf64}\VST3\Wavy Suite\$($pl.Product).vst3"""
    $u += "Type: files; Name: ""{commoncf64}\CLAP\Wavy Suite\$($pl.Product).clap"""
}
($c + '' + $f + '' + $u) | Set-Content -Encoding utf8 "$here/plugins.iss.inc"
Get-Content "$here/plugins.iss.inc"
