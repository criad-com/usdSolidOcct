"""Source checks use a separate, ABI-matched native runtime."""
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def runtime_paths():
    runtime = Path(os.environ.get("USD_SOLID_OCCT_RUNTIME", ROOT / "result-runtime"))
    return json.loads((runtime / "paths.json").read_text())


def clean_env(plugins=None):
    env = dict(os.environ)
    for name in ("PYTHONPATH", "PXR_PLUGINPATH_NAME", "PXR_AR_DEFAULT_SEARCH_PATH"):
        env.pop(name, None)
    if plugins:
        env["PXR_PLUGINPATH_NAME"] = str(plugins)
    return env


def native_script(paths, script, *args):
    result = subprocess.run([paths["python"], str(ROOT / script), *map(str, args)],
                            env=clean_env(paths["plugins"]), capture_output=True,
                            text=True, timeout=180)
    if result.returncode:
        raise RuntimeError((result.stdout + result.stderr)[-4000:])
    return result.stdout


def native_primitives(paths, directory):
    result = subprocess.run([str(Path(paths["bridge"]) / "libexec/usdSolidOcct/usdSolidOcctFixtures"),
                             str(directory)], env=clean_env(paths["plugins"]),
                            capture_output=True, text=True, timeout=180)
    if result.returncode:
        raise RuntimeError(result.stderr[-4000:])
    runtime = Path(os.environ.get("USD_SOLID_OCCT_RUNTIME", ROOT / "result-runtime"))
    return json.loads(native_script(paths, "tools/probe.py", "primitives", runtime / "paths.json", directory))


def assert_primitive(row):
    assert not row.get("error"), row
    assert row["valid"] and row["roundtripValid"], row["name"]
    assert not row["findings"] and not row["roundtripFindings"], row["name"]
    assert row["sourceFaces"] == row["faces"] == row["roundtripFaces"], row["name"]
    for value in (row["volume"], row["roundtripVolume"]):
        assert abs(value - row["sourceVolume"]) <= abs(row["sourceVolume"]) * 1e-9, (row["name"], value, row["sourceVolume"])
    return True
