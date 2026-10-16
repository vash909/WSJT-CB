import os
from pathlib import Path
import subprocess
import time

log = Path('/tmp/wsjtcb-test/application.log')
with log.open('w') as stream:
    process = subprocess.Popen(['/usr/bin/wsjtcb', '-r', 'CI'],
                               cwd='/tmp/wsjtcb-test', stdout=stream, stderr=stream)
    try:
        for _ in range(40):
            time.sleep(1)
            if process.poll() is not None:
                raise RuntimeError(f'Application exited with {process.returncode}: {log.read_text()}')
        window_tree = subprocess.check_output(['xwininfo', '-root', '-tree'], text=True)
        if 'WSJT-CB' not in window_tree or '1.4.1' not in window_tree:
            raise RuntimeError(f'Expected 1.4.1 window missing: {window_tree}')
        subprocess.run(['pgrep', '-P', str(process.pid), '-x', 'jt9'], check=True)
        data_root = Path.home() / '.local/share'
        if not (data_root / 'WSJT-CB - CI').is_dir():
            raise RuntimeError('Stable application data directory missing')
        if list(data_root.glob('WSJT-CB by *')):
            raise RuntimeError('Application recreated a branded data directory')
        print('PASS: installed 1.4.1 GUI and decoder ran for 40 seconds as an ordinary user; stable data path verified')
    finally:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
