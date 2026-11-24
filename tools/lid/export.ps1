# Writes resources/lid/tb303-sheet.json, the lid's drawing, from CoolSynth's mockup (CoolSynth's D310):
# docs/design/mockups/coolsynth-303-lid.html with ?export=1 lays the sheet out, checks it against the netlist and logs
# it as JSON in pieces; a headless Edge runs it and join_export.py puts the pieces together.
#
#   powershell -File tools\lid\export.ps1 -Mockup E:\github\CoolSynth\docs\design\mockups\coolsynth-303-lid.html
param([Parameter(Mandatory = $true)][string]$Mockup)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$work = Join-Path ([IO.Path]::GetTempPath()) "coolsynth303-lid-export"
New-Item -ItemType Directory -Force $work | Out-Null
$log = Join-Path $work "profile\chrome_debug.log"
if (Test-Path $log) { Remove-Item $log }
$edge = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
$url = "file:///" + ((Resolve-Path $Mockup).Path -replace '\\', '/') + "?export=1"
& $edge --headless=new --disable-gpu "--user-data-dir=$work\profile" --enable-logging --v=0 --window-size=1280,800 `
    --virtual-time-budget=20000 "--screenshot=$work\shot.png" $url | Out-Null
Start-Sleep -Seconds 3
$out = Join-Path $here "..\..\resources\lid\tb303-sheet.json"
python -I -X utf8 (Join-Path $here "join_export.py") $log $out
