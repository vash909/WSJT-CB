param(
  [string]$BuildDir = "build",
  [string]$DistDir = "dist",
  [int]$Jobs = [Environment]::ProcessorCount,
  [string]$MsysBash = "C:\msys64\usr\bin\bash.exe",
  [string]$InnoSetupCompiler = "",
  [switch]$SkipInstaller,
  [switch]$RunTests,
  [switch]$Clean
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $RepoRoot

function Resolve-RepoPath {
  param([string]$Path)

  if ([System.IO.Path]::IsPathRooted($Path)) {
    return [System.IO.Path]::GetFullPath($Path)
  }

  return [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $Path))
}

function Assert-UnderRepo {
  param([string]$Path)

  $full = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
  $root = [System.IO.Path]::GetFullPath($RepoRoot).TrimEnd('\')
  if ($full -ne $root -and -not $full.StartsWith($root + "\", [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to operate outside the repository: $full"
  }
}

function ConvertTo-MsysPath {
  param([string]$Path)

  $full = [System.IO.Path]::GetFullPath($Path).Replace('\', '/')
  if ($full -match '^([A-Za-z]):/(.*)$') {
    return "/" + $Matches[1].ToLowerInvariant() + "/" + $Matches[2]
  }

  return $full
}

function Invoke-Msys {
  param(
    [string]$Step,
    [string]$Command
  )

  Write-Host ""
  Write-Host "==> $Step"
  & $MsysBash -lc "export MSYSTEM=UCRT64; export PATH=/ucrt64/bin:/usr/local/bin:/usr/bin:/bin:`$PATH; export PKG_CONFIG_PATH=/ucrt64/lib/pkgconfig; $Command"
  if ($LASTEXITCODE -ne 0) {
    throw "$Step failed with exit code $LASTEXITCODE"
  }
}

function Find-InnoSetupCompiler {
  if ($InnoSetupCompiler) {
    if (-not (Test-Path -LiteralPath $InnoSetupCompiler)) {
      throw "Inno Setup compiler not found: $InnoSetupCompiler"
    }
    return $InnoSetupCompiler
  }

  $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
  if ($command) {
    return $command.Source
  }

  $candidates = @(
    (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
  )

  foreach ($candidate in $candidates) {
    if (Test-Path -LiteralPath $candidate) {
      return $candidate
    }
  }

  throw "Inno Setup compiler not found. Install Inno Setup 6 or pass -InnoSetupCompiler."
}

$BuildPath = Resolve-RepoPath $BuildDir
$DistPath = Resolve-RepoPath $DistDir
$InstallerOutputPath = Resolve-RepoPath "installer-output"

Assert-UnderRepo $BuildPath
Assert-UnderRepo $DistPath
Assert-UnderRepo $InstallerOutputPath

if (-not (Test-Path -LiteralPath $MsysBash)) {
  throw "MSYS2 bash not found: $MsysBash"
}

if ($Clean) {
  foreach ($path in @($BuildPath, $DistPath, $InstallerOutputPath)) {
    if (Test-Path -LiteralPath $path) {
      Write-Host "Removing $path"
      Remove-Item -LiteralPath $path -Recurse -Force
    }
  }
}

$SourceMsys = ConvertTo-MsysPath $RepoRoot
$BuildMsys = ConvertTo-MsysPath $BuildPath
$DistMsys = ConvertTo-MsysPath $DistPath

$configureCommand = @(
  "cmake",
  "-S '$SourceMsys'",
  "-B '$BuildMsys'",
  "-G Ninja",
  "-DCMAKE_BUILD_TYPE=Release",
  "-DCMAKE_INSTALL_PREFIX='$DistMsys'",
  "-DWSJT_SKIP_MANPAGES=ON",
  "-DWSJT_GENERATE_DOCS=OFF",
  "-DWSJT_ENABLE_WERROR=OFF",
  "-DWSJT_WITH_OMNIRIG=ON",
  "-DWSJT_SKIP_BUNDLE_FIXUP=ON",
  "-DCMAKE_DISABLE_FIND_PACKAGE_OpenMP=FALSE",
  "-DWSJT_BUILD_DECODER_TESTS=$(if ($RunTests) { 'ON' } else { 'OFF' })",
  "-DWSJT_BUILD_PATH_TESTS=$(if ($RunTests) { 'ON' } else { 'OFF' })",
  "-DQt5_DIR=/ucrt64/lib/cmake/Qt5",
  "-DCMAKE_PREFIX_PATH=/ucrt64"
) -join " "

Invoke-Msys -Step "Configure CMake for Windows/UCRT64 with OmniRig" -Command $configureCommand
Invoke-Msys -Step "Build WSJT-CB" -Command "cmake --build '$BuildMsys' --config Release --parallel $Jobs"
if ($RunTests) {
  Invoke-Msys -Step "Run regression tests" -Command "ctest --test-dir '$BuildMsys' --output-on-failure"
}
Invoke-Msys -Step "Install staged files into dist" -Command "cmake --install '$BuildMsys' --config Release"

$deployCommand = @'
set -euo pipefail

app="__DIST__/bin/wsjtcb.exe"
dest="__DIST__/bin"
ldd_log="__BUILD__/ldd-wsjtcb.txt"

if [ ! -f "$app" ]; then
  echo "Executable not found: $app" >&2
  exit 1
fi

copy_deps() {
  local file="$1"

  ldd "$file" 2>/dev/null | awk '
    /=>/ && $3 ~ /^\// { print $3 }
    /^[[:space:]]*\// { print $1 }
  ' | while IFS= read -r dep; do
    case "$dep" in
      /ucrt64/bin/*.dll|/mingw64/bin/*.dll)
        local base
        base="$(basename "$dep")"
        if [ ! -f "$dest/$base" ]; then
          cp -p "$dep" "$dest/$base"
          copy_deps "$dest/$base"
        fi
        ;;
    esac
  done
}

# Qt can load OpenSSL dynamically, so it is not always visible to ldd.
cp -p /ucrt64/bin/libssl-*.dll /ucrt64/bin/libcrypto-*.dll "$dest/"
copy_deps "$app"
while IFS= read -r -d '' dll; do
  copy_deps "$dll"
done < <(find "__DIST__" -type f \( -iname '*.dll' -o -iname '*.exe' \) -print0)

ldd "$app" | tee "$ldd_log"
if grep -i "not found" "$ldd_log"; then
  echo "Unresolved runtime dependencies detected. See $ldd_log" >&2
  exit 1
fi
'@

$deployCommand = $deployCommand.Replace("__DIST__", $DistMsys).Replace("__BUILD__", $BuildMsys)
$deployScriptPath = Join-Path $BuildPath "deploy-runtime-deps.sh"
$deployScriptMsys = ConvertTo-MsysPath $deployScriptPath
Set-Content -LiteralPath $deployScriptPath -Value $deployCommand -Encoding ASCII
Invoke-Msys -Step "Copy and verify MSYS2 runtime DLLs" -Command "bash '$deployScriptMsys'"

if (-not $SkipInstaller) {
  $iscc = Find-InnoSetupCompiler
  Write-Host ""
  Write-Host "==> Build Inno Setup installer"
  & $iscc "/DDistDir=$DistPath" (Join-Path $RepoRoot "wsjtcb_dist_x64.iss")
  if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE"
  }

  $versionLine = Select-String -Path (Join-Path $RepoRoot "wsjtcb_dist_x64.iss") -Pattern '#define AppVersion "([^"]+)"' | Select-Object -First 1
  $version = $versionLine.Matches[0].Groups[1].Value
  $installer = Join-Path $InstallerOutputPath "wsjtcb-$version-win64-setup.exe"
  if (-not (Test-Path -LiteralPath $installer)) {
    throw "Expected installer was not created: $installer"
  }

  Write-Host ""
  Write-Host "Installer created: $installer"
} else {
  Write-Host ""
  Write-Host "Installer skipped. Staged application is in: $DistPath"
}
