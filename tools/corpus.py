"""Live corpus checks with explicit, reviewable acceptance limitations."""
import json
from pathlib import Path
import tempfile
from check_support import ROOT, native_script, runtime_paths

PROFILE = ROOT / "testenv/corpus-profile.json"


def key(row):
    return f"{row['file']}#{row['prim']}#{row['index']}"


def issues(row):
    if row.get("error"):
        return [row["error"]]
    failures = []
    if row["findings"]:
        failures.append("Validator findings")
    if not row["valid"] or not row["roundtripValid"]:
        failures.append("Invalid OCCT shape")
    if row["authoredFaces"] != row["faces"]:
        failures.append("Build changed face count")
    if row["faces"] != row["roundtripFaces"]:
        failures.append("Write split periodic faces")
    if row["solids"] != row["roundtripSolids"]:
        failures.append("Solid count changed")
    if row["volume"] is not None:
        if row["roundtripVolume"] is None or abs(row["volume"]-row["roundtripVolume"]) > abs(row["volume"])*1e-9:
            failures.append("Relative volume exceeds 1e-9")
    elif abs(row["area"]-row["roundtripArea"]) > abs(row["area"])*1e-9:
        failures.append("Relative sheet area exceeds 1e-9")
    return failures


def measure(paths=None, directory=None):
    paths = paths or runtime_paths()
    names = [p.relative_to(paths["fixtures"]).as_posix()
             for p in sorted(Path(paths["fixtures"]).rglob("*.usda"))]
    with tempfile.TemporaryDirectory() as scratch:
        config = Path(scratch) / "paths.json"
        config.write_text(json.dumps(paths))
        return json.loads(native_script(paths, "tools/probe.py", "corpus", config,
                                        directory or Path(scratch) / "stages", *names))


def verify(rows):
    expected = json.loads(PROFILE.read_text())
    actual = {key(row): row for row in rows}
    assert set(actual) == set(expected), "Corpus fixture inventory changed"
    for name, profile in expected.items():
        row = actual[name]
        assert issues(row) == profile["issues"], (name, issues(row), profile["issues"])
        assert row.get("vertices") == profile.get("vertices"), (name, row.get("vertices"))
        assert row.get("baseline") == profile.get("baseline"), name
    proven = sum(not issues(row) for row in rows)
    baselines = [r for r in rows if r.get("baseline") is not None]
    matched = sum(r["vertices"] == r["baseline"] for r in baselines)
    return f"{proven}/{len(rows)} exact round trips; {len(rows)-proven} NOT PROVEN; {matched}/{len(baselines)} tessellation baselines match"


if __name__ == "__main__":
    print("== stage: upstream corpus", flush=True)
    print(verify(measure()))
