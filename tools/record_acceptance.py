"""Remeasure the native acceptance record; publish only with --publish."""
import argparse
from collections import Counter
from datetime import datetime, timezone
import json
from pathlib import Path
import platform
import tempfile

from check_support import ROOT, assert_primitive, native_primitives, runtime_paths
from corpus import measure, verify
from pin_checks import check_built_revisions


def record(publish=False):
    paths = runtime_paths()
    pins = json.loads((ROOT / "dependencies.json").read_text())
    check_built_revisions(paths, pins)
    print("== stage: measure native acceptance", flush=True)
    with tempfile.TemporaryDirectory() as directory:
        native = sorted(native_primitives(paths, directory), key=lambda row: row["name"])
    for row in native:
        assert_primitive(row)
    upstream = measure(paths)
    print(verify(upstream), flush=True)
    # Validator execution order is unspecified. Preserve the published order
    # only when the newly measured findings have identical contents/counts.
    previous = json.loads((ROOT / "docs/acceptance.json").read_text())
    prior = {(row["file"], row["prim"], row["index"]): row for row in previous["upstream"]}
    for row in upstream:
        old = prior.get((row["file"], row["prim"], row["index"]), {})
        if "findings" in row and "findings" in old:
            signature = lambda values: Counter(json.dumps(value, sort_keys=True) for value in values)
            if signature(row["findings"]) == signature(old["findings"]):
                row["findings"] = old["findings"]
    baseline = [row for row in upstream if row.get("baseline") is not None]
    result = {
        "measuredAt": datetime.now(timezone.utc).isoformat(),
        "platform": "aarch64-darwin" if platform.system() == "Darwin" else "x86_64-linux",
        "build": Path(paths["bridge"]).name,
        "upstreamRevision": paths["revisions"]["upstream"],
        "occtVersion": Path(paths["occt"]).name.split("-occt-", 1)[1],
        "usdSolidVersion": pins["repos"]["usdSolid"]["ref"].removeprefix("v"),
        "validatorCount": 20,
        "relativeTolerance": 1e-9,
        "tessellation": {"linearDeflection": 0.1, "angularDeflection": 0.5, "sew": False,
                         "matching": sum(row["vertices"] == row["baseline"] for row in baseline),
                         "total": len(baseline)},
        "native": native,
        "upstream": upstream,
        "version": json.loads((ROOT / "library.json").read_text())["version"],
        "pins": pins["repos"],
    }
    target = ROOT / ("docs/acceptance.json" if publish else "out/acceptance.json")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(result, indent=2) + "\n")
    print("Recorded " + target.relative_to(ROOT).as_posix())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--publish", action="store_true")
    record(parser.parse_args().publish)
