"""CTest for native reference shapes, both exported generations and validators."""
from pathlib import Path
import subprocess
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from check_support import assert_primitive
from probe import primitives

with tempfile.TemporaryDirectory() as directory:
    subprocess.run([sys.argv[1], directory], check=True)
    rows = primitives(directory)
    assert len(rows) == 12
    for row in rows:
        print({k: v for k, v in row.items() if k not in ("findings", "roundtripFindings")}, flush=True)
    for row in rows:
        assert_primitive(row)
    print("12 native round trips: equal face counts, volume within 1e-9, zero findings")
