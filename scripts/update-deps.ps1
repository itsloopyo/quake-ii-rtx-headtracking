#!/usr/bin/env pwsh
#Requires -Version 5.1
# Bump vendored Ultimate ASI Loader (dinput8.dll) to the latest upstream within
# the pinned range and rewrite vendor/ultimate-asi-loader/{LICENSE,README.md}.
# Manual: dev runs this for a fresh upstream bump, then commits. CI never refreshes.
# See ~/.claude/CLAUDE.md "Vendoring Third-Party Dependencies".
#
# Ultimate-ASI-Loader ships a DLL inside a release zip, not as a standalone
# asset. We extract dinput8.dll and vendor it directly so install.cmd can copy
# it into the game's exe dir as the configured hook slot (winmm.dll for Q2RTX).

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

$scriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

$module = Join-Path $projectDir 'cameraunlock-core/powershell/ModLoaderSetup.psm1'
if (-not (Test-Path $module)) {
    throw "ModLoaderSetup.psm1 not found at $module. Run 'pixi run sync' to update the cameraunlock-core submodule."
}
Import-Module $module -Force

$vendorAsiDir = Join-Path $projectDir 'vendor/ultimate-asi-loader'
$vendorAsiDll = Join-Path $vendorAsiDir 'dinput8.dll'
if (-not (Test-Path $vendorAsiDir)) {
    New-Item -ItemType Directory -Path $vendorAsiDir -Force | Out-Null
}

