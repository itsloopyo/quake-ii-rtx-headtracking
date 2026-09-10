#!/usr/bin/env pwsh
#Requires -Version 5.1
# Thin wrapper - dev-deploy orchestration lives in
# cameraunlock-core/powershell/DevDeploy.psm1.

param(
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,
    [Parameter(Mandatory=$false, Position=1)]
    [string]$GivenPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference = 'SilentlyContinue'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

Import-Module (Join-Path $projectRoot "cameraunlock-core\powershell\DevDeploy.psm1") -Force
Import-Module (Join-Path $projectRoot "cameraunlock-core\powershell\ModDeployment.psm1") -Force

$buildOutput = Join-Path $projectRoot "bin\$Configuration"
$configFile = Join-Path $projectRoot 'config\HeadTracking.ini'
$vendorLoader = Join-Path $projectRoot 'vendor\ultimate-asi-loader\dinput8.dll'

$result = Invoke-DevDeployASILoader `
    -GameId 'quake-ii-rtx' `
    -GameDisplayName 'Quake II RTX' `
    -BuildOutputPath $buildOutput `
    -ModDllName 'QuakeIIRTXHeadTracking.asi' `
    -ConfigFile $configFile `
    -VendorLoaderDll $vendorLoader `
    -AsiLoaderName 'winmm.dll' `
    -ExtraDlls @() `
    -GivenPath $GivenPath

Write-DeploymentSuccess `
    -ModName "Quake II RTX Head Tracking" `
    -DeployPath $result.DeployedDllPath `
    -Controls @(
        "End       - Toggle head tracking on/off",
        "Page Up   - Cycle tracking mode (6DOF / rotation only / position only)",
        "Page Down - Toggle yaw mode (world / local)",
        "",
        "No nav cluster? Chords: Ctrl+Shift+ Y=Toggle G=Mode H=Yaw"
    )
