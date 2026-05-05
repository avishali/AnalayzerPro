param(
    [string]$BuildDir = "build-release-windows-x64",
    [string]$Config = "Release",
    [string]$PluginName = "AnalyzerPro",
    [string]$CertThumbprint = "",
    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($CertThumbprint)) {
    Write-Error "CertThumbprint is required. Example: .\scripts\release_sign_windows.ps1 -CertThumbprint <thumbprint>"
}

$artifactsDir = Join-Path $BuildDir "$($PluginName)_artefacts\$Config"
if (!(Test-Path $artifactsDir)) {
    Write-Error "Artifacts directory not found: $artifactsDir"
}

function Sign-Target {
    param([string]$Path)
    if (Test-Path $Path) {
        Write-Host "Signing: $Path"
        signtool sign /sha1 $CertThumbprint /fd SHA256 /tr $TimestampUrl /td SHA256 "$Path"
    }
    else {
        Write-Host "Skip (not found): $Path"
    }
}

$standaloneExe = Join-Path $artifactsDir "Standalone\$PluginName.exe"
$vst3Dll = Join-Path $artifactsDir "VST3\$PluginName.vst3\Contents\x86_64-win\$PluginName.vst3"
$aaxDll = Join-Path $artifactsDir "AAX\$PluginName.aaxplugin\Contents\x64\$PluginName.aaxplugin"

Sign-Target -Path $standaloneExe
Sign-Target -Path $vst3Dll
Sign-Target -Path $aaxDll

Write-Host ""
Write-Host "Windows signing complete."
Write-Host "Note: AAX still requires final PACE/Avid distribution signing workflow."
