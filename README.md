> **Disclaimer**
> This software is provided for educational purposes only.
> You are solely responsible for ensuring full compliance with all applicable local, national, and international radiocommunication laws and regulations.
> This project is distributed under the GNU General Public License v3.0 (GPLv3); see `COPYING` for details.

# WSJT-CB 1.4.1: a 27 MHz version of WSJT-X by Joe Taylor K1JT

Modified by 1AT106 / 1XZ001 Vash. Application credits: 1AT106, 1XZ732, 1AT1989.

This document describes the code changes currently present in this fork, compared to the original WSJT-X source snapshot used as base.

## Release Notes 1.4.1 — Scarlet

- Fixed WSPR and radio-control errors caused by settings or files still pointing
  to the old branded application directory. Known legacy paths now migrate to
  `WSJT-CB`, preserving existing files and separate rig profiles.

- WSPR now supports CB callsigns with a dedicated encoding that transmits the
  full callsign, a 4-character locator and transmit power in every frame.
  This WSPR variant communicates with other WSJT-CB stations; standard
  WSJT-X/WSPR and WSPRnet cannot decode it. WSPRnet upload was removed.
- The FT8 multithread decoder incorporates reliability fixes from WSJT-X
  3.2.0-rc1: coordinated signal subtraction, separate worker spectra, coverage
  of the full frequency range with the actual OpenMP thread count, and safer
  FFTW plan caching and worker state.
- FT8 decoder plausibility checks now accept complete CB callsigns, including
  type 4 messages with a hashed callsign in either position. CRC and signal
  quality checks still apply.
- The interface includes the MBD decoder logo and retains the dark theme and
  stable settings/log directories introduced in 1.3.
- WSPR band hopping has been removed from the CB interface.
- The Windows x64 release is packaged as `wsjtcb-1.4.1-win64-setup.exe`,
  with English and Italian setup languages, OmniRig integration and the
  required Qt/compiler runtime libraries. See section 10 for installation.
- Optional regression tests cover FT8 concurrency, frequency ranges, CB
  callsign packing, AutoSeq decisions and simulated audio decoding. See
  [the decoder test instructions](tests/ft8/README.md).

Full version history is in [releasenotes.txt](releasenotes.txt). Linux package
installation and compatibility details are in section 8 below.

## Release Notes 1.2 (historical)

Version 1.2 is a focused refinement of WSJT-CB 1.0. This release does not try to add complexity; instead, it makes the CB-oriented workflow cleaner, more reliable, and more consistent for everyday operation.

Compared with 1.0, version 1.2 improves the handling of CB-to-CB contacts, simplifies the interface by removing features that are less relevant to the project direction, and strengthens packaging, branding, and application identity.

### Highlights of Version 1.2

- Improved CB-to-CB QSO handling, especially around signal reports, free-text parsing, and final exchange messages such as `RR73` and `73`.
- More reliable AutoSeq and logging flow during non-standard CB callsign contacts, with fewer edge-case failures and cleaner end-of-QSO progression.
- Refined CB country recognition and decoded text presentation.
- Corrected and cleaned up country naming and lookup behavior, including fixes for mismatches such as `U.S.A.`.
- Unknown CB prefixes are no longer shown with rough placeholder labels, making the decoded display cleaner and more consistent.
- PSK Reporter spotting is now enabled by default so a fresh installation is immediately more useful without extra setup.
- Removed older or less relevant legacy components, including the Astronomical Data widget, ALLCALL7 filtering logic, legacy side tools, and several obsolete subprojects and documentation pieces.
- WSJT-CB branding is now more consistent across the UI, help menu, log files, configuration names, and installer-related paths.
- The help area has been simplified and a Telegram group entry has been added.
- Windows packaging has been improved with a dedicated `1.2.0` installer configuration and fixes for country-file lookup in installed environments.

### Summary

Version 1.2 is a maturity release. It sharpens the reliability of CB-specific operating flows, cleans up presentation and country handling, reduces legacy baggage, and delivers a more coherent WSJT-CB experience from first launch to daily use.

## Important Note: `<...>` with Non-Standard Callsigns (FT8)

When two non-standard/CB callsigns are exchanged in FT8, one callsign may be sent as a hash.
Because of this, third-party listeners can see decodes like `<...> 26AT016` instead of `30AT084 26AT016`.

This is a protocol/encoding behavior, not an AutoSeq bug.
If your station has not yet learned the hash-to-callsign mapping (for example because that station has not been decoded in clear text yet), the real callsign may remain hidden until a later decode provides that mapping.

