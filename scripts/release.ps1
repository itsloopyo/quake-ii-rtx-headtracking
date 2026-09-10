#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
.SYNOPSIS
    Automated release workflow for Quake II RTX Head Tracking.
.DESCRIPTION
    Bumps the version in src/version.h + CMakeLists.txt + scripts/install.cmd,
    builds Release, regenerates CHANGELOG, commits, tags, and pushes (CI then
    builds and publishes the GitHub release).
.NOTES
    Run via: pixi run release <major|minor|patch|nightly|X.Y.Z>
#>
param(
    [Parameter(Position=0)]
    [string]$Version = "",
    # Ship a release even when there are no user-facing commits since the
    # last tag (writes a maintenance changelog entry instead of aborting).
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Ahead of the notices sync below, which commits on this repo's behalf: a
# nightly publishes an existing commit and must not create one.
if ($Version -eq 'nightly') {
    & (Join-Path $PSScriptRoot 'release-nightly.ps1')
    exit $LASTEXITCODE
}

# THIRD-PARTY-NOTICES.md names the cameraunlock-core commit compiled into the
# release ZIPs, and bumping the submodule does not touch it. Packaging refuses
# to ship that mismatch, so a bump with no notices edit stopped the release
# here, or in CI once the tag had already been pushed. Re-sync it and let this
# release carry the correction.
$noticesRoot = Split-Path -Parent $PSScriptRoot
& git -C $noticesRoot diff --quiet -- THIRD-PARTY-NOTICES.md
if ($LASTEXITCODE -ne 0) { throw "THIRD-PARTY-NOTICES.md has uncommitted edits. Commit or discard them, then re-run." }
& (Join-Path $noticesRoot 'cameraunlock-core\scripts\sync-core-notices.ps1') -Repo $noticesRoot
if ($LASTEXITCODE -ne 0) { throw "sync-core-notices.ps1 exited $LASTEXITCODE - fix THIRD-PARTY-NOTICES.md before releasing." }
& git -C $noticesRoot diff --quiet -- THIRD-PARTY-NOTICES.md
if ($LASTEXITCODE -ne 0) {
    & git -C $noticesRoot commit -q -m 'chore: record the cameraunlock-core commit this build compiles' -- THIRD-PARTY-NOTICES.md
    if ($LASTEXITCODE -ne 0) { throw "Could not commit the re-synced THIRD-PARTY-NOTICES.md." }
    Write-Host 'THIRD-PARTY-NOTICES.md re-synced to the pinned cameraunlock-core commit.' -ForegroundColor Yellow
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir
$versionHeader = Join-Path $projectDir "src\version.h"

Import-Module (Join-Path $projectDir "cameraunlock-core\powershell\ReleaseWorkflow.psm1") -Force

# Mirrors New-ChangelogFromCommits' insertion so a -Force maintenance entry
# lands in the same place with the same shape.
function Add-MaintenanceChangelogEntry {
    param([string]$Path, [string]$NewVersion)
    $date = Get-Date -Format 'yyyy-MM-dd'
    $entry = "## [$NewVersion] - $date`n`n### Changed`n`n- Maintenance release (no user-facing changes).`n`n"
    $changelog = Get-Content $Path -Raw
    if ($changelog -match '(?s)(# Changelog.*?)(## \[)') {
        $changelog = $changelog -replace '(?s)(# Changelog.*?\n\n)', "`$1$entry"
    } else {
        $changelog = $changelog -replace '(?s)(# Changelog.*?\n)', "`$1$entry"
    }
    $changelog = $changelog.TrimEnd() + "`n"
    Set-Content $Path $changelog -NoNewline
}

function Get-CurrentVersion {
    $line = Select-String -Path $versionHeader -Pattern 'MOD_VERSION\s*=\s*"([0-9]+\.[0-9]+\.[0-9]+)"'
    if (-not $line) { throw "MOD_VERSION not found in src/version.h" }
    return $line.Matches[0].Groups[1].Value
}

function Set-Version {
    param([string]$NewVersion)
    (Get-Content $versionHeader -Raw) -replace 'MOD_VERSION\s*=\s*"[0-9]+\.[0-9]+\.[0-9]+"', "MOD_VERSION = `"$NewVersion`"" | Set-Content $versionHeader -NoNewline
    $cmakePath = Join-Path $projectDir "CMakeLists.txt"
    (Get-Content $cmakePath -Raw) -replace 'project\(QuakeIIRTXHeadTracking VERSION [0-9]+\.[0-9]+\.[0-9]+', "project(QuakeIIRTXHeadTracking VERSION $NewVersion" | Set-Content $cmakePath -NoNewline
    $installCmdPath = Join-Path $scriptDir "install.cmd"
    (Get-Content $installCmdPath -Raw) -replace 'set "MOD_VERSION=.*?"', "set `"MOD_VERSION=$NewVersion`"" | Set-Content $installCmdPath -NoNewline
    # Regex on the raw text, not ConvertFrom-Json | ConvertTo-Json: the round
    # trip reformats the entire file (Windows PowerShell 5.1 puts two spaces
    # after every colon), which rewrites the base64 config seed's surroundings
    # and every other line for a three-character change.
    $manifestPath = Join-Path $projectDir "launcher-manifest.json"
    (Get-Content $manifestPath -Raw) -replace '("version"\s*:\s*")[0-9]+\.[0-9]+\.[0-9]+"', "`${1}$NewVersion`"" | Set-Content $manifestPath -NoNewline
    # pixi.toml is not read by the release workflow today, which is exactly why
    # it would have sat at 0.0.0 unnoticed: version-source: pixi is a supported
    # mode, and switching to it would then publish every build as 0.0.0.
    $pixiPath = Join-Path $projectDir "pixi.toml"
    (Get-Content $pixiPath -Raw) -replace '(?m)^version = "[0-9]+\.[0-9]+\.[0-9]+"', "version = `"$NewVersion`"" | Set-Content $pixiPath -NoNewline
}

Write-Host "=== Quake II RTX Head Tracking Release ===" -ForegroundColor Cyan
$currentVersion = Get-CurrentVersion

if ([string]::IsNullOrWhiteSpace($Version)) {
    Write-Host "Current version: $currentVersion" -ForegroundColor White
    Write-Host "Usage: pixi run release <major|minor|patch|nightly|X.Y.Z>" -ForegroundColor Yellow
    exit 0
}

try {
    $Version = Resolve-ReleaseVersion -Argument $Version -CurrentVersion $currentVersion
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

$tagName = "v$Version"

$currentBranch = git rev-parse --abbrev-ref HEAD
if ($currentBranch -ne "main") {
    Write-Host "Error: Must be on 'main' branch to release (currently on '$currentBranch')" -ForegroundColor Red
    exit 1
}
if (git status --porcelain) {
    Write-Host "Error: Working directory has uncommitted changes" -ForegroundColor Red
    exit 1
}
if (git tag -l $tagName) {
    Write-Host "Error: Tag '$tagName' already exists" -ForegroundColor Red
    exit 1
}

Write-Host "Current version: $currentVersion -> $Version" -ForegroundColor Green

# Generate CHANGELOG from commits since last tag. This is the gate that
# aborts when there are no user-facing commits, so run it BEFORE mutating
# any version files or building - a failure here then leaves a clean tree
# instead of stranding a half-applied version bump with no tag.
Write-Host "Generating CHANGELOG..." -ForegroundColor Cyan
$changelogPath = Join-Path $projectDir "CHANGELOG.md"
if (-not (git tag -l 2>$null)) {
    # No tags yet, so there is no range to generate from. Write a stub only when
    # nothing has been written by hand: overwriting would throw away the first
    # release's notes, which are the ones that ship inside the installer ZIP and
    # become the GitHub release body.
    $date = Get-Date -Format 'yyyy-MM-dd'
    $existing = if (Test-Path $changelogPath) { (Get-Content $changelogPath -Raw).Trim() } else { "" }
    if ($existing -match '(?m)^##\s') {
        Write-Host "  keeping the CHANGELOG entry already written for this release." -ForegroundColor DarkGray
    } else {
        Set-Content $changelogPath "# Changelog`n`n## [$Version] - $date`n`nFirst release.`n"
    }
} else {
    try {
        New-ChangelogFromCommits -ChangelogPath $changelogPath -Version $Version -ArtifactPaths @(
            "src/", "cameraunlock-core/", "scripts/install.cmd", "scripts/uninstall.cmd"
        )
    } catch {
        if (-not $Force) {
            Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
            Write-Host "No user-facing changes to release. Re-run with -Force for a maintenance release." -ForegroundColor Yellow
            exit 1
        }
        Write-Host "No user-facing commits since last tag - writing maintenance entry (-Force)." -ForegroundColor Yellow
        Add-MaintenanceChangelogEntry -Path $changelogPath -NewVersion $Version
    }
}

# Every file this run has already modified, so a later failure can put the tree
# back the way it found it.
$versionedFiles = @(
    $versionHeader,
    (Join-Path $projectDir "CMakeLists.txt"),
    (Join-Path $scriptDir "install.cmd"),
    (Join-Path $projectDir "launcher-manifest.json"),
    (Join-Path $projectDir "pixi.toml"),
    $changelogPath
)

Set-Version $Version

# The bump comes first so the binary this build produces carries the version
# being released, and the build's failure path undoes it. Leaving the bump
# applied stranded six modified files with no tag, and the next run then aborted
# on the uncommitted-changes check without saying which six to revert.
# Two gates, neither of which the release path used to run. `test` is what
# compares the three copies of the config document - the shipped ini,
# Config::WriteDefault and the manifest's base64 loader.seed - so a
# HeadTracking.ini edit that was not carried into the seed cannot tag and
# publish a stale config. `validate-manifest` chains through package and checks
# the built ZIP against the manifest's own file rows, which is a different
# question and the one that catches a payload path that has moved.
Write-Host "Testing and packaging Release configuration..." -ForegroundColor Cyan
& pixi run test
if ($LASTEXITCODE -eq 0) { & pixi run validate-manifest }
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: the release build gate failed; reverting the version bump" -ForegroundColor Red
    & git checkout -- $versionedFiles
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Warning: could not revert. These files are modified: $($versionedFiles -join ', ')" -ForegroundColor Yellow
    }
    exit 1
}

# $ErrorActionPreference does not apply to native commands, so every one of
# these is checked. An unchecked push is the dangerous one: a rejected
# non-fast-forward push to main followed by a successful tag push publishes a
# release built from a commit that is not on the branch.
function Invoke-GitStep {
    param([string[]]$Arguments, [string]$What)
    & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: $What failed (git exited $LASTEXITCODE)" -ForegroundColor Red
        exit 1
    }
}
Invoke-GitStep -Arguments (@('add') + $versionedFiles) -What "staging the version bump"
Invoke-GitStep -Arguments @('commit', '-m', "Release v$Version") -What "committing the version bump"
# Annotated, not lightweight: a lightweight tag carries no tagger or date, so
# git describe and tag listings behave differently here than across the fleet.
Invoke-GitStep -Arguments @('tag', '-a', $tagName, '-m', "Release v$Version") -What "tagging $tagName"
Invoke-GitStep -Arguments @('push', 'origin', 'main') -What "pushing main"
Invoke-GitStep -Arguments @('push', 'origin', $tagName) -What "pushing tag $tagName"

Write-Host ""
Write-Host "Release $tagName initiated. CI will build and publish." -ForegroundColor Green