$tempZip = Join-Path $env:TEMP ("asi-update-" + [IO.Path]::GetRandomFileName() + ".zip")
try {
    Write-Host "Refreshing vendor/ultimate-asi-loader from upstream..." -ForegroundColor Cyan
    $meta = Invoke-FetchLatestLoader `
        -OutputPath $tempZip `
        -Owner 'ThirteenAG' -Repo 'Ultimate-ASI-Loader' `
        -VersionPrefix 'v9.' `
        -AssetPattern '^Ultimate-ASI-Loader_x64\.zip$'

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($tempZip)
    try {
        $dllEntry = $zip.Entries | Where-Object { $_.Name -eq 'dinput8.dll' } | Select-Object -First 1
        if (-not $dllEntry) { throw "Upstream zip $($meta.AssetName) does not contain dinput8.dll." }
        # Staged, not written in place. File::Create truncates as its first act,
        # so a throw part way through the copy (a corrupt entry, a full disk, a
        # scanner holding the file) used to leave the vendored loader empty with
        # no backup, and the packager only tests Test-Path before shipping it.
        $stagedDll = "$vendorAsiDll.new"
        $out = [System.IO.File]::Create($stagedDll)
        try { $in = $dllEntry.Open(); try { $in.CopyTo($out) } finally { $in.Dispose() } } finally { $out.Dispose() }
        if ((Get-Item $stagedDll).Length -eq 0) {
            Remove-Item $stagedDll -Force
            throw "Upstream dinput8.dll extracted as an empty file; the vendored loader is unchanged."
        }
        Move-Item -Path $stagedDll -Destination $vendorAsiDll -Force

        $licenseEntry = $zip.Entries | Where-Object { $_.Name -match '^(license|LICENSE)(\..+)?$' -and $_.FullName -notmatch '/.+/' } | Select-Object -First 1
        if ($licenseEntry) {
            $out = [System.IO.File]::Create((Join-Path $vendorAsiDir 'LICENSE'))
            try { $in = $licenseEntry.Open(); try { $in.CopyTo($out) } finally { $in.Dispose() } } finally { $out.Dispose() }
        }
    } finally { $zip.Dispose() }

    if (-not (Test-Path (Join-Path $vendorAsiDir 'LICENSE'))) {
        $licenseUrl = "https://raw.githubusercontent.com/ThirteenAG/Ultimate-ASI-Loader/$($meta.Tag)/license"
        Invoke-WebRequest -Uri $licenseUrl -OutFile (Join-Path $vendorAsiDir 'LICENSE') -UseBasicParsing -TimeoutSec 30 -Headers @{ "User-Agent" = "CameraUnlock-HeadTracking" }
    }

    $noBom = New-Object System.Text.UTF8Encoding $false
    $dllSha = (Get-FileHash -Path $vendorAsiDll -Algorithm SHA256).Hash.ToLower()
    $readme = @(
        '# Ultimate ASI Loader (vendored)',
        '',
        'Bundled copy of Ultimate ASI Loader, the install-time source of truth.',
        'Refresh manually with `pixi run update-deps`, then commit.',
        '',
        '## Snapshot',
        '',
        '- Upstream: https://github.com/ThirteenAG/Ultimate-ASI-Loader',
        "- Tag: ``$($meta.Tag)``",
        "- Commit: ``$($meta.CommitSha)``",
        "- Asset: ``$($meta.AssetName)``",
        "- dinput8.dll SHA-256: ``$dllSha``",
        "- Fetched at: $($meta.FetchedAt)",
        '',
        '`dinput8.dll` is extracted from the upstream asset untouched. install.cmd copies it to',
        'the Quake II RTX exe dir as `winmm.dll` (the hook slot q2rtx.exe loads ASI plugins through).'
    ) -join "`n"
    [System.IO.File]::WriteAllText((Join-Path $vendorAsiDir 'README.md'), $readme + "`n", $noBom)

    # THIRD-PARTY-NOTICES.md publishes this version, commit and checksum, and
    # install.cmd writes the version into the launcher's state file. Both ship,
    # and both had already drifted from the vendored binary once by hand.
    $noticesPath = Join-Path $projectDir 'THIRD-PARTY-NOTICES.md'
    $notices = Get-Content $noticesPath -Raw
    $notices = $notices -replace '(?m)^- Version: `v[0-9][^`]*`$', "- Version: ``$($meta.Tag)``"
    $notices = $notices -replace '(?m)^- Commit: `[0-9a-f]{40}`$', "- Commit: ``$($meta.CommitSha)``"
    $notices = $notices -replace '(?m)^- SHA-256: `[0-9a-f]{64}`$', "- SHA-256: ``$dllSha``"
    # Every remaining mention of the LOADER's own version: the two transitive
    # table rows, the premake sentence, and the injector/miniz "as vendored in"
    # lines. Missing these left the notices claiming two loader versions at once.
    $notices = $notices -replace '(?<=Ultimate ASI Loader )v[0-9]+(\.[0-9]+)*', $meta.Tag
    $notices = $notices -replace '(?<=premake5\.lua` at )v[0-9]+(\.[0-9]+)*', $meta.Tag
    $notices = $notices -replace '\| Ultimate ASI Loader \| v[0-9][0-9.]* \|', "| Ultimate ASI Loader | $($meta.Tag) |"
    # WriteAllText with a BOM-less encoder, not Set-Content -Encoding UTF8:
    # Windows PowerShell 5.1 writes a BOM for that encoding, and a BOM in front
    # of install.cmd's first line stops cmd.exe running it at all.
    [System.IO.File]::WriteAllText($noticesPath, $notices, $noBom)

    $installPath = Join-Path $projectDir 'scripts/install.cmd'
    $install = Get-Content $installPath -Raw
    $install = $install -replace '(?m)^(set "ASI_LOADER_VERSION=)[^"]*(")', "`${1}$($meta.Tag -replace '^v','')`${2}"
    [System.IO.File]::WriteAllText($installPath, $install, $noBom)

    Write-Host "  tag=$($meta.Tag) sha256=$($dllSha.Substring(0,12))..." -ForegroundColor DarkGray
    Write-Host "  THIRD-PARTY-NOTICES.md and install.cmd updated to match." -ForegroundColor DarkGray
    Write-Warning "The transitive components (injector, miniz) are pinned by the tag in THIRD-PARTY-NOTICES.md. Re-check them against the new tag by hand; this script cannot read the upstream submodule pins."

} finally {
    Remove-Item $tempZip -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "vendor/ultimate-asi-loader refreshed. Review and commit." -ForegroundColor Green
