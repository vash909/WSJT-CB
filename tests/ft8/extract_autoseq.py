"""Compile the actual CB AutoSeq entry points in a headless test harness."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
start = source.index('bool MainWindow::has_complete_cb_peer (')
end = source.index('void MainWindow::pskPost (', start)
Path(sys.argv[2]).write_text(source[start:end])
