"""Decode reproducible CB audio through jt9, then feed its output to AutoSeq."""
from array import array
from pathlib import Path
import math
import os
import random
import re
import subprocess
import sys
import tempfile
import wave

decoder, simulator, autoseq = map(lambda p: str(Path(p).resolve()), sys.argv[1:])


def run(args, directory, env=None):
    result = subprocess.run(args, cwd=directory, env=env, text=True,
                            capture_output=True, timeout=45)
    if result.returncode:
        raise AssertionError(f"{args}: {result.returncode}\n{result.stdout}\n{result.stderr}")
    return result.stdout


with tempfile.TemporaryDirectory(prefix="wsjtcb-ft8-") as directory:
    root = Path(directory)
    cases = [
        ("CQ 1AT106", 1000),
        ("CQ 26AT101", 1467),
        ("CQ 161XZ085", 2100),
        ("<1AT106> 26AT101", 1500),
        ("1AT106 -12", 1500),
        ("1AT106 R-12", 1500),
        ("<1AT106> 26AT101 RR73", 1500),
        ("1AT106 73", 1500),
    ]
    files = []
    expected = {}
    for i, (message, frequency) in enumerate(cases):
        run([simulator, message, str(frequency), "0", "0", "0", "1", "99"], root)
        with wave.open(str(root / "000000_000001.wav"), "rb") as wav:
            params = wav.getparams()
            samples = array("h", wav.readframes(wav.getnframes()))
        if sys.byteorder != "little":
            samples.byteswap()
        rng = random.Random(1701 + i)
        # The simulator's noiseless peak is 32767. Add seeded noise at -12 dB
        # in the FT8 reference bandwidth (2500 Hz), without clipping.
        amplitude = math.sqrt(2 * 2500 / 6000) * 10 ** (-12 / 20)
        samples = array("h", (round(1000 * (amplitude * s / 32767 + rng.gauss(0, 1)))
                              for s in samples))
        if sys.byteorder != "little":
            samples.byteswap()
        stamp = f"12{i // 4:02}{15 * (i % 4):02}"
        target = root / f"260101_{stamp}.wav"
        with wave.open(str(target), "wb") as wav:
            wav.setparams(params)
            wav.writeframes(samples.tobytes())
        files.append(str(target))
        expected[stamp] = (message, frequency)

    # All cycle counts, 1/4/12 workers, and teams smaller than requested.
    configurations = [(1, 1, 1, "FALSE"), (4, 2, 4, "FALSE"),
                      (12, 3, 12, "FALSE"), (12, 3, 2, "FALSE"),
                      (12, 3, 1, "TRUE")]
    for workers, cycles, limit, dynamic in configurations:
        env = {**os.environ, "OMP_THREAD_LIMIT": str(limit), "OMP_DYNAMIC": dynamic}
        output = run([decoder, "-8", "-M", "-N", str(workers), "-C", str(cycles),
                      "-c", "1AT106", "-x", "26AT101", "-Q", "3", *files], root, env)
        found = set()
        qso_lines = []
        for line in output.splitlines():
            match = re.match(r"^(\d{6})\s+(-?\d+)\s+(-?[\d.]+)\s+(\d+) ~  (.*)$", line)
            if not match:
                continue
            stamp, snr, dt, frequency, message = match.groups()
            message = message[:43].strip()
            wanted, wanted_frequency = expected[stamp]
            assert message == wanted, (workers, limit, line, wanted)
            assert abs(int(frequency) - wanted_frequency) <= 2, line
            assert abs(float(dt)) < 0.2 and -20 <= int(snr) <= -5, line
            found.add(stamp)
            if not message.startswith("CQ "):
                qso_lines.append(line)
        assert found == set(expected), (workers, limit, set(expected) - found, output)
        actual_decodes = root / "qso-decoded.txt"
        actual_decodes.write_text("\n".join(qso_lines) + "\n")
        run([autoseq, str(actual_decodes)], root)
        print(f"PASS workers={workers}, cycles={cycles}, thread_limit={limit}: "
              f"{len(found)} CB audio messages and AutoSeq")
