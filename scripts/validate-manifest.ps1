#!/usr/bin/env pwsh
#Requires -Version 5.1
# Checks the built installer ZIP against launcher-manifest.json using
# cameraunlock-core's validator: every manifest source present in the ZIP, no
# absolute or ../ target, and nothing shipped that no manifest row deploys.
#
# A wrapper rather than a bare pixi cmd because the validator takes the ZIP
# path, not the directory, and picking the newest one needs a variable that
# pixi's own shell would try to expand.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

$zip = Get-ChildItem (Join-Path $projectDir 'release/*-installer.zip') |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $zip) {
    throw "No installer ZIP in release/. Run 'pixi run package' first."
}

$validator = Join-Path $projectDir 'cameraunlock-core/scripts/validate-manifest.mjs'
if (-not (Test-Path $validator)) {
    throw "validate-manifest.mjs not found at $validator. Run 'pixi run sync' to update the cameraunlock-core submodule."
}

& node $validator $zip.FullName
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
