$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-VerifiedDownload {
  param([string]$Url, [string]$Path, [string]$Sha256)
  Invoke-WebRequest -Uri $Url -OutFile $Path
  if ((Get-FileHash -Algorithm SHA256 $Path).Hash.ToLowerInvariant() -ne $Sha256) {
    throw "Download checksum mismatch: $Url"
  }
}

$omniZip = Join-Path $env:RUNNER_TEMP 'omnirig.zip'
Get-VerifiedDownload 'https://dxatlas.com/OmniRig/Files/OmniRig.zip' $omniZip `
  '46654ec3ba2fe9ff23a21edaa657c46e0e9867b62b254968a3c4436dd81531a8'
$omniSource = Join-Path $env:RUNNER_TEMP 'omnirig-setup'
Expand-Archive -Path $omniZip -DestinationPath $omniSource -Force
$omni = Start-Process (Join-Path $omniSource 'OmniRigSetup.exe') `
  -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -Wait -PassThru
if ($omni.ExitCode -ne 0) { throw "OmniRig setup failed: $($omni.ExitCode)" }

$innoExe = Join-Path $env:RUNNER_TEMP 'innosetup.exe'
Get-VerifiedDownload 'https://github.com/jrsoftware/issrc/releases/download/is-6_7_3/innosetup-6.7.3.exe' $innoExe `
  '9c73c3bae7ed48d44112a0f48e66742c00090bdb5bef71d9d3c056c66e97b732'
$innoDir = Join-Path $env:RUNNER_TEMP 'innosetup'
$inno = Start-Process $innoExe -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$innoDir`"") -Wait -PassThru
if ($inno.ExitCode -ne 0) { throw "Inno Setup installation failed: $($inno.ExitCode)" }
if (-not (Test-Path (Join-Path $innoDir 'ISCC.exe'))) { throw 'Inno Setup compiler missing' }
