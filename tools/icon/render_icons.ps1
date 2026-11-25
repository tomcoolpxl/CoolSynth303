# Renders both icons at every size with headless Edge, from render.html served over HTTP with the panel font beside it:
#   copy resources\fonts\Archivo-ExtraBold.ttf here, then: python -m http.server 47634 --bind 127.0.0.1 --directory tools\icon
# then make_ico.py builds the .ico files (Pillow).
$s = $PSScriptRoot
$edge = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
foreach ($icon in "303", "808") {
    foreach ($size in 16, 20, 24, 32, 40, 48, 64, 96, 128, 256, 512) {
        $out = "$s\png\icon-$icon-$size.png"
        $p = Start-Process -FilePath $edge -ArgumentList "--headless=new", "--disable-gpu", "--hide-scrollbars",
            "--default-background-color=00000000", "--window-size=$size,$size", "--virtual-time-budget=4000",
            "--user-data-dir=$s\edge-prof", "--screenshot=$out",
            "http://127.0.0.1:47634/render.html?icon=$icon&size=$size" -PassThru -WindowStyle Hidden
        $p | Wait-Process -Timeout 60
        "{0} {1} px: {2} bytes" -f $icon, $size, (Get-Item $out).Length
    }
}

