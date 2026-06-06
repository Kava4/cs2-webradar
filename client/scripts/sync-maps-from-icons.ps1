# Sync missing map assets from MurkyYT/cs2-map-icons into client/data/
# Usage: .\client\scripts\sync-maps-from-icons.ps1

$ErrorActionPreference = "Stop"

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$clientDir = Resolve-Path (Join-Path $scriptDir "..")
$dataDir = Join-Path $clientDir "data"
$coordsFile = Join-Path $scriptDir "map-coords-fallback.json"

$skipMaps = @("cs_icon", "lobby_mapveto")
$iconsManifestUrl = "https://raw.githubusercontent.com/MurkyYT/cs2-map-icons/main/data/available.json"

function Get-RadarUrl {
	param($RadarPaths)
	if (-not $RadarPaths -or $RadarPaths.Count -eq 0) { return $null }

	$upper = @($RadarPaths | Where-Object { $_ -notmatch "_lower_" })
	if ($upper.Count -gt 0) {
		$psd = @($upper | Where-Object { $_ -match "_radar_psd" })
		if ($psd.Count -gt 0) { return $psd[0] }
		return $upper[0]
	}

	return $RadarPaths[0]
}

function Get-BackgroundUrl {
	param($MapEntry)
	if ($MapEntry.thumb_paths -and $MapEntry.thumb_paths.Count -gt 0) {
		$first = @($MapEntry.thumb_paths | Where-Object { $_ -match "_1_png" })
		if ($first.Count -gt 0) { return $first[0] }
		return $MapEntry.thumb_paths[-1]
	}
	return $MapEntry.path
}

function Download-File {
	param(
		[string]$Url,
		[string]$Dest
	)
	$parent = Split-Path -Parent $Dest
	if (-not (Test-Path $parent)) {
		New-Item -ItemType Directory -Path $parent -Force | Out-Null
	}
	Invoke-WebRequest -Uri $Url -OutFile $Dest -UseBasicParsing
}

function Write-DataJson {
	param(
		[string]$MapName,
		[string]$Dest,
		$KnownCoords
	)

	if ($KnownCoords.PSObject.Properties.Name -contains $MapName) {
		$entry = $KnownCoords.$MapName
	}
	else {
		# Approximate placeholder so the radar UI can load; tune from game overview files.
		$entry = @{ x = -3230; y = 1713; scale = 5.0 }
		Write-Warning "No coords for $MapName - using placeholder (positions may be off)"
	}

	$json = '{' + [Environment]::NewLine +
		"`t""x"": $($entry.x)," + [Environment]::NewLine +
		"`t""y"": $($entry.y)," + [Environment]::NewLine +
		"`t""scale"": $($entry.scale)" + [Environment]::NewLine +
		'}'
	Set-Content -Path $Dest -Value $json -Encoding UTF8 -NoNewline
}

Write-Host "Fetching cs2-map-icons manifest..."
$manifest = Invoke-RestMethod -Uri $iconsManifestUrl
$knownCoords = @{}
if (Test-Path $coordsFile) {
	$knownCoords = Get-Content $coordsFile -Raw | ConvertFrom-Json
}

$existing = @{}
if (Test-Path $dataDir) {
	$existing = @{}
	Get-ChildItem $dataDir -Directory | ForEach-Object { $existing[$_.Name] = $true }
}

$added = 0
$skipped = 0

$mapNames = @($manifest.maps.PSObject.Properties | ForEach-Object { $_.Name })

foreach ($mapName in $mapNames) {
	if ($skipMaps -contains $mapName) {
		Write-Host "Skip non-playable: $mapName"
		continue
	}

	if ($existing.ContainsKey($mapName)) {
		$skipped++
		continue
	}

	$entry = $manifest.maps.$mapName
	$mapDir = Join-Path $dataDir $mapName
	New-Item -ItemType Directory -Path $mapDir -Force | Out-Null

	$radarUrl = Get-RadarUrl $entry.radar_paths
	if (-not $radarUrl) {
		$radarUrl = $entry.path
		Write-Warning "$mapName has no radar image - using map icon"
	}

	$bgUrl = Get-BackgroundUrl $entry

	Write-Host "Adding $mapName..."
	Download-File -Url $radarUrl -Dest (Join-Path $mapDir "radar.png")
	Download-File -Url $bgUrl -Dest (Join-Path $mapDir "background.png")
	Write-DataJson -MapName $mapName -Dest (Join-Path $mapDir "data.json") -KnownCoords $knownCoords
	$added++
}

Write-Host ""
Write-Host "Done. Added $added map(s), skipped $skipped existing."