## WSPR for CB Callsigns (WSJT-CB variant, 1.4)

Standard WSPR cannot carry an 11 m CB callsign. Its message formats only pack a
*standard* amateur callsign (digit in the 2nd or 3rd character) into 28 bits;
CB callsigns such as `1AT001` or `26CT100` are rejected by the packer. The only
WSPR path for a non-standard callsign is a 15-bit hash (Type 3), and a receiver
can turn that hash back into a callsign only if it already learned the mapping
from a decodable Type-1/Type-2 transmission — which a CB callsign can never
produce. So plain WSPR would only ever show `<...>` for CB stations, unlike FT8.

WSJT-CB 1.4 therefore replaces the WSPR **source encoding** with a CB-specific,
self-contained format (WSPR mode is CB-only in this fork):

- The CB callsign (`N{1,3}L{1,2}N{0,3}`, plus the 4-digit-suffix case for a
  1-digit prefix) is enumerated bijectively into **30 bits** (935,913,420
  possible callsigns, which fits within 2^30).
- The 4-character Maidenhead grid takes **15 bits** and the transmit power a
  **5-bit** index over the 19 standard WSPR dBm levels.
- Together that is exactly the **50** WSPR source bits, so the full callsign is
  transmitted in **every** frame — no hash table, receivers never show `<...>`.

The convolutional code (K=32, r=1/2), interleaving, sync vector and 4-FSK
modulation are untouched, so weak-signal sensitivity is identical to standard
WSPR. The trade-off is deliberate and matches the fork's goals: these
transmissions are decodable **only by other WSJT-CB stations** and are **not**
compatible with standard WSPR/WSJT-X or with wsprnet (which does not host 11 m
activity anyway). 6-character grids are not used in this mode.

Implementation: `packcb`/`unpackcb` in `lib/packjt.f90` (TX) and the mirrored C
`packcb`/`unpackcb` in `lib/wsprd/wsprd_utils.c` and `lib/wsprd/wsprsim_utils.c`
(RX and signal-subtraction), wired through `lib/wqencode.f90` and
`MainWindow::WSPR_message()`.

## Modified Files

- `CMakeLists.txt`
- `Radio.hpp`
- `Radio.cpp`
- `models/Bands.cpp`
- `models/FrequencyList.cpp`
- `widgets/mainwindow.cpp`
- `main.cpp`
- `lib/packjt.f90` (WSPR CB source coding)
- `lib/wqencode.f90` (WSPR CB source coding)
- `lib/wsprd/wsprd_utils.c` (WSPR CB decode)
- `lib/wsprd/wsprd_utils.h` (WSPR CB decode)
- `lib/wsprd/wsprsim_utils.c` (WSPR CB encode / subtraction)

## 1) CB Callsign Support (`N{1,3}L{1,2}N{1,3}` and `N{1,3}L{1,2}/L{2}`)

Files:
- `Radio.hpp`
- `Radio.cpp`

Changes:
- Added `Radio::is_cb_callsign(QString const&)`.
- Added `Radio::cb_country_prefix(QString const&)` so CB country lookup can use
  only the leading numeric prefix, including compound calls such as `999ZZ/ZZ`.
- Added CB callsign regex:
  - `^[0-9]{1,3}[A-Z]{1,2}[0-9]{1,3}$`
  - special-case extension for 4-digit unit numbers only with a 1-digit country prefix
  - compound extension `^[0-9]{1,3}[A-Z]{1,2}/[A-Z]{2}$`
- Extended `Radio::is_callsign(...)` so CB calls are treated as valid callsigns.

Impact:
  - Callsigns like `1A1`, `26AT101`, `21AT106`, `999ZZ999`, `1AT1000`, and `999ZZ/ZZ` are accepted by validation logic used by the UI and message processing flow.

Examples:

