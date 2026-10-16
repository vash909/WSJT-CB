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

## 1.4.1 release build on the Qt/MinGW toolchain

To build the 1.4.1 Windows release with the established Qt/MinGW toolchain,
use Qt 5.15.2, MinGW/GFortran 8.1.0, Boost 1.74 and the locally installed
static Hamlib 4.5.5 and PortAudio libraries. This is a separate toolchain from the UCRT64 script above.
OpenMP is enabled in this build, including the FT8 multithread decoder.

To configure this toolchain, put the matching CMake, MinGW and Qt `bin`
directories on `PATH`, and pass the Qt, Boost, Hamlib, PortAudio and libusb
prefixes in `CMAKE_PREFIX_PATH`. Configure a new build directory with:

```powershell
cmake -S . -B build-user/release-1.4.1 -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DWSJT_SKIP_MANPAGES=ON -DWSJT_GENERATE_DOCS=OFF `
  -DWSJT_WITH_OMNIRIG=ON -DWSJT_BUILD_DECODER_TESTS=ON `
  -DCMAKE_DISABLE_FIND_PACKAGE_OpenMP=FALSE `
  "-DCMAKE_PREFIX_PATH=<Qt>;<Boost>;<Hamlib>;<PortAudio>;<libusb>" `
  "-DFFTW3_ROOT_DIR=<FFTW>" `
  "-DCMAKE_INSTALL_PREFIX=$PWD/build-user/release-1.4.1/stage"
cmake --build build-user/release-1.4.1 --parallel 6
ctest --test-dir build-user/release-1.4.1 --output-on-failure
cmake --install build-user/release-1.4.1
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" `
  "/DDistDir=$PWD\build-user\release-1.4.1\stage" wsjtcb_dist_x64.iss
```

Replace the angle-bracket paths with the installed dependency prefixes.
The tests also require Python 3 and the dependency DLL directories on
`PATH`. Use a fresh staging directory so obsolete DLLs from previous
releases are not included. `DistDir` defaults to `dist` when omitted from
the Inno Setup command.
