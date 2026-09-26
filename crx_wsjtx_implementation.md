## CRX-API Integration for WSJT-CB
## Bastien 14FDX897
## Date    : 2026/09
## Version : 1.0

## Overview

This integration adds CRX Cloud API support natively inside WSJT-CB's Settings dialog. When enabled, logged QSOs are forwarded to a CRX logbook
and dx-spots can optionally be forwarded to the 11m network dxcluster (via CRX 11m node `N1`). all without requiring an external bridge process.

The implementation follows the existing Cloudlog integration pattern (`Network/Cloudlog.hpp` / `.cpp`), 
reusing the same `QNetworkAccessManager` instance and `pimpl` idiom.

## Files Modified

| File | Change |
|---|---|
| `Configuration.ui` | Added CRX section inside `network_group_box` (Network Services) |
| `Configuration.hpp` | Added 5 CRX getter declarations |
| `Configuration.cpp` | Added CRX member vars, getters, read/write/accept, UI toggle + button slots |
| `widgets/mainwindow.h` | Added `#include "Network/CrxApi.hpp"` + `CrxApi m_crxApi` member |
| `widgets/mainwindow.cpp` | Hook `acceptQSO()` → logbook; hook `pskPost()` → DXCluster spot |
| `CMakeLists.txt` | Added `Network/CrxApi.cpp` to source list |

## Files Created

| File | Purpose |
|---|---|
| `Network/CrxApi.hpp` | CRX API client class (pimpl, Cloudlog pattern) |
| `Network/CrxApi.cpp` | Full implementation — ADIF parser, HTTP POST, band mapping |
| `crx_wsjtx_implementation.md` | This file |

## API Endpoints Used

All calls hit a single endpoint: `https://s.crx.cloud/api/` via HTTPS POST with `Content-Type: application/json`.

| Action | JSON Body |
|---|---|
| Health check | `{"req":{"type":"radio","query":"health_check","apikey":"KEY"}}` |
| Log QSO | `{"req":{"type":"radio","query":"edit_myqso","apikey":"KEY","qsoData":{...}}}` |
| Send DX spot | `{"req":{"type":"radio","query":"edit_myspot","apikey":"KEY","spotData":{...}}}` |
| Fetch logbooks | `{"req":{"type":"radio","query":"get_mylogs","apikey":"KEY"}}` |

### QSO Payload

`qsoData` fields: `qso_id` (0=new), `f_log_id`, `logentry_his_call`, `logentry_his_name`, `logentry_band`, `logentry_frequency`, `logentry_mode`, `logentry_his_report`, `logentry_my_report`, `logentry_comment`.

ADIF fields parsed: `call`, `mycall`, `qso_mode`, `band`, `qso_freq`, `rst_sent`, `rst_rcvd`, `gridsquare`, `name`, `comment`.

### Spot Payload

`spotData` fields: `callsign_dx`, `callsign_sender`, `frequency`, `contact_comment`.

## Settings Schema (QSettings keys)

| Key | Type | Default | Description |
|---|---|---|---|
| `CRXAPIEnabled` | bool | `false` | Master toggle for CRX integration |
| `CRXAPIKey` | QString | `""` | API key from s.crx.cloud |
| `CRXForwardLogbook` | bool | `true` | Forward logged QSOs to CRX logbook |
| `CRXForwardDXCluster` | bool | `false` | Forward decoded spots to DXCluster |
| `CRXLogbookID` | int | `0` | Selected logbook ID |

## UI Layout

**Settings → Reporting → Network Services** (below PSK Reporter row, separated by a horizontal line):

```
[X] Enable CRX-API Service
    CRX API Key:  [************] [Test]
    Forward to Logbook:  [-- Select logbook -- ]  [Refresh]
    [X] Forward to DXCluster
```

- **Enable CRX-API Service** — master checkbox; toggles visibility of all child widgets
- **CRX API Key** — password-edit field + Test button (green=OK, orange=read-only, red=invalid)
- **Forward to Logbook** — combobox populated by 🔄 button (calls `get_mylogs`)
- **Forward to DXCluster** — checkbox, unchecked by default

## Data Flow

### QSO Logbook Forwarding

```
QSO completed in WSJT-CB
  → MainWindow::acceptQSO() called with ADIF payload
    → if (spot_to_crx_api && crx_forward_logbook)
        → m_crxApi.logQso(ADIF)
          → CrxApi parses ADIF, extracts fields
          → POST to https://s.crx.cloud/api/  (query=edit_myqso)
```

### DX Spot Forwarding

```
Station decoded with grid in WSJT-CB
  → MainWindow::pskPost() called
    → if (spot_to_crx_api && crx_forward_dxcluster)
        → m_crxApi.sendSpot(call, grid, freq, mode, snr)
          → POST to https://s.crx.cloud/api/  (query=edit_myspot)
```

## Build

Standard CMake build — no extra dependencies required. The `Network/CrxApi.cpp` file is automatically added to the build via `CMakeLists.txt`.

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Relation to wsjtx_crx_bridge.py (https://git.crx.cloud/f4eyq/crx-qso-agent)

The standalone `wsjtx_crx_bridge.py` script performs the same function (QSO logbook + DX spot forwarding)
but runs as a separate process listening on UDP. This native integration:
- Eliminates the need for an external Python process
- Uses WSJT-CB's own network stack (`QNetworkAccessManager`)
- Follows the same API contract as the Python bridge (same endpoints, same payload format)
- The Python bridge remains available as an alternative for users who prefer it

## Band Mapping

Frequency to band name conversion matches `wsjtx_crx_bridge.py` BAND_MAP:
136kHz, 500kHz, 160m, 80m, 60m, 40m, 30m, 20m, 17m, 15m, 12m, 11m, 10m, 6m, 2m

## Error Handling

- API connection failures (network error, HTTP non-200): silent in background, logged via `qWarning()`
- Invalid API key: detected via health_check test (button turns red)
- ADIF parse errors: missing fields default to safe values (e.g., RST defaults to "599")
- Self-spotting prevention: inherited from existing pskPost() grid/CQ checks
