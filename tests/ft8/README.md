# FT8 MTD backport and CB regression tests

The backport is based on WSJT-X `v3.2.0-rc1` (September 2026), restricted to
the existing FT8 multithread decoder (`lib/ft8var`). Relevant upstream commits
in https://github.com/WSJTX/wsjtx are:

- `2fe5512622cc25c9af2daadc9327f219f1512c1d`: versioned residuals, independent
  worker spectra, conflict refitting and duplicate subtraction protection.
- `17440fc05a3378b572fc68d0c195590296b7af38`: initialize against the actual
  OpenMP team, including runtime thread limits.
- `7befc7d4eb24066f428735939dba68bb37076324`: FFTW plan-cache locking,
  thread-local waveform state and serial callback assignment.
- `08f3c404a7a0dec4c203434002abda7a6abd6888`: scratch storage lifetime and
  synchronization metric initialization.
- `e8a2f9b72ca5e4a279a64e131c387378f7d0358a`: contiguous nonempty frequency
  partitions and synchronization search bounds.
- `894424d67542b8f716f1feee06057df840b062e6`: callback scratch variables are
  local; only persistent counters retain SAVE.

The original WSJT-CB message packing and GUI/QSO logic remain in use. Upstream
progress reporting and ThreadSanitizer hooks are omitted because this fork
does not contain that infrastructure. The large `csig0` scratch array uses
allocated storage so decoding works with the default worker stack size.
Serial and OpenMP Fortran modules use separate build directories.

The additional CB fix is local to MTD unpacking and its amateur callsign
plausibility filters. `cb_callsigns.f90` follows the complete callsign grammar
in `Radio.cpp`, including numeric and slash suffixes. It accepts valid CB
type 4 messages in either hash orientation; it does not bypass CRC or signal
quality checks, and incomplete calls do not qualify for the exception.

## Running

```sh
cmake -S . -B build -DWSJT_BUILD_DECODER_TESTS=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Tests are optional (default OFF), require OpenMP and Python 3, and do not start
the GUI, audio devices, CAT, networking or radio transmission.

- Three upstream tests exercise range coverage, residual/spectrum coherence,
  overlapping transactions, duplicates, phase transitions and actual
  concurrent readers/writers.
- `cb_mtd` round-trips the production CB packer/unpacker and checks complete,
  incomplete, malformed and hashed calls, plus preservation of rejection.
- `cb_autoseq` compiles the actual `has_complete_cb_peer` and `auto_sequence`
  methods extracted from `mainwindow.cpp`, together with the real Radio and
  DecodedText implementations. UI controls and the subsequent `processMessage`
  call are substitutes: this verifies AutoSeq admission/stop decisions, not
  live radio operation or the complete transmit state machine.
- `cb_decoder_integration` generates eight reproducible noisy FT8 audio
  messages with the existing simulator, decodes successive 15-second slots
  with 1/4/12 requested workers and 1/2/3 cycles, including runtime limits of
  1 and 2 workers. It checks callsigns, reports, timestamps, frequencies and
  SNR, and feeds the decoded QSO messages into the AutoSeq test.

Additional manual regression during this backport: the upstream sample
`samples/FT8/210703_133430.wav` recovered the same 20 unique messages before
and after the change (4 workers), also with 12 workers and with 12 requested
but 2 available. This is a regression check, not a sensitivity benchmark.
