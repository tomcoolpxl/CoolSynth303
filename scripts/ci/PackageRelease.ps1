# Packages a build made by BuildAndTest.ps1 (CoolSynth's D363): the zip of the app, the VST3 and the notices, the
# installer when asked (it needs Inno Setup 6.5 or newer), and a checksum file, all in build/ci-<configuration>/packages.
param(
    [Parameter(Mandatory)][ValidateSet('Debug', 'Release')][string]$Configuration,
    [string]$TagName = '',
    [switch]$Installer
)

. "$PSScriptRoot/Common.ps1"

$sourceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$context = New-Cs303BuildContext -SourceRoot $sourceRoot -Configuration $Configuration -TagName $TagName

$packages = @(New-Cs303Zip -BuildContext $context)
if ($Installer) {
    $packages = @(New-Cs303Installer -BuildContext $context) + $packages
}

$checksumPath = Join-Path $context.PackageDirectory "CoolSynth303-windows-x64-sha256-$($context.TagName).txt"
Write-Cs303ChecksumFile -PackagedAssets $packages -DestinationPath $checksumPath

Write-Host 'Packaged assets:'
foreach ($package in $packages) {
    Write-Host " - $($package.FileName)"
}
Write-Host " - $(Split-Path -Path $checksumPath -Leaf)"