| Callsign | Accepted | Reason |
| --- | --- | --- |
| `1A1` | Yes | 1-digit prefix, 1 letter, 1-digit suffix |
| `1TT1` | Yes | 1-digit prefix, 2 letters, 1-digit suffix |
| `1TT01` | Yes | 1-digit prefix, 2 letters, 2-digit suffix |
| `1TT001` | Yes | 1-digit prefix, 2 letters, 3-digit suffix |
| `1TT1000` | Yes | 1-digit prefix, 2 letters, 4-digit suffix allowed by special case |
| `1AT1000` | Yes | 1-digit prefix, 2 letters, 4-digit suffix allowed by special case |
| `11TT1` | Yes | 2-digit prefix, 2 letters, 1-digit suffix |
| `111TT11` | Yes | 3-digit prefix, 2 letters, 2-digit suffix |
| `111TT999` | Yes | 3-digit prefix, 2 letters, 3-digit suffix |
| `26AT715` | Yes | 2-digit prefix, 2 letters, 3-digit suffix |
| `999ZZ/ZZ` | Yes | 3-digit prefix, 2 letters, slash, 2-letter suffix |
| `26AT1000` | No | 4-digit suffix is not allowed with a 2-digit prefix |
| `111TT1000` | No | 4-digit suffix is not allowed with a 3-digit prefix |
| `99Z9999` | No | 4-digit suffix is not allowed with a 2-digit prefix |
| `1TT10000` | No | suffix longer than 4 digits is not allowed |
| `1TT` | No | missing numeric suffix |
| `1AT` | No | missing numeric suffix |
| `AT1000` | No | missing numeric prefix |
| `AAA` | No | letters only; numeric prefix and suffix are both missing |
| `123` | No | digits only; the alphabetic middle part is missing |
| `12A` | No | missing numeric suffix |
| `12ABC1` | No | 3-letter middle part is not allowed; only 1 or 2 letters are accepted |
| `ABC123` | No | missing numeric prefix |
| `1/AT100` | No | slash is only allowed as a final `/LL` suffix after the CB stem |

## 2) 11m/CB Band, UI Modes, and Default Frequencies

Files:
- `models/Bands.cpp`
- `models/FrequencyList.cpp`

Changes:
- Added ADIF-style band entry:
  - `11m` from `26.965 MHz` to `27.405 MHz`.
- Limited the main UI to CB-safe operating modes:
  - `FT8`
  - `FT4`
  - `FST4`
  - `Q65`
  - `WSPR` (CB-specific variant added in 1.4)
- Hidden from the main UI:
  - `JT4`
  - `JT9`
  - `JT65`
  - `MSK144`
  - `FST4W`
  - `Echo`
  - `FreqCal`
- Added/kept default CB working frequencies:
  - `FT8`: `27.265 MHz` (`27265000` Hz), label `CB`, preferred
  - `FT8`: `27.045 MHz` (`27045000` Hz), label `CB (2nd)`
  - `FT4`: `27.045 MHz` (`27045000` Hz), label `CB`
  - `FST4`: `27.045 MHz` (`27045000` Hz), label `CB`
  - `Q65`: `27.045 MHz` (`27045000` Hz), label `CB`
  - `WSPR`: `27.195 MHz` (`27195000` Hz), label `CB`

Impact:
- The fork no longer treats 27.265 MHz as out-of-band for normal operation in this added 11m segment.
- FT8 keeps the dual-frequency choice on CB, while the other exposed CB modes default to `27.045 MHz`.
- Modes without reliable CB callsign support are no longer user-selectable from the main UI, reducing accidental use of unsupported workflows.

## 3) CB NNN Country Lookup in Decoded Display

Files:
- `logbook/AD1CCty.cpp`
- `widgets/displaytext.cpp`

Changes:
- Added CB country-resolution using the numeric prefix for CB callsigns matching `N{1,3}L{1,2}N{1,3}` and `N{1,3}L{1,2}/L{2}`.
- Added static NNN→country map based on international CB prefix list.
- Added lookup in `AD1CCty::lookup(...)` before standard DXCC lookup. For CB callsigns it normalizes the numeric prefix to a zero-padded `NNN`, then returns a record with `entity_name` country and `primary_prefix` `NNN`.
- Kept fallback lookup behavior unchanged for regular non-CB callsigns.

Impact:
- CB calls like `001AB123`, `1AT106`, and `26AT101` now display the CB country directly in decoded text, instead of falling back to DXCC prefix logic.

## 4) Auto-Sequence Fixes for Long CB Callsigns

File:
- `widgets/mainwindow.cpp`

Main changes:
- `auto_sequence(...)`
  - Added handling for non-standard decodes clearly addressed to local station.
  - Added CB-specific detection for incoming tokens:
    - `+NN`, `-NN`, `R+NN`, `R-NN`, `RR73`, `RRR`, `73`.
  - Allows AutoSeq progression when both stations use long CB-style callsigns.

