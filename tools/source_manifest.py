"""Fingerprint the native inputs so gates reject a stale compiled library."""
import hashlib
import json
from pathlib import Path
import sys


def manifest(root):
    root = Path(root)
    names = ["CMakeLists.txt", "library.json", "plugInfo.json.in", "tools/extract.py",
             "tools/source_manifest.py", "testenv/testUsdSolidOcct.cpp", "testenv/fixtures.cpp",
             "testenv/smoke.py", "testenv/primitives.py", "testenv/contracts.py", "testenv/diagnose.cpp", "tools/probe.py", "tools/check_support.py"]
    names += [p.relative_to(root).as_posix() for p in (root / "usdSolidOcct").iterdir()
              if p.suffix in (".h", ".cpp", ".py")]
    return {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in sorted(names)}


if __name__ == "__main__":
    Path(sys.argv[2]).write_text(json.dumps(manifest(sys.argv[1]), indent=2) + "\n")
