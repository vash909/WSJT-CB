$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$installer = (Resolve-Path 'installer-output/wsjtcb-1.4.1-win64-setup.exe').Path
$installDir = Join-Path $env:RUNNER_TEMP 'wsjtcb-installed'
$installLog = Join-Path $PWD 'installer-output/install.log'
$setup = Start-Process -FilePath $installer -ArgumentList @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$installDir`"", "/LOG=`"$installLog`"") -Wait -PassThru
if ($setup.ExitCode -ne 0) { throw "Installer failed: $($setup.ExitCode)" }
$exe = Join-Path $installDir 'bin/wsjtcb.exe'
$version = (Get-Item $exe).VersionInfo.ProductVersion
if ($version -notmatch '^1\.4\.1') { throw "Unexpected installed version: $version" }
if (-not (Test-Path (Join-Path $installDir 'share/wsjtcb/cty.dat'))) { throw 'Country data missing' }

# Remove all build-tool DLL directories: the installed package must be sufficient.
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\Wbem"
Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue
Remove-Item Env:QTDIR -ErrorAction SilentlyContinue
$process = Start-Process -FilePath $exe -ArgumentList @('-r', 'CI') -PassThru `
  -RedirectStandardOutput 'installer-output/application.stdout.log' `
  -RedirectStandardError 'installer-output/application.stderr.log'
try {
  for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Seconds 1
    $process.Refresh()
    if ($process.HasExited) { throw "Installed application exited: $($process.ExitCode)" }
  }
  if ($process.MainWindowHandle -eq 0) { throw 'Application window missing' }
  if ($process.MainWindowTitle -notmatch '1\.4\.1') { throw "Unexpected window: $($process.MainWindowTitle)" }
  $children = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($process.Id)")
  if (-not ($children | Where-Object { $_.Name -eq 'jt9.exe' })) { throw 'Installed decoder is not running' }
  if (-not (Test-Path (Join-Path $env:LOCALAPPDATA 'WSJT-CB - CI'))) { throw 'Stable data directory missing' }
  if (Test-Path (Join-Path $env:LOCALAPPDATA 'WSJT-CB by 1AT106 1XZ732 1AT1989 - CI')) { throw 'Old branded data directory recreated' }
  'PASS: installed 1.4.1 GUI and decoder ran for 40 seconds without build DLL paths; stable data folder verified' |
    Set-Content 'installer-output/installation-test.txt'
} finally {
  if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
  Get-CimInstance Win32_Process -Filter "ParentProcessId=$($process.Id)" |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
}
git rev-parse HEAD | Set-Content 'installer-output/SOURCE_COMMIT.txt'
$hash = (Get-FileHash -Algorithm SHA256 $installer).Hash.ToLowerInvariant()
"$hash  wsjtcb-1.4.1-win64-setup.exe" | Set-Content 'installer-output/SHA256SUMS.txt'
