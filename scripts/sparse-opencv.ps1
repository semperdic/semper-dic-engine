# Drop unused OpenCV trees from the submodule worktree (doc/samples/data/apps).
# Run from the engine repo root after: git submodule update --init --recursive
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Oc = Join-Path $Root "third_party\opencv"
if (-not (Test-Path $Oc)) {
    Write-Error "OpenCV submodule not checked out at $Oc"
}
Push-Location $Oc
try {
    git sparse-checkout init --cone
    git sparse-checkout set modules include 3rdparty cmake platforms hal
    git checkout -- .
    foreach ($stub in @("doc", "data")) {
        $dir = Join-Path $Oc $stub
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        Set-Content -Path (Join-Path $dir "CMakeLists.txt") `
            -Value "# Auto-generated stub for sparse OpenCV checkout`n"
    }
    Write-Host "OpenCV sparse checkout applied under $Oc"
    $size = (Get-ChildItem -Recurse -File -ErrorAction SilentlyContinue |
        Measure-Object Length -Sum).Sum / 1MB
    Write-Host ("Approx worktree size: {0:N0} MB" -f $size)
} finally {
    Pop-Location
}
