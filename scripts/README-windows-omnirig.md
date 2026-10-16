# Windows build with OmniRig

This repository is configured for a Windows x64 build through MSYS2 UCRT64,
Qt 5, Hamlib, OmniRig and Inno Setup.

## Build and package

From a PowerShell prompt in the repository root:

```powershell
.\scripts\build-windows-omnirig.ps1
```

The script performs these steps:

1. Configures CMake/Ninja for MSYS2 UCRT64.
2. Builds `wsjtcb.exe` with `WSJT_WITH_OMNIRIG=ON`.
3. Installs the staged application into `dist`.
4. Copies the required MSYS2 runtime DLLs into `dist\bin`.
5. Builds the Inno Setup installer.

The installer is written to:

```text
installer-output\wsjtcb-1.4.1-win64-setup.exe
```

## Useful options

```powershell
.\scripts\build-windows-omnirig.ps1 -Clean
.\scripts\build-windows-omnirig.ps1 -SkipInstaller
.\scripts\build-windows-omnirig.ps1 -Jobs 4
```

## Toolchain notes

Expected tools and dependencies:

- MSYS2 installed in `C:\msys64`.
- MSYS2 UCRT64 packages for GCC, GFortran, CMake, Ninja, Qt 5, Boost, FFTW,
  PortAudio and libusb.
- Hamlib 4.x installed into the UCRT64 prefix.
- OmniRig installed on Windows.
- Inno Setup 6 installed on Windows.

OpenMP is enabled for the FT8 multithread decoder. Pass `-RunTests` to run
the decoder and application-path regression tests before staging the installer.
The release workflow also installs the generated EXE and verifies that the GUI
and decoder start without any build-tool DLL directories on PATH.

## Reproducing the GitHub release build

The manual [release workflow](../.github/workflows/release-packages.yml)
lists the UCRT64 packages and setup commands used for 1.4.1. It builds
Hamlib 4.6.5 from a verified source archive, installs OmniRig and Inno Setup,
and invokes this script with `-RunTests` and a fresh staging directory.
See `scripts/ci/` for the dependency setup and installer smoke test.

Use matching UCRT64 libraries throughout the build. The older Qt 5.15.2 /
MinGW 8.1 toolchain recipe does not describe the current release artifacts.
