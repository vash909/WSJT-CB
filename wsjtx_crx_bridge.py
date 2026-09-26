#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
╔══════════════════════════════════════════════════════╗
║          CRX-QSO-AGENT  v1.1.3                       ║
║    WSJT-X UDP listener → CRX Logbook API bridge      ║
╚══════════════════════════════════════════════════════╝

Author   : f4eyq
Version  : 1.1.3  -  September 2026
License  : GNU

Compatible : Python 3.10+ | Windows 10/11 | Linux (Ubuntu/Debian/Fedora)
Build      : pyinstaller --onefile --windowed wsjtx_crx_bridge.py

Runtime dependencies : pip install requests
Build dependencies   : pip install pyinstaller

How it works:
  1. Opens a UDP socket on the configured port (default 2237)
  2. Receives QSOLogged messages from WSJT-X (Qt QDataStream protocol)
  3. Parses fields: callsign, band, frequency, mode, RST, grid…
  4. Sends each QSO to the CRX API via HTTPS POST
  5. Handles errors with automatic retry queue
  6. Optional duplicate filtering (same call/band/mode/UTC day)
  7. Optional auto-dxspot: HAM keys use the ham spot payload, 11M-xxxx keys
     are routed automatically to the 11M/CB spot payload
"""

# -- Standard imports ----------------------------------------------------------
import json
import logging
import os
import queue
import socket
import struct
import sys
import threading
import time
from collections import deque, defaultdict
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Set, Tuple

# -- GUI -----------------------------------------------------------------------
import tkinter as tk
from tkinter import font as tkfont
from tkinter import messagebox, scrolledtext, ttk

# -- Required dependency -------------------------------------------------------
try:
    import requests
    from requests.adapters import HTTPAdapter
    from urllib3.util.retry import Retry
except ImportError:
    sys.exit(
        "ERROR: 'requests' module missing.\n"
        "Install it with:  pip install requests"
    )


# ══════════════════════════════════════════════════════════════════════════════
#  §1  CONSTANTS
# ══════════════════════════════════════════════════════════════════════════════

APP_NAME    = "CRX-QSO-AGENT"
APP_VERSION = "1.1.3"
CRX_API_URL = "https://s.crx.cloud/api/"

# Mode sent in the 11M/CB spot. Left empty like in the validated
# "Test Spot 11M" button. The real mode (FT8…) stays in the spot comment.
CB_SPOT_MODE = ""

# -- WSJT-X Protocol ----------------------------------------------------------
WSJTX_MAGIC    = 0xADBCCBDA
MSG_HEARTBEAT  = 0
MSG_STATUS     = 1
MSG_QSOLOGGED  = 5
MSG_LOGGEDADIF = 12

# -- Dark theme ----------------------------------------------------------------
BG_DARK   = "#1a1f2e"
BG_PANEL  = "#242938"
BG_ENTRY  = "#2d3452"
BG_TABLE  = "#1e2535"
FG_TEXT   = "#dce3f0"
FG_DIM    = "#6b7899"
FG_HDR    = "#a0aec0"
C_GREEN   = "#48bb78"
C_RED     = "#fc8181"
C_AMBER   = "#f6ad55"
C_BLUE    = "#63b3ed"
C_PURPLE  = "#b794f4"
C_ROW_OK  = "#1a3a2a"
C_ROW_ERR = "#3a1a1a"
C_ROW_PND = "#3a2e0a"


# ══════════════════════════════════════════════════════════════════════════════
#  §2  BAND / FREQUENCY MAPPING
# ══════════════════════════════════════════════════════════════════════════════

#  (freq_min_MHz, freq_max_MHz, crx_band_name)
BAND_MAP: List[Tuple[float, float, str]] = [
    (0.1355,    0.1385,   "136khz"),
    (0.4720,    0.4790,   "500khz"),
    (1.800,     2.000,    "160m"),
    (3.500,     4.000,    "80m"),
    (5.250,     5.450,    "60m"),
    (7.000,     7.300,    "40m"),
    (10.100,    10.150,   "30m"),
    (14.000,    14.350,   "20m"),
    (18.068,    18.168,   "17m"),
    (21.000,    21.450,   "15m"),
    (24.890,    24.990,   "12m"),
    (26.960,    27.410,   "11m"),
    (28.000,    29.700,   "10m"),
    (50.000,    54.000,   "6m"),
    (70.000,    70.500,   "4m"),
    (144.000,   148.000,  "2m"),
    (220.000,   225.000,  "220"),
    (430.000,   440.000,  "70cm"),
    (1240.000,  1300.000, "23cm"),
    (2300.000,  2450.000, "13cm"),
    (3300.000,  3500.000, "9cm"),
]


def hz_to_band(freq_hz: int) -> str:
    """Converts a frequency in Hz to a CRX band name (e.g. '20m')."""
    mhz = freq_hz / 1_000_000.0
    for lo, hi, name in BAND_MAP:
        if lo <= mhz <= hi:
            return name
    return "???"


def hz_to_khz_str(freq_hz: int) -> str:
    """Converts Hz → kHz string (e.g. 14074000 → '14074.0')."""
    return str(round(freq_hz / 1000.0, 3))


# ══════════════════════════════════════════════════════════════════════════════
#  §3  WSJT-X PARSER  (Qt QDataStream format, big-endian)
# ══════════════════════════════════════════════════════════════════════════════

class WsjtxParser:
    """
    Decodes UDP packets emitted by WSJT-X.

    General format:
        uint32  magic     (0xADBCCBDA)
        uint32  schema    (protocol version)
        uint32  type      (message type)
        QString client_id
        ...     message-specific fields
    """

    __slots__ = ("_d", "_o")

    def __init__(self, data: bytes):
        self._d = data   # bytes buffer
        self._o = 0      # current offset

    # -- Read primitives -------------------------------------------------------

    def _rem(self) -> int:
        return len(self._d) - self._o

    def _u8(self) -> int:
        v = struct.unpack_from(">B", self._d, self._o)[0]
        self._o += 1
        return v

    def _u32(self) -> int:
        v = struct.unpack_from(">I", self._d, self._o)[0]
        self._o += 4
        return v

    def _i32(self) -> int:
        v = struct.unpack_from(">i", self._d, self._o)[0]
        self._o += 4
        return v

    def _u64(self) -> int:
        v = struct.unpack_from(">Q", self._d, self._o)[0]
        self._o += 8
        return v

    # -- Qt types --------------------------------------------------------------

    def _qstring(self) -> Optional[str]:
        """
        Qt QString:
            uint32 length in bytes  (0xFFFFFFFF = null QString)
            bytes in UTF-16 big-endian
        """
        if self._rem() < 4:
            return None
        n = self._u32()
        if n == 0xFFFFFFFF:
            return None          # null QString
        if n == 0:
            return ""
        if self._rem() < n:
            return None
        raw = self._d[self._o: self._o + n]
        self._o += n
        try:
            return raw.decode("utf-8")
        except Exception:
            return raw.decode("latin-1", errors="replace")

    def _qdatetime(self) -> Optional[datetime]:
        """
        Qt QDateTime:
            uint64  julianDay         (Julian day number)
            uint32  msFromMidnight    (milliseconds since midnight)
            uint8   timeSpec          (0=local 1=UTC 2=OffsetUTC 3=TZ)
            [int32  offsetSeconds]    (only if timeSpec == 2)

        Returns a UTC datetime.
        """
        if self._rem() < 13:
            return None
        jd = self._u64()
        ms = self._u32()
        ts = self._u8()
        if ts == 2 and self._rem() >= 4:
            self._i32()          # UTC offset ignored (we normalise to UTC)

        # Conversion: JD 2440588 = 1970-01-01 (Unix epoch)
        unix_sec = (jd - 2440588) * 86400 + ms // 1000
        try:
            return datetime.fromtimestamp(unix_sec, tz=timezone.utc)
        except (ValueError, OSError):
            return None

    # -- Common header ---------------------------------------------------------

    def read_header(self) -> Optional[Tuple[int, int, str]]:
        """
        Reads the WSJT-X header.
        Returns (schema, msg_type, client_id) or None if magic is invalid.
        """
        if len(self._d) < 12:
            return None
        magic = self._u32()
        if magic != WSJTX_MAGIC:
            return None
        schema   = self._u32()
        msg_type = self._u32()
        client   = self._qstring() or ""
        return schema, msg_type, client

    # -- Message type 5: QSOLogged ---------------------------------------------

    def parse_qso_logged(self, schema: int) -> Optional[Dict[str, Any]]:
        """
        Decodes a QSOLogged message (type 5).

        Fields by schema:
          all   : DateTimeOff DxCall DxGrid TxFreq Mode RstSent RstRcvd
                  TxPower Comments Name DateTimeOn
          ≥ 2   : OperatorCall
          ≥ 3   : MyCall MyGrid ExchangeSent ExchangeRcvd ADIFPropMode
        """
        try:
            dt_off  = self._qdatetime()
            call    = (self._qstring() or "").upper().strip()
            grid    = (self._qstring() or "").upper().strip()
            freq_hz = self._u64()
            mode    = (self._qstring() or "").upper().strip()
            rst_s   = (self._qstring() or "599").strip()
            rst_r   = (self._qstring() or "599").strip()
            txpwr   = (self._qstring() or "").strip()
            cmnt    = (self._qstring() or "").strip()
            name    = (self._qstring() or "").strip()
            dt_on   = self._qdatetime()

            op_call   = my_call = my_grid = exch_s = exch_r = prop_mode = ""

            if schema >= 2:
                op_call = (self._qstring() or "").strip()
            if schema >= 3:
                my_call   = (self._qstring() or "").strip()
                my_grid   = (self._qstring() or "").strip()
                exch_s    = (self._qstring() or "").strip()
                exch_r    = (self._qstring() or "").strip()
            if schema >= 3 and self._rem() >= 4:
                prop_mode = (self._qstring() or "").strip()

            # Enriched comment for the CRX field
            parts = [p for p in [
                cmnt,
                f"TX:{txpwr}W"        if txpwr    else "",
                f"PROP:{prop_mode}"   if prop_mode else "",
                "WSJTX-CRX",
            ] if p]
            comment = " | ".join(parts)

            return {
                "dx_call":   call,
                "dx_grid":   grid,
                "freq_hz":   freq_hz,
                "freq_khz":  hz_to_khz_str(freq_hz),
                "band":      hz_to_band(freq_hz),
                "mode":      mode,
                "rst_sent":  rst_s,
                "rst_rcvd":  rst_r,
                "name":      name,
                "comment":   comment,
                "dt_off":    dt_off,
                "dt_on":     dt_on,
                "my_call":   my_call or op_call,
                "my_grid":   my_grid,
                "exch_sent": exch_s,
                "exch_rcvd": exch_r,
            }

        except Exception:
            return None


def decode_wsjtx_packet(data: bytes) -> Optional[Dict[str, Any]]:
    """
    Entry point: decodes a WSJT-X UDP packet.
    Returns a normalised QSO dict if it is a QSOLogged message (type 5),
    otherwise None (heartbeat, status, etc. are silently ignored).
    """
    try:
        p = WsjtxParser(data)
        hdr = p.read_header()
        if hdr is None:
            return None
        schema, msg_type, client_id = hdr
        if msg_type != MSG_QSOLOGGED:
            return None
        qso = p.parse_qso_logged(schema)
        if qso:
            qso["wsjtx_id"] = client_id
            qso["schema"]   = schema
        return qso
    except Exception:
        return None


# ══════════════════════════════════════════════════════════════════════════════
#  §4  CRX API CLIENT
# ══════════════════════════════════════════════════════════════════════════════

class CrxApiClient:
    """
    HTTP client for the CRX Cloud API (https://s.crx.cloud/api/).
    Includes automatic retry on transient server errors.
    """

    def __init__(self, api_key: str):
        self.api_key = api_key.strip()
        self.session = requests.Session()
        retry = Retry(
            total=3,
            backoff_factor=0.6,
            status_forcelist=[502, 503, 504],
            allowed_methods=["POST"],
        )
        self.session.mount("https://", HTTPAdapter(max_retries=retry))
        self.session.headers.update({"Content-Type": "application/json"})

    @property
    def is_11m(self) -> bool:
        """True if the key is an 11M/CB key ('11M-xxxx-xxxx-xxxx')."""
        return self.api_key.upper().startswith("11M-")

    def _post(self, query: str, extra: Optional[Dict] = None, timeout: int = 10) -> Dict:
        body: Dict[str, Any] = {
            "req": {"type": "radio", "query": query, "apikey": self.api_key}
        }
        if extra:
            body["req"].update(extra)
        r = self.session.post(CRX_API_URL, json=body, timeout=timeout)
        if not r.ok:
            # raise_for_status() used to hide the server's reason for the
            # rejection (e.g. invalid CB callsign format) — surface it.
            detail = (r.text or "").strip().replace("\n", " ")[:300]
            raise requests.HTTPError(
                f"HTTP {r.status_code}: {detail or r.reason}", response=r)
        return r.json()

    def health_check(self) -> bool:
        """Returns True if the API responds correctly."""
        try:
            d = self._post("health_check", timeout=6)
            return d.get("status") == "online"
        except Exception:
            return False

    def get_logs(self) -> List[Dict]:
        """Retrieves the list of logbooks for the user."""
        d = self._post("get_mylogs")
        if "error" in d:
            raise ValueError(d["error"])
        return d.get("logs", [])

    def create_qso(self, log_id: int, qso: Dict[str, Any]) -> Dict[str, Any]:
        """Creates a QSO in the CRX logbook. Raises ValueError if the API returns an error."""
        d = self._post("edit_myqso", extra={
            "qsoData": {
                "qso_id":              0,
                "f_log_id":            log_id,
                "logentry_his_call":   qso["dx_call"],
                "logentry_his_name":   qso.get("name", ""),
                "logentry_band":       qso["band"],
                "logentry_frequency":  qso["freq_khz"],
                "logentry_mode":       qso["mode"],
                "logentry_his_report": qso["rst_sent"],
                "logentry_my_report":  qso["rst_rcvd"],
                "logentry_comment":    qso.get("comment", ""),
            }
        })
        if "error" in d:
            raise ValueError(d["error"])
        return d

    def create_spot(self, qso: Dict[str, Any]) -> Dict[str, Any]:
        """
        Sends a DX spot to the DXCluster network via edit_myspot.
        HAM branch only (4-field payload). With an 11M key use
        send_spot_for_qso(), which routes to create_spot_11m().
        With a HAM key this queues an async task on the DXSpider network
        (the spot will not appear instantly via get_spots).
        """
        comment = f"{qso['mode']} Sent: {qso['rst_sent']} Rcvd: {qso['rst_rcvd']}"
        d = self._post("edit_myspot", extra={
            "spotData": {
                "callsign_dx":     qso["dx_call"],
                "callsign_sender": qso.get("my_call", "") or qso.get("wsjtx_id", ""),
                "frequency":       qso["freq_khz"],
                "contact_comment": comment,
            }
        })
        if "error" in d:
            raise ValueError(d["error"])
        return d

    def create_spot_11m(
        self,
        dx_call: str,
        sender_call: str,
        freq_khz: str,
        comment: str = "",
        band: str = "11",
        mode: str = "",
        report_parts: Tuple[str, str, str, str] = ("", "", "", ""),
        antpath: str = "",
    ) -> Dict[str, Any]:
        """
        Sends a DX spot via edit_myspot in 11M/CB mode.

        With an '11M-xxxx-xxxx-xxxx' API key, the CRX API routes edit_myspot
        into its CB branch (cbmode=true), which validates dx_call/sender_call
        against the French CB callsign format (e.g. '14CR000') instead of
        forwarding to the ham DXSpider network. The 4-field payload used by
        create_spot() (built for the HAM branch) is therefore rejected with
        an HTTP 400 under an 11M key; this method sends the full field set
        the CB branch actually expects.
        """
        d = self._post("edit_myspot", extra={
            "spotData": {
                "callsign_dx":              dx_call,
                "callsign_sender":          sender_call,
                "frequency":                freq_khz,
                "contact_comment":          comment,
                "band":                     band,
                "type":                     "",
                "mode":                     mode,
                "select_report_part1_form": report_parts[0],
                "select_report_part2_form": report_parts[1],
                "select_report_part3_form": report_parts[2],
                "select_report_part4_form": report_parts[3],
                "antpath":                  antpath,
            }
        })
        if "error" in d:
            raise ValueError(d["error"])
        return d

    def send_spot_for_qso(self, qso: Dict[str, Any], cb_sender: str = "") -> Dict[str, Any]:
        """
        Sends the spot of a QSO, picking the right branch automatically:
          - HAM key → create_spot()      (unchanged behaviour)
          - 11M key → create_spot_11m()  (full CB payload, band "11")

        cb_sender : CB callsign of the operator (e.g. '14CR004'). If empty,
                    falls back to the 'My call' reported by WSJT-X.
        """
        if not self.is_11m:
            return self.create_spot(qso)

        if qso.get("band") != "11m":
            raise ValueError(
                f"11M key: QSO is on {qso.get('band')}, spot skipped "
                "(only the 11m band is spotted in CB mode)")

        sender = (cb_sender or qso.get("my_call", "")).strip().upper()
        if not sender:
            raise ValueError(
                "11M key: no sender callsign — fill in 'CB callsign' "
                "or set 'My call' in WSJT-X")

        # 27315000 Hz -> "27315" (same format as the working test button)
        freq_khz = f"{qso['freq_hz'] / 1000.0:.3f}".rstrip("0").rstrip(".")
        comment  = f"{qso['mode']} Sent: {qso['rst_sent']} Rcvd: {qso['rst_rcvd']}"

        return self.create_spot_11m(
            dx_call=qso["dx_call"],
            sender_call=sender,
            freq_khz=freq_khz,
            comment=comment,
            band="11",
            mode=CB_SPOT_MODE,
        )


# ══════════════════════════════════════════════════════════════════════════════
#  §5  CONFIGURATION MANAGEMENT
# ══════════════════════════════════════════════════════════════════════════════

_CFG_DEFAULTS: Dict[str, Any] = {
    "api_key":    "",
    "udp_port":   2237,
    "log_id":     0,
    "log_name":   "",
    "auto_start": False,
    "auto_dxspot": False,
    "cb_callsign": "",
    "dedup":      True,
    "window_w":   720,
    "window_h":   660,
    "window_x":   120,
    "window_y":   80,
}


def _cfg_dir() -> str:
    """Returns the configuration directory (cross-platform)."""
    if sys.platform == "win32":
        base = os.environ.get("APPDATA", os.path.expanduser("~"))
    else:
        base = os.path.join(os.path.expanduser("~"), ".config")
    d = os.path.join(base, "wsjtx_crx_bridge")
    os.makedirs(d, exist_ok=True)
    return d


def _cfg_path() -> str:
    return os.path.join(_cfg_dir(), "config.json")


def load_cfg() -> Dict[str, Any]:
    cfg = dict(_CFG_DEFAULTS)
    try:
        with open(_cfg_path(), encoding="utf-8") as f:
            cfg.update(json.load(f))
    except Exception:
        pass
    return cfg


def save_cfg(cfg: Dict[str, Any]) -> None:
    with open(_cfg_path(), "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)


# ══════════════════════════════════════════════════════════════════════════════
#  §6  DEDUPLICATION MANAGER
# ══════════════════════════════════════════════════════════════════════════════

class DedupManager:
    """
    Prevents sending the same QSO twice.
    Dedup key: callsign + band + mode + UTC day.
    Keeps the last 500 keys in memory (simplified LRU).
    """

    _MAX = 500

    def __init__(self):
        self._buf: deque       = deque()
        self._set: Set[str]    = set()

    def _key(self, qso: Dict) -> str:
        day = qso["dt_off"].strftime("%Y%m%d") if qso.get("dt_off") else "00000000"
        return f"{qso['dx_call']}|{qso['band']}|{qso['mode']}|{day}"

    def is_duplicate(self, qso: Dict) -> bool:
        return self._key(qso) in self._set

    def register(self, qso: Dict) -> None:
        k = self._key(qso)
        if k in self._set:
            return
        self._buf.append(k)
        self._set.add(k)
        if len(self._buf) > self._MAX:
            self._set.discard(self._buf.popleft())


# ══════════════════════════════════════════════════════════════════════════════
#  §7  RETRY QUEUE (failed QSOs)
# ══════════════════════════════════════════════════════════════════════════════

class RetryQueue:
    """
    Queue for QSOs whose transmission failed.
    Each item is retried up to MAX_ATTEMPTS times with increasing delays.
    """

    MAX_ATTEMPTS = 5
    BASE_DELAY   = 30  # seconds between attempts

    def __init__(self):
        self._q: deque         = deque()
        self._lock             = threading.Lock()

    def push(self, qso: Dict, log_id: int) -> None:
        with self._lock:
            self._q.append({
                "qso":      qso,
                "log_id":   log_id,
                "attempts": 1,
                "next_at":  time.monotonic() + self.BASE_DELAY,
            })

    def pop_due(self) -> Optional[Dict]:
        """Returns and removes the first item whose retry time has elapsed."""
        with self._lock:
            if self._q and self._q[0]["next_at"] <= time.monotonic():
                return self._q.popleft()
        return None

    def requeue(self, item: Dict) -> None:
        """Puts an item back in the queue if it hasn't exceeded MAX_ATTEMPTS."""
        item["attempts"] += 1
        if item["attempts"] <= self.MAX_ATTEMPTS:
            item["next_at"] = time.monotonic() + self.BASE_DELAY * item["attempts"]
            with self._lock:
                self._q.appendleft(item)

    def size(self) -> int:
        with self._lock:
            return len(self._q)


# ══════════════════════════════════════════════════════════════════════════════
#  §8  FILE LOGGER
# ══════════════════════════════════════════════════════════════════════════════

def _build_file_logger() -> logging.Logger:
    log_file = os.path.join(_cfg_dir(), "bridge.log")
    logger   = logging.getLogger("wsjtx_crx")
    logger.setLevel(logging.DEBUG)
    if not logger.handlers:
        fh = logging.FileHandler(log_file, encoding="utf-8")
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(logging.Formatter(
            "%(asctime)s  %(levelname)-7s  %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S"
        ))
        logger.addHandler(fh)
    return logger


_flog = _build_file_logger()


# ══════════════════════════════════════════════════════════════════════════════
#  §9  GUI APPLICATION
# ══════════════════════════════════════════════════════════════════════════════

class BridgeApp(tk.Tk):
    """Main window of the WSJTX-CRX Bridge."""

    # Fixed test-spot payload for a quick 11M/CB smoke test (🧪 Test Spot 11M
    # button). Edit these values to try something else — this spot is not
    # linked to any QSO, it only exercises the edit_myspot/CB code path.
    TEST_SPOT_11M: Dict[str, str] = {
        "dx_call":     "14CR000",
        "sender_call": "14CR004",
        "freq_khz":    "27315",
        "comment":     "sorry spot-wsjt-test",
        "band":        "11",
        "mode":        "",
    }

    def __init__(self):
        super().__init__()

        # -- State -------------------------------------------------------------
        self.cfg      = load_cfg()
        self.api:     Optional[CrxApiClient] = None
        self._sock:   Optional[socket.socket] = None
        self.running  = False
        self._mq      = queue.Queue()         # threads → GUI thread messages

        self.logs_list: List[Tuple[int, str]] = []
        self.dedup    = DedupManager()
        self.retry_q  = RetryQueue()

        # Session counters
        self._total  = 0
        self._by_band: Dict[str, int] = defaultdict(int)
        self._by_mode: Dict[str, int] = defaultdict(int)

        # -- Build -------------------------------------------------------------
        self._build_style()
        self._build_ui()
        self._apply_cfg()

        # -- Recurring timers --------------------------------------------------
        self.after(100,  self._drain_queue)
        self.after(5000, self._tick_retry)

        # -- Auto-start or welcome message -------------------------------------
        if self.cfg.get("auto_start") and self.cfg.get("api_key"):
            self.after(500, self._auto_start)
        else:
            self.after(300, self._print_welcome)

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.1  THEME / ttk STYLE
    # ══════════════════════════════════════════════════════════════════════════

    def _build_style(self) -> None:
        s = ttk.Style(self)
        s.theme_use("clam")

        # Global base
        s.configure(".",
            background=BG_DARK, foreground=FG_TEXT,
            troughcolor=BG_PANEL, selectbackground=BG_ENTRY,
            selectforeground=FG_TEXT, fieldbackground=BG_ENTRY,
            bordercolor=BG_PANEL, darkcolor=BG_PANEL, lightcolor=BG_PANEL,
            insertcolor=FG_TEXT,
        )

        # Notebook / tabs
        s.configure("TNotebook",    background=BG_DARK, bordercolor=BG_PANEL)
        s.configure("TNotebook.Tab",
            background=BG_PANEL, foreground=FG_DIM,
            padding=[12, 4], font=("Arial", 9))
        s.map("TNotebook.Tab",
            background=[("selected", BG_ENTRY)],
            foreground=[("selected", FG_TEXT)])

        # Treeview
        s.configure("Treeview",
            background=BG_TABLE, foreground=FG_TEXT,
            fieldbackground=BG_TABLE, rowheight=23,
            font=("Courier New", 9))
        s.configure("Treeview.Heading",
            background=BG_PANEL, foreground=FG_HDR,
            font=("Arial", 9, "bold"), relief="flat")
        s.map("Treeview",
            background=[("selected", "#354060")],
            foreground=[("selected", FG_TEXT)])

        # Combobox
        s.configure("TCombobox",
            fieldbackground=BG_ENTRY, background=BG_ENTRY,
            foreground=FG_TEXT, selectbackground=BG_ENTRY,
            selectforeground=FG_TEXT, arrowcolor=FG_DIM)
        s.map("TCombobox",
            fieldbackground=[("readonly", BG_ENTRY)],
            foreground=[("readonly", FG_TEXT)])

        # Scrollbar
        s.configure("TScrollbar",
            background=BG_PANEL, troughcolor=BG_DARK,
            arrowcolor=FG_DIM, bordercolor=BG_PANEL)

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.2  UI CONSTRUCTION
    # ══════════════════════════════════════════════════════════════════════════

    def _build_ui(self) -> None:
        self.title(f"{APP_NAME}  v{APP_VERSION}")
        self.configure(bg=BG_DARK)
        self.minsize(660, 580)

        # Fonts
        f9  = ("Arial", 9)
        f9b = ("Arial", 9, "bold")
        f10b = ("Arial", 10, "bold")
        f14b = ("Arial", 14, "bold")
        mono = ("Courier New", 9)

        # -- Header ------------------------------------------------------------
        hdr = tk.Frame(self, bg=BG_DARK)
        hdr.pack(fill=tk.X, padx=12, pady=(10, 2))

        self._lbl_pending = tk.Label(hdr, text="",
                                     font=f9, fg=C_AMBER, bg=BG_DARK)
        self._lbl_pending.pack(side=tk.RIGHT, padx=4)

        # -- Configuration panel -----------------------------------------------
        pnl = tk.LabelFrame(self, text=" ⚙  Configuration ",
                             bg=BG_PANEL, fg=FG_HDR, font=f9b,
                             bd=1, relief=tk.GROOVE)
        pnl.pack(fill=tk.X, padx=12, pady=6)

        # Row 1: API key
        r1 = tk.Frame(pnl, bg=BG_PANEL)
        r1.pack(fill=tk.X, padx=8, pady=(6, 2))

        tk.Label(r1, text="CRX API Key:", width=15, anchor="w",
                 font=f9, fg=FG_TEXT, bg=BG_PANEL).pack(side=tk.LEFT)

        self._sv_key = tk.StringVar()
        self._e_key  = tk.Entry(
            r1, textvariable=self._sv_key, show="•",
            width=36, font=mono,
            bg=BG_ENTRY, fg=FG_TEXT, insertbackground=FG_TEXT,
            relief=tk.FLAT, bd=4)
        self._e_key.pack(side=tk.LEFT, padx=4)

        tk.Button(r1, text="👁", font=f9, relief=tk.FLAT,
                  bg=BG_ENTRY, fg=FG_DIM, cursor="hand2",
                  command=self._toggle_key_visibility).pack(side=tk.LEFT, padx=2)

        self._btn_test = tk.Button(
            r1, text="🔌 Test", font=f9,
            relief=tk.FLAT, padx=8,
            bg="#1e3a5f", fg=FG_TEXT, cursor="hand2",
            command=self._test_api_connection)
        self._btn_test.pack(side=tk.LEFT, padx=6)

        # Row 2: UDP port + logbook
        r2 = tk.Frame(pnl, bg=BG_PANEL)
        r2.pack(fill=tk.X, padx=8, pady=2)

        tk.Label(r2, text="UDP Port:", width=15, anchor="w",
                 font=f9, fg=FG_TEXT, bg=BG_PANEL).pack(side=tk.LEFT)

        self._sv_port = tk.StringVar(value="2237")
        tk.Entry(
            r2, textvariable=self._sv_port, width=7, font=mono,
            bg=BG_ENTRY, fg=FG_TEXT, insertbackground=FG_TEXT,
            relief=tk.FLAT, bd=4).pack(side=tk.LEFT, padx=4)

        tk.Label(r2, text="Logbook:", font=f9,
                 fg=FG_TEXT, bg=BG_PANEL).pack(side=tk.LEFT, padx=(12, 4))

        self._sv_log = tk.StringVar()
        self._cb_log = ttk.Combobox(
            r2, textvariable=self._sv_log,
            width=26, state="readonly", font=f9)
        self._cb_log.pack(side=tk.LEFT)

        self._btn_rlogs = tk.Button(
            r2, text=" 🔄 ", font=f9, relief=tk.FLAT,
            bg=BG_ENTRY, fg=FG_DIM, cursor="hand2",
            command=self._fetch_logs)
        self._btn_rlogs.pack(side=tk.LEFT, padx=3)

        # Row 2b: auto-dxspot (below UDP Port)
        r2b = tk.Frame(pnl, bg=BG_PANEL)
        r2b.pack(fill=tk.X, padx=8, pady=(2, 0))

        self._sv_autospot = tk.BooleanVar(value=False)

        tk.Checkbutton(
            r2b, text="auto-dxspot  (also send a spot to the DXCluster for each QSO)",
            variable=self._sv_autospot,
            font=f9, fg=FG_DIM, bg=BG_PANEL,
            selectcolor=BG_ENTRY, activebackground=BG_PANEL,
        ).pack(side=tk.LEFT)

        # Row 2c: CB callsign (sender of the 11M spots)
        r2c = tk.Frame(pnl, bg=BG_PANEL)
        r2c.pack(fill=tk.X, padx=8, pady=(2, 0))

        tk.Label(r2c, text="CB callsign (11M):", width=15, anchor="w",
                 font=f9, fg=FG_TEXT, bg=BG_PANEL).pack(side=tk.LEFT)

        self._sv_cbcall = tk.StringVar()
        tk.Entry(
            r2c, textvariable=self._sv_cbcall, width=12, font=mono,
            bg=BG_ENTRY, fg=FG_TEXT, insertbackground=FG_TEXT,
            relief=tk.FLAT, bd=4).pack(side=tk.LEFT, padx=4)

        tk.Label(r2c, text="(sender of the spots, e.g. 14CR004 — 11M keys only)",
                 font=f9, fg=FG_DIM, bg=BG_PANEL).pack(side=tk.LEFT, padx=6)

        # Row 3: options
        r3 = tk.Frame(pnl, bg=BG_PANEL)
        r3.pack(fill=tk.X, padx=8, pady=(2, 8))

        self._sv_auto  = tk.BooleanVar()
        self._sv_dedup = tk.BooleanVar(value=True)

        tk.Checkbutton(
            r3, text="Auto-start",
            variable=self._sv_auto,
            font=f9, fg=FG_DIM, bg=BG_PANEL,
            selectcolor=BG_ENTRY, activebackground=BG_PANEL,
        ).pack(side=tk.LEFT)

        tk.Checkbutton(
            r3, text="Duplicate filter (call / band / mode / UTC day)",
            variable=self._sv_dedup,
            font=f9, fg=FG_DIM, bg=BG_PANEL,
            selectcolor=BG_ENTRY, activebackground=BG_PANEL,
        ).pack(side=tk.LEFT, padx=18)

        # -- Control bar -------------------------------------------------------
        ctrl = tk.Frame(self, bg=BG_DARK)
        ctrl.pack(fill=tk.X, padx=12, pady=4)

        self._btn_start = tk.Button(
            ctrl, text="▶  START", font=f10b,
            bg="#1a4731", fg="white", relief=tk.FLAT,
            padx=18, pady=6, cursor="hand2",
            command=self._start_bridge)
        self._btn_start.pack(side=tk.LEFT, padx=(0, 6))

        self._btn_stop = tk.Button(
            ctrl, text="⏹  STOP", font=f10b,
            bg="#5c1a1a", fg="white", relief=tk.FLAT,
            padx=18, pady=6, cursor="hand2",
            state=tk.DISABLED, command=self._stop_bridge)
        self._btn_stop.pack(side=tk.LEFT, padx=(0, 12))

        # Status LED
        self._cnv = tk.Canvas(ctrl, width=18, height=18,
                              bg=BG_DARK, highlightthickness=0)
        self._cnv.pack(side=tk.LEFT, pady=4)
        self._led = self._cnv.create_oval(2, 2, 16, 16, fill=C_RED, outline="")

        self._lbl_status = tk.Label(
            ctrl, text="Stopped", font=f10b, fg=C_RED, bg=BG_DARK)
        self._lbl_status.pack(side=tk.LEFT, padx=8)

        # Simulate button (test without WSJT-X)
        tk.Button(
            ctrl, text="🧪 Simulate QSO", font=f9,
            bg=BG_PANEL, fg=FG_DIM, relief=tk.FLAT,
            cursor="hand2", padx=6,
            command=self._simulate_qso).pack(side=tk.RIGHT, padx=6)

        # Test Spot 11M button (fixed CB-format test spot, no QSO involved)
        tk.Button(
            ctrl, text="🧪 Test Spot 11M", font=f9,
            bg=BG_PANEL, fg=FG_DIM, relief=tk.FLAT,
            cursor="hand2", padx=6,
            command=self._send_test_spot_11m).pack(side=tk.RIGHT, padx=6)

        # Session counter
        self._lbl_count = tk.Label(
            ctrl, text="Session: 0 QSOs",
            font=f9, fg=FG_DIM, bg=BG_DARK)
        self._lbl_count.pack(side=tk.RIGHT, padx=6)

        # -- Notebook (Log / QSOs) ---------------------------------------------
        nb = ttk.Notebook(self)
        nb.pack(fill=tk.BOTH, expand=True, padx=12, pady=(4, 2))

        # ·· Log tab ···························································
        tab_log = tk.Frame(nb, bg=BG_DARK)
        nb.add(tab_log, text="  📋  Log  ")

        self._txt = scrolledtext.ScrolledText(
            tab_log, state=tk.DISABLED, height=12,
            bg="#0d1117", fg=FG_TEXT, font=mono,
            insertbackground=FG_TEXT, relief=tk.FLAT, wrap=tk.WORD)
        self._txt.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        for tag, col, extra in [
            ("ok",     C_GREEN,  {}),
            ("err",    C_RED,    {}),
            ("warn",   C_AMBER,  {}),
            ("info",   C_BLUE,   {}),
            ("dim",    FG_DIM,   {}),
            ("purple", C_PURPLE, {}),
            ("qso",    "#f0fff4", {"font": ("Courier New", 9, "bold")}),
        ]:
            self._txt.tag_config(tag, foreground=col, **extra)

        btn_row = tk.Frame(tab_log, bg=BG_DARK)
        btn_row.pack(fill=tk.X, padx=4, pady=(0, 4))
        tk.Button(
            btn_row, text="Clear log", font=f9,
            relief=tk.FLAT, bg=BG_PANEL, fg=FG_DIM, cursor="hand2",
            command=self._clear_log).pack(side=tk.RIGHT)

        # ·· Session QSOs tab ··············································
        tab_qso = tk.Frame(nb, bg=BG_DARK)
        nb.add(tab_qso, text="  📡  Session QSOs  ")

        cols = ("time", "callsign", "band", "mode", "rst_s", "rst_r", "status")
        self._tree = ttk.Treeview(
            tab_qso, columns=cols, show="headings",
            height=12, selectmode="browse")

        col_cfg = [
            ("time",     "UTC Time",   100, "center"),
            ("callsign", "Callsign",   115, "w"),
            ("band",     "Band",        68, "center"),
            ("mode",     "Mode",        68, "center"),
            ("rst_s",    "RST snt",     60, "center"),
            ("rst_r",    "RST rcv",     60, "center"),
            ("status",   "Status",     155, "w"),
        ]
        for cid, lbl, w, anch in col_cfg:
            self._tree.heading(cid, text=lbl)
            self._tree.column(cid, width=w, anchor=anch, stretch=False)

        # Row colour tags
        self._tree.tag_configure("ok",  background=C_ROW_OK)
        self._tree.tag_configure("err", background=C_ROW_ERR)
        self._tree.tag_configure("pnd", background=C_ROW_PND)

        vsb = ttk.Scrollbar(tab_qso, orient="vertical",
                            command=self._tree.yview)
        self._tree.configure(yscrollcommand=vsb.set)
        vsb.pack(side=tk.RIGHT, fill=tk.Y)
        self._tree.pack(fill=tk.BOTH, expand=True, padx=4, pady=4)

        # Band/mode stats line below the table
        self._lbl_stats = tk.Label(
            tab_qso, text="",
            font=("Courier New", 8), fg=FG_DIM, bg=BG_DARK)
        self._lbl_stats.pack(fill=tk.X, padx=4, pady=(0, 4))

        # -- Status bar (bottom of window) -------------------------------------
        self._lbl_bar = tk.Label(
            self, text="Ready.",
            font=("Arial", 8), fg=FG_DIM, bg="#131720", anchor="w")
        self._lbl_bar.pack(fill=tk.X, side=tk.BOTTOM, padx=6, pady=2)

        self.protocol("WM_DELETE_WINDOW", self._on_close)

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.3  CONFIGURATION MANAGEMENT
    # ══════════════════════════════════════════════════════════════════════════

    def _apply_cfg(self) -> None:
        c = self.cfg
        self._sv_key.set(c.get("api_key", ""))
        self._sv_port.set(str(c.get("udp_port", 2237)))
        self._sv_auto.set(c.get("auto_start", False))
        self._sv_autospot.set(c.get("auto_dxspot", False))
        self._sv_cbcall.set(c.get("cb_callsign", ""))
        self._sv_dedup.set(c.get("dedup", True))
        geo = (
            f"{c.get('window_w', 720)}x{c.get('window_h', 660)}"
            f"+{c.get('window_x', 120)}+{c.get('window_y', 80)}"
        )
        self.geometry(geo)

    def _snapshot_cfg(self) -> None:
        log_id, log_name = self._selected_log()
        self.cfg.update({
            "api_key":    self._sv_key.get().strip(),
            "udp_port":   self._get_port(),
            "log_id":     log_id,
            "log_name":   log_name,
            "auto_start": self._sv_auto.get(),
            "auto_dxspot": self._sv_autospot.get(),
            "cb_callsign": self._sv_cbcall.get().strip().upper(),
            "dedup":      self._sv_dedup.get(),
            "window_w":   self.winfo_width(),
            "window_h":   self.winfo_height(),
            "window_x":   self.winfo_x(),
            "window_y":   self.winfo_y(),
        })
        save_cfg(self.cfg)

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.4  UI HELPERS
    # ══════════════════════════════════════════════════════════════════════════

    def _get_port(self) -> int:
        try:
            return max(1024, min(65535, int(self._sv_port.get().strip())))
        except ValueError:
            return 2237

    def _selected_log(self) -> Tuple[int, str]:
        sel = self._sv_log.get()
        for lid, lname in self.logs_list:
            if lname == sel:
                return lid, lname
        return 0, ""

    def _toggle_key_visibility(self) -> None:
        self._e_key.config(
            show="" if self._e_key.cget("show") == "•" else "•")

    # -- GUI log ---------------------------------------------------------------

    def _log(self, msg: str, tag: str = "") -> None:
        ts = datetime.now().strftime("%H:%M:%S")
        self._txt.config(state=tk.NORMAL)
        self._txt.insert(tk.END, f"[{ts}] ", "dim")
        self._txt.insert(tk.END, msg + "\n", tag or "")
        self._txt.config(state=tk.DISABLED)
        self._txt.see(tk.END)

    def _clear_log(self) -> None:
        self._txt.config(state=tk.NORMAL)
        self._txt.delete("1.0", tk.END)
        self._txt.config(state=tk.DISABLED)

    def _set_led(self, text: str, fg: str, led_color: str) -> None:
        self._lbl_status.config(text=text, fg=fg)
        self._cnv.itemconfig(self._led, fill=led_color)

    def _statusbar(self, msg: str) -> None:
        self._lbl_bar.config(text=msg)

    def _refresh_counters(self) -> None:
        self._lbl_count.config(text=f"Session: {self._total} QSOs")
        bands = "  ".join(f"{b}:{n}" for b, n in sorted(self._by_band.items()))
        modes = "  ".join(f"{m}:{n}" for m, n in sorted(self._by_mode.items()))
        self._lbl_stats.config(text=f"{bands}    {modes}".strip())
        p = self.retry_q.size()
        self._lbl_pending.config(
            text=f"⏳ {p} pending (retry)" if p else "")

    # -- QSO Treeview ---------------------------------------------------------

    def _tree_add(self, qso: Dict, status: str, tag: str, iid: str = "") -> str:
        """
        Adds (iid='') or updates (iid provided) a row in the QSO table.
        Returns the row iid.
        """
        dt = qso.get("dt_off")
        ts = dt.strftime("%H:%M:%SZ") if dt else "--:--:--Z"
        vals = (
            ts,
            qso.get("dx_call", ""),
            qso.get("band", ""),
            qso.get("mode", ""),
            qso.get("rst_sent", ""),
            qso.get("rst_rcvd", ""),
            status,
        )
        if iid:
            try:
                self._tree.item(iid, values=vals, tags=(tag,))
            except tk.TclError:
                pass            # row deleted in the meantime
        else:
            iid = self._tree.insert("", 0, values=vals, tags=(tag,))
            # Keep history to 250 rows
            kids = self._tree.get_children()
            if len(kids) > 250:
                self._tree.delete(kids[-1])
        return iid

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.5  USER ACTIONS — API
    # ══════════════════════════════════════════════════════════════════════════

    def _test_api_connection(self) -> None:
        key = self._sv_key.get().strip()
        if not key:
            messagebox.showwarning("API Key", "Please enter your CRX API key.")
            return
        self._log("Testing CRX API connection…", "info")
        self._btn_test.config(state=tk.DISABLED)

        def _run():
            c  = CrxApiClient(key)
            ok = c.health_check()
            self._mq.put(("api_ok", c) if ok else ("api_fail", None))

        threading.Thread(target=_run, daemon=True).start()

    def _fetch_logs(self) -> None:
        key = self._sv_key.get().strip()
        if not key:
            messagebox.showwarning("API Key",
                                   "Please enter your CRX API key first.")
            return
        self._log("Loading logbooks…", "info")
        self._btn_rlogs.config(state=tk.DISABLED)

        def _run():
            try:
                logs = CrxApiClient(key).get_logs()
                self._mq.put(("logs_ok", logs))
            except Exception as e:
                self._mq.put(("logs_fail", str(e)))

        threading.Thread(target=_run, daemon=True).start()

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.6  START / STOP BRIDGE
    # ══════════════════════════════════════════════════════════════════════════

    def _start_bridge(self) -> None:
        key = self._sv_key.get().strip()
        if not key:
            messagebox.showwarning("Configuration",
                                   "Please enter your CRX API key.")
            return
        log_id, log_name = self._selected_log()
        if log_id == 0:
            messagebox.showwarning("Configuration",
                "Please select a target logbook.\n"
                "(Use the 🔄 button to load your logbooks)")
            return
        port = self._get_port()

        self._snapshot_cfg()
        self.api     = CrxApiClient(key)
        self.running = True

        if self.api.is_11m:
            self._log("🔑  11M key detected → CB mode "
                      "(spots sent with band '11')", "purple")
            if self._sv_autospot.get() and not self._sv_cbcall.get().strip():
                self._log("   ⚠  auto-dxspot: no CB callsign set — "
                          "WSJT-X 'My call' will be used as sender", "warn")

        t = threading.Thread(
            target=self._udp_thread,
            args=(port, log_id),
            daemon=True,
            name="wsjtx-udp-listener")
        t.start()

        self._btn_start.config(state=tk.DISABLED)
        self._btn_stop.config(state=tk.NORMAL)
        self._set_led(f"Listening :{port}", C_AMBER, C_AMBER)
        self._statusbar(
            f"Bridge active  —  UDP:{port}  →  [{log_id}] {log_name}")
        self._log(f"Starting: UDP :{port} → [{log_id}] {log_name}", "info")
        _flog.info(f"Bridge started  UDP:{port}  log_id:{log_id}  log:{log_name}")

    def _stop_bridge(self) -> None:
        self.running = False
        if self._sock:
            try:
                self._sock.close()
            except Exception:
                pass
        self._btn_start.config(state=tk.NORMAL)
        self._btn_stop.config(state=tk.DISABLED)
        self._set_led("Stopped", C_RED, C_RED)
        self._statusbar("Bridge stopped.")
        self._log("Bridge stopped.", "warn")
        _flog.info("Bridge stopped")

    def _auto_start(self) -> None:
        sid   = self.cfg.get("log_id", 0)
        sname = self.cfg.get("log_name", "")
        if sid and sname:
            self.logs_list = [(sid, sname)]
            self._cb_log["values"] = [sname]
            self._cb_log.set(sname)
            self._start_bridge()
        else:
            self._fetch_logs()

    def _print_welcome(self) -> None:
        self._log(f"═══  {APP_NAME}  v{APP_VERSION}  ═══", "info")
        self._log("-" * 44, "dim")
        self._log("1.  Enter your CRX API key  → 🔌 Test",    "dim")
        self._log("2.  Load your logbooks      → 🔄",          "dim")
        self._log("3.  Select a logbook",                       "dim")
        self._log("4.  Start the bridge        → ▶  START",    "dim")
        self._log("-" * 44, "dim")
        self._log("WSJT-X setup: File → Settings → Reporting", "dim")
        self._log("  UDP Server: 127.0.0.1   Port: 2237",      "dim")
        self._log("  ☑ Accept UDP requests",                    "dim")
        self._log("-" * 44, "dim")

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.7  UDP LISTENER THREAD
    # ══════════════════════════════════════════════════════════════════════════

    def _udp_thread(self, port: int, log_id: int) -> None:
        """
        Runs in a dedicated thread.
        Listens on the WSJT-X UDP port and pushes parsed packets into the message queue.
        """

        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.settimeout(1.0)

            self._log(
                f"[WSJT-X UDP] Listening on 127.0.0.1:{port}",
                "info"
            )

            s.bind(("127.0.0.1", port))
            self._sock = s

            self._mq.put(("udp_up", port))

            while self.running:
                try:
                    # This socket can only receive packets destined for
                    # 127.0.0.1:<port>
                    data, (src_ip, src_port) = s.recvfrom(65535)

                    # Log ONLY packets actually received by this socket.
                    self._log(
                        f"[WSJT-X UDP] RX {len(data)} bytes "
                        f"from {src_ip}:{src_port} "
                        f"→ 127.0.0.1:{port}",
                        "info"
                    )

                    # Raw packet, useful for WSJT-X protocol debugging.
                    self._log(
                        f"[WSJT-X UDP] HEX: {data.hex(' ')}",
                        "info"
                    )

                    qso = decode_wsjtx_packet(data)

                    if qso:
                        self._log(
                            f"[WSJT-X UDP] Decoded: {qso}",
                            "info"
                        )

                        self._mq.put(
                            ("qso_in", qso, log_id, src_ip)
                        )
                    else:
                        self._log(
                            "[WSJT-X UDP] Packet received "
                            "but not decoded",
                            "warn"
                        )

                except socket.timeout:
                    # Normal timeout: do not log anything.
                    continue

                except OSError as e:
                    self._log(
                        f"[WSJT-X UDP] Socket error: {e}",
                        "error"
                    )
                    break

        except OSError as e:
            self._log(
                f"[WSJT-X UDP] Unable to listen on "
                f"127.0.0.1:{port}: {e}",
                "error"
            )

            self._mq.put(("udp_err", str(e)))

        finally:
            try:
                if self._sock:
                    self._sock.close()
            except Exception:
                pass
    
    # ══════════════════════════════════════════════════════════════════════════
    #  §9.8  SEND QSO TO CRX
    # ══════════════════════════════════════════════════════════════════════════

    def _send_qso(self, qso: Dict, log_id: int, iid: str) -> None:
        """Starts a thread to send the QSO to the CRX API."""
        def _run():
            try:
                result  = self.api.create_qso(log_id, qso)
                qso_id  = result.get("qso_id", "?")
                self._mq.put(("qso_ok", qso, qso_id, iid))
                _flog.info(
                    f"QSO sent  {qso['dx_call']}  {qso['band']}  "
                    f"{qso['mode']}  id:{qso_id}")
            except Exception as e:
                self._mq.put(("qso_err", qso, log_id, str(e), iid))
                _flog.error(f"QSO send failed  {qso['dx_call']}  {e}")

        threading.Thread(target=_run, daemon=True).start()

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.8b  SEND DX SPOT TO CRX  (auto-dxspot option)
    # ══════════════════════════════════════════════════════════════════════════

    def _send_spot(self, qso: Dict) -> None:
        """
        Starts a thread to send a DX spot to the CRX DXCluster (edit_myspot).
        Only called when the 'auto-dxspot' checkbox is enabled — runs
        independently of the logbook QSO send, so one failing does not
        block the other.

        The API client picks the right payload from the key type:
        HAM key → ham spot, 11M key → 11M/CB spot (see send_spot_for_qso).
        """
        # Tk variables are read here (GUI thread), not from the network thread
        cb_sender = self._sv_cbcall.get().strip()

        def _run():
            try:
                self.api.send_spot_for_qso(qso, cb_sender)
                self._mq.put(("spot_ok", qso))
                _flog.info(
                    f"Spot sent  {qso['dx_call']}  {qso['band']}  {qso['mode']}")
            except Exception as e:
                self._mq.put(("spot_err", qso, str(e)))
                _flog.error(f"Spot send failed  {qso['dx_call']}  {e}")

        threading.Thread(target=_run, daemon=True).start()

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.8c  SEND 11M TEST SPOT TO CRX  (🧪 Test Spot 11M button)
    # ══════════════════════════════════════════════════════════════════════════

    def _send_test_spot_11m(self) -> None:
        """
        Sends a fixed test spot in the 11M/CB format (see TEST_SPOT_11M),
        independently of any QSO. Meant to verify that edit_myspot works
        correctly with an 11M-xxxx-xxxx-xxxx API key: under that key type
        the server validates callsigns against the French CB format
        (e.g. '14CR000'), so a normal ham callsign always gets rejected
        there — this button lets you confirm the pipeline itself is fine.
        """
        if not self.api:
            messagebox.showwarning(
                "Test Spot 11M",
                "Please test your CRX API key first (🔌 Test).")
            return

        t = self.TEST_SPOT_11M
        self._log(
            f"🧪  Sending 11M test spot:  "
            f"{t['sender_call']} → {t['dx_call']}  "
            f"{t['freq_khz']} kHz  \"{t['comment']}\"", "info")

        def _run():
            try:
                self.api.create_spot_11m(
                    dx_call=t["dx_call"],
                    sender_call=t["sender_call"],
                    freq_khz=t["freq_khz"],
                    comment=t["comment"],
                    band=t["band"],
                    mode=t["mode"],
                )
                self._mq.put(("spot11_ok", t))
                _flog.info(f"11M test spot sent  {t['dx_call']}")
            except Exception as e:
                self._mq.put(("spot11_err", t, str(e)))
                _flog.error(f"11M test spot failed  {t['dx_call']}  {e}")

        threading.Thread(target=_run, daemon=True).start()

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.9  RETRY LOOP
    # ══════════════════════════════════════════════════════════════════════════

    def _tick_retry(self) -> None:
        """Called every 5 seconds by after()."""
        item = self.retry_q.pop_due()
        if item and self.api:
            qso    = item["qso"]
            log_id = item["log_id"]
            att    = item["attempts"]
            self._log(
                f"↩  Retry [{att}/{RetryQueue.MAX_ATTEMPTS}] "
                f"{qso['dx_call']}  {qso['band']}  {qso['mode']}…",
                "warn")

            def _run():
                try:
                    r = self.api.create_qso(log_id, qso)
                    self._mq.put(("retry_ok", qso, r.get("qso_id", "?")))
                    _flog.info(f"Retry OK  {qso['dx_call']}")
                except Exception as e:
                    self.retry_q.requeue(item)
                    self._mq.put(("retry_fail", qso, str(e)))
                    _flog.warning(f"Retry fail  {qso['dx_call']}  {e}")

            threading.Thread(target=_run, daemon=True).start()

        self._refresh_counters()
        self.after(5000, self._tick_retry)

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.10  QSO SIMULATION (test without WSJT-X)
    # ══════════════════════════════════════════════════════════════════════════

    def _simulate_qso(self) -> None:
        """Injects a fake FT8 QSO to test the pipeline without WSJT-X."""
        import random
        CALLS  = ["W1AW", "DL1ABC", "JA1XYZ", "VK2TST", "F5XYZ", "PA3ABC",
                  "UR5XYZ", "SP1ABC", "LU3ABC", "PY2TST"]
        GRIDS  = ["JN18eu", "EM50", "PM95", "QF44", "FN31", "JO22"]
        BANDS  = ["20m", "40m", "15m", "10m", "17m", "6m"]
        MODES  = ["FT8", "FT4", "JT65", "Q65"]
        FREQS  = {"20m": 14_074_000, "40m": 7_074_000, "15m": 21_074_000,
                  "10m": 28_074_000, "17m": 18_100_000, "6m": 50_313_000}

        band   = random.choice(BANDS)
        mode   = random.choice(MODES)
        freq   = FREQS.get(band, 14_074_000)

        log_id, _ = self._selected_log()
        if log_id == 0:
            messagebox.showwarning(
                "Simulation", "Please select a target logbook first.")
            return

        qso: Dict[str, Any] = {
            "dx_call":   random.choice(CALLS),
            "dx_grid":   random.choice(GRIDS),
            "freq_hz":   freq,
            "freq_khz":  hz_to_khz_str(freq),
            "band":      band,
            "mode":      mode,
            "rst_sent":  f"{random.randint(-20, +5):+d}",
            "rst_rcvd":  f"{random.randint(-20, +5):+d}",
            "name":      "Test",
            "comment":   "WSJTX-CRX Bridge Simulation",
            "dt_off":    datetime.now(tz=timezone.utc),
            "dt_on":     datetime.now(tz=timezone.utc),
            "my_call":   "F4TEST",
            "my_grid":   "JN18eu",
            "wsjtx_id":  "SIMULATE",
            "schema":    3,
        }
        self._mq.put(("qso_in", qso, log_id, "127.0.0.1"))

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.11  DRAIN MESSAGE QUEUE (GUI thread)
    # ══════════════════════════════════════════════════════════════════════════

    def _drain_queue(self) -> None:
        """Processes all pending messages in the queue — called by after()."""
        try:
            while True:
                self._dispatch(self._mq.get_nowait())
        except queue.Empty:
            pass
        finally:
            self.after(80, self._drain_queue)

    def _dispatch(self, msg: tuple) -> None:  # noqa: C901
        k = msg[0]

        # -- API test ----------------------------------------------------------
        if k == "api_ok":
            self.api = msg[1]
            self._btn_test.config(state=tk.NORMAL)
            self._log("✅  CRX API connected successfully!", "ok")
            self._fetch_logs()

        elif k == "api_fail":
            self._btn_test.config(state=tk.NORMAL)
            self._log("❌  API connection failed — check your key and network.", "err")

        # -- Logbooks ----------------------------------------------------------
        elif k == "logs_ok":
            logs = msg[1]
            self._btn_rlogs.config(state=tk.NORMAL)
            self.logs_list = [(l["log_id"], l["log_name"]) for l in logs]
            names = [l["log_name"] for l in logs]
            self._cb_log["values"] = names
            saved = self.cfg.get("log_name", "")
            self._cb_log.set(
                saved if saved in names else (names[0] if names else ""))
            self._log(f"✅  {len(logs)} logbook(s) loaded.", "ok")
            for l in logs:
                self._log(f"   [{l['log_id']:>5}]  {l['log_name']}", "dim")

        elif k == "logs_fail":
            self._btn_rlogs.config(state=tk.NORMAL)
            self._log(f"❌  Error loading logbooks: {msg[1]}", "err")

        # -- UDP ---------------------------------------------------------------
        elif k == "udp_up":
            port = msg[1]
            self._log(
                f"✅  UDP listener active on :{port} — "
                "waiting for WSJT-X QSOs…", "ok")
            self._set_led(f"Active :{port}", C_GREEN, C_GREEN)

        elif k == "udp_err":
            err = msg[1]
            self._log(f"❌  UDP error: {err}", "err")
            if "10048" in err or "already in use" in err.lower():
                self._log(
                    "   → Port already in use. "
                    "Change the port or close the other application.", "warn")
            self._stop_bridge()

        # -- Incoming QSO ------------------------------------------------------
        elif k == "qso_in":
            qso, log_id, src = msg[1], msg[2], msg[3]

            # Duplicate filter
            if self._sv_dedup.get() and self.dedup.is_duplicate(qso):
                self._log(
                    f"⚠  Duplicate ignored:  "
                    f"{qso['dx_call']}  {qso['band']}  {qso['mode']}", "warn")
                _flog.debug(
                    f"Dedup skip  {qso['dx_call']}  {qso['band']}  {qso['mode']}")
                return

            self.dedup.register(qso)

            dt  = qso.get("dt_off")
            ts  = dt.strftime("%H:%M:%SZ") if dt else "--:--Z"
            self._log(
                f"📡  {ts}  "
                f"{qso['dx_call']:<12s}  {qso['band']:<5s}  {qso['mode']:<7s}  "
                f"{qso['freq_khz']} kHz  "
                f"{qso['rst_sent']} / {qso['rst_rcvd']}",
                "qso")

            iid = self._tree_add(qso, "⏳ sending…", "pnd")
            self._send_qso(qso, log_id, iid)

            # auto-dxspot: also push a spot to the DXCluster, independently
            # of the logbook QSO send above.
            if self._sv_autospot.get():
                self._send_spot(qso)

        # -- QSO sent OK -------------------------------------------------------
        elif k == "qso_ok":
            qso, qso_id, iid = msg[1], msg[2], msg[3]
            self._log(
                f"   ✅  CRX QSO #{qso_id}  ←  {qso['dx_call']}", "ok")
            self._tree_add(qso, f"✅  ID #{qso_id}", "ok", iid)
            self._total += 1
            self._by_band[qso["band"]] += 1
            self._by_mode[qso["mode"]] += 1
            self._refresh_counters()

        # -- QSO error → retry queue -------------------------------------------
        elif k == "qso_err":
            qso, log_id, err, iid = msg[1], msg[2], msg[3], msg[4]
            self._log(
                f"   ❌  Send error {qso['dx_call']}: {err}", "err")
            self._log(
                f"   ⏳  Added to retry queue (max {RetryQueue.MAX_ATTEMPTS} attempts).",
                "warn")
            self._tree_add(qso, "❌  pending retry", "err", iid)
            self.retry_q.push(qso, log_id)
            self._refresh_counters()

        # -- Retry OK ----------------------------------------------------------
        elif k == "retry_ok":
            qso, qso_id = msg[1], msg[2]
            self._log(f"   ✅  Retry successful — QSO #{qso_id}  {qso['dx_call']}", "ok")
            self._total += 1
            self._by_band[qso["band"]] += 1
            self._by_mode[qso["mode"]] += 1
            self._refresh_counters()

        elif k == "retry_fail":
            qso, err = msg[1], msg[2]
            self._log(f"   ⚠  Retry failed {qso['dx_call']}: {err}", "warn")
            self._refresh_counters()

        # -- DX spot sent OK (auto-dxspot) --------------------------------------
        elif k == "spot_ok":
            qso = msg[1]
            self._log(
                f"   📡  DXCluster spot sent  ←  {qso['dx_call']}  "
                f"{qso['freq_khz']} kHz  {qso['mode']}", "purple")

        # -- DX spot error (auto-dxspot) -----------------------------------------
        elif k == "spot_err":
            qso, err = msg[1], msg[2]
            self._log(f"   ❌  Spot send error {qso['dx_call']}: {err}", "err")

        # -- 11M test spot sent OK -----------------------------------------------
        elif k == "spot11_ok":
            t = msg[1]
            self._log(
                f"   📡  11M test spot OK  ←  {t['sender_call']} → {t['dx_call']}  "
                f"{t['freq_khz']} kHz  \"{t['comment']}\"", "purple")

        # -- 11M test spot error --------------------------------------------------
        elif k == "spot11_err":
            t, err = msg[1], msg[2]
            self._log(f"   ❌  11M test spot error {t['dx_call']}: {err}", "err")

    # ══════════════════════════════════════════════════════════════════════════
    #  §9.12  CLEAN SHUTDOWN
    # ══════════════════════════════════════════════════════════════════════════

    def _on_close(self) -> None:
        self._stop_bridge()
        self._snapshot_cfg()
        self.destroy()


# ══════════════════════════════════════════════════════════════════════════════
#  §10  ENTRY POINT
# ══════════════════════════════════════════════════════════════════════════════

def main() -> None:
    app = BridgeApp()
    app.mainloop()


if __name__ == "__main__":
    main()