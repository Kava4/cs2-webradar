# Build the all-in-one AimSync WebRadar exe (embeds web server + client)
# Usage: .\build-aio.ps1

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$radar = Join-Path $root "radar"
$client = $PSScriptRoot
$clientData = Join-Path $client "data"
$radarData = Join-Path $radar "public\data"

Write-Host "==> Generating brand icons from logo.png..." -ForegroundColor Cyan
python (Join-Path $client "generate-brand-assets.py")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not (Test-Path $clientData)) {
    Write-Error "Map assets missing. Expected folder: $clientData"
}

if (Test-Path $radarData) { Remove-Item $radarData -Recurse -Force }
Copy-Item $clientData $radarData -Recurse -Force
Write-Host "==> Map assets synced: client/data -> radar/public/data" -ForegroundColor Green

Write-Host "==> Building radar UI + pkg server..." -ForegroundColor Cyan
Set-Location $radar
npm run build:all
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$server = Join-Path $radar "ws\release\aimsync_webradar_server.exe"
if (-not (Test-Path $server)) {
    Write-Error "Server exe not found at $server"
}

Copy-Item $server (Join-Path $client "aimsync_webradar_server.exe") -Force
Write-Host "==> Server exe copied to client/" -ForegroundColor Green

Write-Host "==> Open client\cs2_webradar.sln in Visual Studio and Build (release|x64)" -ForegroundColor Cyan
Write-Host "    Output: client\Release\aimsync_webradar.exe (+ client\Release\data\)" -ForegroundColor Cyan