- `processMessage(...)`
  - Accepts CB free-text messages addressed to local station even without `<...>` wrappers.
  - Adds recovery when parser returns invalid/partial `hiscall` (for example from `R+13`), forcing the currently selected DX callsign when needed.
  - Adds CB pair fallback logic to avoid stalled QSO state when standard FT8 parsing for two long non-standard calls is ambiguous.
  - Uses guarded numeric parsing (`toInt(&ok)`) to avoid false transitions from empty tokens.

- `genStdMsgs(...)`
  - For two long CB non-standard calls, generates a deterministic, encodable sequence:
    - Tx1: `<HISCALL> MYCALL`
    - Tx2: `HISCALL +/-NN`
    - Tx3: `HISCALL R+/-NN`
    - Tx4: `HISCALL RR73`
    - Tx5: `HISCALL 73`
  - Avoids unreliable `CALL CALL +/-NN` patterns that can fail with long non-standard pairs.

Impact:
- Auto-sequencing progresses correctly in CB-to-CB QSOs and includes report/roger/signoff phases instead of looping at CQ or report exchanges.

## 4) Pre-Release Warning and Time Lock Disabled

File:
- `widgets/mainwindow.cpp`

Changes:
- `MainWindow::not_GA_warning_message()` is now intentionally empty.

Impact:
- The pre-release popup is removed.
- The related expiration behavior tied to that warning flow is disabled in this fork.

## 5) Application Name / Branding

File:
- `main.cpp`

Changes:
- Application name changed to:
  - `WSJT-CB by 1AT106 1XZ732 1AT1989`

Impact:
- Updated program title/identity in runtime metadata and window/application naming derived from Qt application name.

## 6) Build System Improvements (CMake)

File:
- `CMakeLists.txt`

Changes:
- Added early Fortran compiler detection before `project(...)`:
  - checks `gfortran`, `ifx`, `ifort`, `flang-new`, `flang`, `f95`, `f90`
  - clear fatal message if missing.
- Added `WSJT_ENABLE_WERROR` option (default `OFF`) to make `-Werror` optional.
- Improved Boost.Log discovery:
  - normal CMake config search first
  - fallback with `Boost_NO_BOOST_CMAKE=ON`
  - clearer fatal error with Debian/Ubuntu package hint.
- Kept/documented toggles used for local builds:
  - `WSJT_SKIP_MANPAGES`
  - `WSJT_GENERATE_DOCS`

Impact:
- More robust configuration on Kubuntu/Ubuntu systems with clearer dependency failures and easier local builds.

## 7) Kubuntu Build Notes Used for This Fork

These package names were verified against Ubuntu/Kubuntu Noble repositories:

- `build-essential`
- `cmake`
- `ninja-build`
- `gfortran`
- `libudev-dev`
- `libboost-log-dev`
- `libboost-thread-dev`
- `libfftw3-dev`
- `libhamlib-dev`
- `libusb-1.0-0-dev`
- `qtbase5-dev`
- `qtmultimedia5-dev`
- `qttools5-dev`
- `qttools5-dev-tools`
- `libqt5serialport5-dev`
- `libqt5websockets5-dev`
- `asciidoc` (only needed if manpages are generated)

For the 1.4 source, check the Hamlib version before building: the package
name alone does not establish API compatibility. See section 8 for the
Hamlib requirement and the distribution-specific release packages.

Example local configure command (no manpages/docs):

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DWSJT_SKIP_MANPAGES=ON \
  -DWSJT_GENERATE_DOCS=OFF
