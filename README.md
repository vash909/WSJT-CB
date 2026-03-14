# WSJT-CB: a 27MHz version of wsjt-x by Joe Taylor K1JT
# mod by 1AT106 / 1XZ001 Vash

This document describes the code changes currently present in this fork, compared to the original WSJT-X source snapshot used as base.


## Modified Files

- `CMakeLists.txt`
- `Radio.hpp`
- `Radio.cpp`
- `models/Bands.cpp`
- `models/FrequencyList.cpp`
- `widgets/mainwindow.cpp`
- `main.cpp`

## 1) CB Callsign Support (`NNNLLNNN`)

Files:
- `Radio.hpp`
- `Radio.cpp`

Changes:
- Added `Radio::is_cb_callsign(QString const&)`.
- Added CB callsign regex:
  - `^[0-9]{3}[A-Z]{2}[0-9]{3}$`
- Extended `Radio::is_callsign(...)` so CB calls are treated as valid callsigns.

Impact:
- Callsigns like `999XZ999` are now accepted by validation logic used by the UI and message processing flow.

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

## 3) Auto-Sequence Fixes for Long CB Callsigns

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

# WSJT-CB
