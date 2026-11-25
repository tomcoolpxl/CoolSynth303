# Configures and builds CoolSynth 303 (the app, the VST3 and the tests) in build/ci-<configuration>, and runs the
# whole test program when asked (CoolSynth's D363, as CoolSynth's own BuildAndTest.ps1).
param(
    [Parameter(Mandatory)][ValidateSet('Debug', 'Release')][string]$Configuration,
    [switch]$RunTests
)

. "$PSScriptRoot/Common.ps1"

$sourceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path
$context = New-Cs303BuildContext -SourceRoot $sourceRoot -Configuration $Configuration

Write-Host "Configuration: $($context.Configuration)"
Write-Host "Build directory: $($context.BuildDirectory)"

Invoke-Cs303Configure -BuildContext $context
Invoke-Cs303Build -BuildContext $context
if ($RunTests) {
    Invoke-Cs303Tests -BuildContext $context
}
