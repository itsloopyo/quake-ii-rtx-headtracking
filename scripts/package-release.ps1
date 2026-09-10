#!/usr/bin/env pwsh
#Requires -Version 5.1
# Packaging for Quake II RTX Head Tracking (C++ ASI mod, no .csproj).
# Produces one ZIP in release/:
#   - QuakeIIRTXHeadTracking-v{version}-installer.zip  (GitHub: install.cmd + plugins/ + vendor + docs)
# CI is offline: consumes whatever is committed under vendor/.
#
# There is deliberately no -nexus.zip. This mod is installer-only because no mod
# manager can deploy it: the payload lands next to q2rtx.exe at the game root,
# and Vortex has no extension for Quake II RTX at all - not bundled under
# resources/app.asar.unpacked/bundledPlugins/, and none published to install -
# so there is no queryModPath to deploy into and no registerModType that could
# reach the game root. A ZIP laid out for a manager would install cleanly,
# deploy nothing the loader can see, and report success. Do not add one back
# without redoing that check.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = 'SilentlyContinue'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

Import-Module (Join-Path $projectDir "cameraunlock-core\powershell\ReleaseWorkflow.psm1") -Force

$versionLine = Select-String -Path (Join-Path $projectDir "src\version.h") -Pattern 'MOD_VERSION\s*=\s*"([0-9]+\.[0-9]+\.[0-9]+)"'
if (-not $versionLine) { throw "Could not find MOD_VERSION in src/version.h" }
$version = $versionLine.Matches[0].Groups[1].Value

Write-Host "=== Quake II RTX Head Tracking - Package Release ===" -ForegroundColor Magenta
Write-Host "Version: $version" -ForegroundColor Cyan

$releaseDir = Join-Path $projectDir "release"
if (-not (Test-Path $releaseDir)) { New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null }

$asiPath = Join-Path $projectDir "bin/Release/QuakeIIRTXHeadTracking.asi"
if (-not (Test-Path $asiPath)) { throw "QuakeIIRTXHeadTracking.asi not found at: $asiPath (run pixi run build-release)" }
$iniPath = Join-Path $projectDir "config/HeadTracking.ini"
if (-not (Test-Path $iniPath)) { throw "config/HeadTracking.ini not found" }
$vendorAsiDir = Join-Path $projectDir "vendor/ultimate-asi-loader"
$scriptsDir = Join-Path $projectDir "scripts"
foreach ($s in @("install.cmd", "uninstall.cmd")) {
    if (-not (Test-Path (Join-Path $scriptsDir $s))) { throw "Required script not found: $s" }
}

# --- GitHub installer ZIP ---
Write-Host "--- GitHub installer ZIP ---" -ForegroundColor Yellow
$ghStagingDir = Join-Path $releaseDir "staging-github"
if (Test-Path $ghStagingDir) { Remove-Item -Recurse -Force $ghStagingDir }
New-Item -ItemType Directory -Path $ghStagingDir -Force | Out-Null

foreach ($s in @("install.cmd", "uninstall.cmd")) {
    Copy-Item (Join-Path $scriptsDir $s) -Destination $ghStagingDir -Force
}

$pluginsDir = Join-Path $ghStagingDir "plugins"
New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
Copy-Item $asiPath -Destination $pluginsDir -Force
Copy-Item $iniPath -Destination $pluginsDir -Force

$ghVendorDir = Join-Path $ghStagingDir "vendor/ultimate-asi-loader"
New-Item -ItemType Directory -Path $ghVendorDir -Force | Out-Null
# The loader's own licence travels with the loader binary. Missing is fatal, not
# skipped: this ZIP redistributes a third-party binary, and a silent omission
# here is a licence breach that every gate would still pass.
foreach ($vendorFile in @("dinput8.dll", "LICENSE", "README.md")) {
    $src = Join-Path $vendorAsiDir $vendorFile
    if (-not (Test-Path $src)) {
        throw "Required vendored file not found: vendor/ultimate-asi-loader/$vendorFile. Every published ZIP is a binary distribution and must carry it."
    }
    Copy-Item $src -Destination $ghVendorDir -Force
}

foreach ($doc in @("LICENSE", "THIRD-PARTY-NOTICES.md")) {
    $docPath = Join-Path $projectDir $doc
    if (-not (Test-Path $docPath)) {
        throw "Required notice file not found: $doc. Every published ZIP is a binary distribution and must carry it."
    }
    Copy-Item $docPath -Destination $ghStagingDir -Force
}
foreach ($doc in @("README.md", "CHANGELOG.md")) {
    $docPath = Join-Path $projectDir $doc
    if (Test-Path $docPath) { Copy-Item $docPath -Destination $ghStagingDir -Force }
}

# Launcher manifest: the contract the launcher reads at the ZIP root. Stamp
# the real release version from version.h so the shipped manifest always
# matches the built binary regardless of what is committed.
$launcherManifestPath = Join-Path $projectDir "launcher-manifest.json"
if (-not (Test-Path $launcherManifestPath)) { throw "launcher-manifest.json not found at: $launcherManifestPath" }
$launcherManifest = Get-Content $launcherManifestPath -Raw | ConvertFrom-Json
$launcherManifest.mod_info.version = $version
$launcherManifest | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $ghStagingDir "launcher-manifest.json") -NoNewline
Write-Host "  launcher-manifest.json (version $version)" -ForegroundColor Green

Copy-SharedBundle -StagingDir $ghStagingDir

$ghZipPath = Join-Path $releaseDir "QuakeIIRTXHeadTracking-v$version-installer.zip"
if (Test-Path $ghZipPath) { Remove-Item $ghZipPath -Force }
Push-Location $ghStagingDir
try { Compress-Archive -Path ".\*" -DestinationPath $ghZipPath -Force } finally { Pop-Location }
Remove-Item -Recurse -Force $ghStagingDir
Write-Host ("  {0} ({1:N0} KB)" -f $ghZipPath, ((Get-Item $ghZipPath).Length / 1KB)) -ForegroundColor Green

Write-Host ""
Write-Host "=== Package Complete ===" -ForegroundColor Magenta
Write-Output $ghZipPath