```

## 8) Linux DEB/RPM Installation and Compatibility

Version 1.4.1 uses separate **amd64/x86-64** packages for each supported
system. Download the file matching your distribution and version from
[GitHub Releases](https://github.com/vash909/WSJT-CB/releases).

| Target system | Package |
| --- | --- |
| Ubuntu 22.04 | `wsjtcb_1.4.1_ubuntu22.04_amd64.deb` |
| Ubuntu 24.04 | `wsjtcb_1.4.1_ubuntu24.04_amd64.deb` |
| Debian 12 | `wsjtcb_1.4.1_debian12_amd64.deb` |
| Debian 13 | `wsjtcb_1.4.1_debian13_amd64.deb` |
| Fedora 43 | `wsjtcb-1.4.1-fedora43.x86_64.rpm` |
| Fedora 44 | `wsjtcb-1.4.1-fedora44.x86_64.rpm` |

For example, on Ubuntu 24.04:

```bash
sudo apt update
sudo apt install ./wsjtcb_1.4.1_ubuntu24.04_amd64.deb
```

On Fedora 44:

```bash
sudo dnf install ./wsjtcb-1.4.1-fedora44.x86_64.rpm
```

Use APT or DNF so the required libraries are installed automatically.
Ubuntu needs the Universe repository enabled. Start **WSJT-CB** from the
application menu or run `wsjtcb`.

Each package is compiled against its target distribution's libraries.
Dependencies are derived from the resulting binaries; Qt multimedia and
SQLite plugins are included in the dependency list. Hamlib 4.6.5 is linked
statically, avoiding incompatibility with older system Hamlib versions.
Boost, Qt, FFTW and compiler runtime libraries come from the target system.
Do not install a package built for another distribution or release.

The manual workflow
[Build and Verify Release Packages](.github/workflows/release-packages.yml)
runs the path-migration and decoder regression tests. It then installs each
package in a fresh official distribution container, checks dependencies,
and starts the installed GUI and `jt9` decoder for 40 seconds under Xvfb
as an ordinary user. The release notes report the results for published
artifacts. These checks do not exercise physical audio devices, CAT/PTT,
native Wayland sessions or live QSOs.

Helpers such as `jt9` and `wsprd` are installed under `/usr/lib/wsjtcb/bin`,
with a launcher at `/usr/bin/wsjtcb`, to avoid conflicts with WSJT-X.
ARM, 32-bit systems, Ubuntu 20.04 and Debian 11 are outside this release's
package test matrix; old compatibility-test results do not apply to 1.4.1.

### Reproducing a native package

The CI scripts install build dependencies, build pinned Hamlib and create a
package with generated dependencies. Run them inside a disposable container,
not directly on your desktop. For example, from the repository root:

```bash
docker run --rm -v "$PWD:/src" -w /src debian:12 \
  bash scripts/ci/build-linux.sh debian12
docker run --rm -v "$PWD:/src" -w /src debian:12 \
  bash scripts/ci/test-linux-package.sh debian12
```

The package and test reports are written to `dist/debian12/`. Replace both
the image and target name for another system listed above. The superseded
`release-deb.yml` and `test-deb-package-compatibility.yml` workflows are
disabled; the manual release workflow provides the current build procedure.

## 9) Arch Linux / CachyOS Build Notes

These package names are for Arch Linux and CachyOS (Arch-based distributions):

- `base-devel` (includes essential build tools)
- `cmake`
- `ninja`
- `gcc-fortran` (provides gfortran)
- `boost` (includes boost-log and boost-thread)
- `fftw`
- `hamlib`
- `libusb`
- `qt5-base`
- `qt5-multimedia`
- `qt5-tools`
- `qt5-serialport`
- `qt5-websockets`
- `asciidoc` (only needed if manpages are generated)

Example local configure command (no manpages/docs):

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DWSJT_SKIP_MANPAGES=ON \
  -DWSJT_GENERATE_DOCS=OFF
```

## 10) Windows x64 Installation (1.4.1)

The Windows installer generated from this repository is:

```text
installer-output\wsjtcb-1.4.1-win64-setup.exe
```

Run the installer, choose English or Italian, and complete setup. Windows
requests administrator permission to install under `Program Files`. Start
**WSJT-CB** from the Start menu or the optional desktop shortcut.

The package includes the application, `jt9` and `wsprd` decoders, rig-control
helpers, Qt plugins, data files and runtime libraries. Qt, MinGW and MSYS2
do not need to be installed on the destination PC. Install OmniRig separately
if you want to use **OmniRig Rig 1 / Rig 2**; other CAT choices use Hamlib.

The release workflow uses MSYS2 UCRT64, Qt 5, GCC/GFortran and Hamlib
4.6.5 with OpenMP enabled. It runs seven regression tests, including path
migration, 40 simulated CB audio decodes and AutoSeq checks with different
thread counts, then installs the EXE and starts the GUI and decoder without
compiler DLL directories on PATH. Published release notes report the results.
Physical audio devices, CAT/PTT hardware and live QSOs require separate tests.

The installer retains the application identity used by previous WSJT-CB
installers and reuses the existing installation directory. Settings and logs
continue to use the stable WSJT-CB data locations introduced in 1.3.

WSPR in 1.4.1 requires another WSJT-CB station using the CB WSPR format;
standard WSJT-X/WSPR stations and WSPRnet are incompatible with that format.

For the MSYS2 UCRT64 build prerequisites and packaging command, see
[the Windows build instructions](scripts/README-windows-omnirig.md).
