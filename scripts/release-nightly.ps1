#!/usr/bin/env pwsh
#Requires -Version 5.1
# Thin shim. Determine version, delegate to the shared publisher.
# See cameraunlock-core/powershell/NightlyRelease.psm1 for what it does.

[CmdletBinding()]
param(
    [switch]$AllowDirty
)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $ProjectRoot 'cameraunlock-core\powershell\NightlyRelease.psm1') -Force

$versionFile = Join-Path $ProjectRoot 'src\version.h'
$versionMatch = Select-String -Path $versionFile -Pattern 'MOD_VERSION\s*=\s*"([^"]+)"'
if (-not $versionMatch) {
    throw "Could not extract version from $versionFile"
}
$version = $versionMatch.Matches[0].Groups[1].Value

Publish-NightlyBuild `
    -ModId 'quake-ii-rtx' `
    -ModName 'QuakeIIRTXHeadTracking' `
    -Version $version `
    -ProjectRoot $ProjectRoot `
    -NoNexusZip `
    -AllowDirty:$AllowDirty
