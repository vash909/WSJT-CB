> **Disclaimer**
> This software is provided for educational purposes only.
> You are solely responsible for ensuring full compliance with all applicable local, national, and international radiocommunication laws and regulations.
> This project is distributed under the GNU General Public License v3.0 (GPLv3); see `COPYING` for details.

# WSJT-CB: a 27MHz version of wsjt-x by Joe Taylor K1JT
# mod by 1AT106 / 1XZ001 Vash

This document describes the code changes currently present in this fork, compared to the original WSJT-X source snapshot used as base.

## Important Note: `<...>` with Non-Standard Callsigns (FT8)

When two non-standard/CB callsigns are exchanged in FT8, one callsign may be sent as a hash.
Because of this, third-party listeners can see decodes like `<...> 26AT016` instead of `30AT084 26AT016`.

This is a protocol/encoding behavior, not an AutoSeq bug.
If your station has not yet learned the hash-to-callsign mapping (for example because that station has not been decoded in clear text yet), the real callsign may remain hidden until a later decode provides that mapping.

## Modified Files

- `CMakeLists.txt`
- `Radio.hpp`
- `Radio.cpp`
- `models/Bands.cpp`
- `models/FrequencyList.cpp`
- `widgets/mainwindow.cpp`
- `main.cpp`

## 1) CB Callsign Support (`N{1,3}L{1,2}N{1,3}`)

Files:
- `Radio.hpp`
- `Radio.cpp`

Changes:
- Added `Radio::is_cb_callsign(QString const&)`.
- Added CB callsign regex:
  - `^[0-9]{1,3}[A-Z]{1,2}[0-9]{1,3}$`
  - special-case extension for 4-digit unit numbers only with a 1-digit country prefix
- Extended `Radio::is_callsign(...)` so CB calls are treated as valid callsigns.

Impact:
- Callsigns like `1A1`, `26AT101`, `21AT106`, `999ZZ999`, and `1AT1000` are accepted by validation logic used by the UI and message processing flow.

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
| `26AT1000` | No | 4-digit suffix is not allowed with a 2-digit prefix |
| `111TT1000` | No | 4-digit suffix is not allowed with a 3-digit prefix |
| `1TT10000` | No | suffix longer than 4 digits is not allowed |
| `1TT` | No | missing numeric suffix |
| `AT1000` | No | missing numeric prefix |

## 2) 11m/CB Band and 27.265 MHz Frequency

Files:
- `models/Bands.cpp`
- `models/FrequencyList.cpp`

Changes:
- Added ADIF-style band entry:
  - `11m` from `26.965 MHz` to `27.405 MHz`.
- Added default FT8 working frequency:
  - `27.265 MHz` (`27265000` Hz), label `CB`, enabled as preferred.

Impact:
- The fork no longer treats 27.265 MHz as out-of-band for normal operation in this added 11m segment.

## 3) CB NNN Country Lookup in Decoded Display

Files:
- `logbook/AD1CCty.cpp`
- `widgets/displaytext.cpp`

Changes:
- Added CB country-resolution using the numeric prefix for CB callsigns matching `N{1,3}L{1,2}N{1,3}`.
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
  - `WSJT-CB by 1AT106 - 1XZ732 - 161XZ085`

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

Example local configure command (no manpages/docs):

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DWSJT_SKIP_MANPAGES=ON \
  -DWSJT_GENERATE_DOCS=OFF
```

## 8) Arch Linux / CachyOS Build Notes

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

# WSJT-CB
