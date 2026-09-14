param([string]$Archive = "")

$ErrorActionPreference = "Stop"
$ReleaseTag = "dawn-sdk-be6033b"
$AssetName = "raygpu-dawn-sdk-windows-x64-be6033b.zip"
$ArchiveSha256 = "812BD76BFD1A4339693BA041F8D792E254E470C2A9A8FA70D95BA0E4F6C906F6"
$LibrarySha256 = "4F7DB86E1C113C7C4CA3BD5C2B8B2F4B65A2A6727FBF8138A5148D53229C71B6"
$DownloadUrl = "https://github.com/Sargonel/RayGPU/releases/download/$ReleaseTag/$AssetName"
$DependencyRoot = Join-Path $PSScriptRoot ".deps"
$SdkPath = Join-Path $DependencyRoot "dawn-sdk"
$LibraryPath = Join-Path $SdkPath "lib\webgpu_dawn.lib"
$HeaderPath = Join-Path $SdkPath "include\webgpu\webgpu.h"

if ($env:OS -ne "Windows_NT" -or -not [Environment]::Is64BitOperatingSystem) {
    throw "This Dawn SDK package supports Windows x64 only."
}

if ((Test-Path -LiteralPath $LibraryPath) -and (Test-Path -LiteralPath $HeaderPath)) {
    $InstalledHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $LibraryPath).Hash
    if ($InstalledHash -eq $LibrarySha256) {
        Write-Host "RayGPU Dawn SDK is already installed and verified."
        Write-Host "Run: make run"
        exit 0
    }
}

New-Item -ItemType Directory -Path $DependencyRoot -Force | Out-Null
$DownloadPath = Join-Path ([System.IO.Path]::GetTempPath()) $AssetName
$OwnsDownload = -not $Archive
$ExtractPath = Join-Path $DependencyRoot ("dawn-sdk-install-" + [Guid]::NewGuid().ToString("N"))

try {
    if ($Archive) {
        $DownloadPath = (Resolve-Path -LiteralPath $Archive).Path
        Write-Host "Using local Dawn SDK archive: $DownloadPath"
    }
    else {
        Write-Host "Downloading RayGPU Dawn SDK..."
        Invoke-WebRequest -Uri $DownloadUrl -OutFile $DownloadPath
    }

    $ActualArchiveHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $DownloadPath).Hash
    if ($ActualArchiveHash -ne $ArchiveSha256) {
        throw "Dawn SDK archive checksum mismatch. Expected $ArchiveSha256, got $ActualArchiveHash."
    }

    Expand-Archive -LiteralPath $DownloadPath -DestinationPath $ExtractPath -Force
    $ExtractedSdk = Join-Path $ExtractPath "dawn-sdk"
    $ExtractedLibrary = Join-Path $ExtractedSdk "lib\webgpu_dawn.lib"
    $ExtractedHeader = Join-Path $ExtractedSdk "include\webgpu\webgpu.h"
    if (-not (Test-Path -LiteralPath $ExtractedLibrary) -or -not (Test-Path -LiteralPath $ExtractedHeader)) {
        throw "The Dawn SDK archive does not contain the expected headers and library."
    }
    $ActualLibraryHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $ExtractedLibrary).Hash
    if ($ActualLibraryHash -ne $LibrarySha256) {
        throw "Dawn library checksum mismatch. Expected $LibrarySha256, got $ActualLibraryHash."
    }

    if (Test-Path -LiteralPath $SdkPath) { Remove-Item -LiteralPath $SdkPath -Recurse -Force }
    Move-Item -LiteralPath $ExtractedSdk -Destination $SdkPath
    Write-Host "RayGPU Dawn SDK installed to $SdkPath"
    Write-Host "Run: make run"
}
finally {
    if (Test-Path -LiteralPath $ExtractPath) { Remove-Item -LiteralPath $ExtractPath -Recurse -Force }
    if ($OwnsDownload -and (Test-Path -LiteralPath $DownloadPath)) { Remove-Item -LiteralPath $DownloadPath -Force }
}
