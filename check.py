#!/usr/bin/env python3
"""Acceptance gate: N checks, M failed."""
import json
from pathlib import Path
import re
import sys
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "tools"))
from check_support import native_script, runtime_paths, native_primitives, assert_primitive
from source_manifest import manifest
from corpus import measure, verify


def main():
    try:
        paths = runtime_paths()
    except (OSError, ValueError):
        print("FAIL NativeRuntime: build .#runtime --out-link result-runtime first")
        print("1 checks, 1 failed")
        return 1
    sys.path.insert(0, str(Path(paths["toolchain"]) / "tools"))
    from usdaeco_check import Report, Result
    from usdaeco_check.structure import check_structure
    report = Report()
    print("== stage: repository contracts", flush=True)
    for row in check_structure(ROOT, only=["S01", "S25", "S26"]):
        report.add(row)
    report.check("KitManifest", json.loads((ROOT / "library.json").read_text()) == {
        "name": "usdSolidOcct", "version": "0.1.3", "kind": "kit", "tier": "toolchain", "licence": "MIT",
        "requires": {"usdSolid": ">=0.1,<0.2"}})
    report.check("KitReadme", re.findall(r"(?m)^## (.+)$", (ROOT / "README.md").read_text()) ==
        ["Purpose", "The library on an index card", "Build", "Upstream pin", "Layout", "Status", "Licence"])
    pins = json.loads((ROOT / "dependencies.json").read_text())["repos"]
    report.check("BuiltRevisions", paths["revisions"] == {k: v.get("revision", v["ref"]) for k, v in pins.items()})
    print("== stage: installed native library", flush=True)
    prefix = Path(paths["bridge"])
    suffix = ".dylib" if sys.platform == "darwin" else ".so"
    report.check("SharedLibrary", (prefix / "lib" / ("libusdSolidOcct" + suffix)).is_file())
    report.check("CmakeExport", (prefix / "lib/cmake/usdSolidOcct/usdSolidOcctConfig.cmake").is_file())
    report.check("CompiledSource", json.loads((prefix / "share/usdSolidOcct/source-manifest.json").read_text()) == manifest(ROOT))
    ctest = (prefix / "share/usdSolidOcct/ctest.log").read_text()
    report.check("NativeCtest", ctest.count("Test Passed.") == 4 and "Test Failed." not in ctest,
                 "4 CTest entries")
    report.check("InstalledCmakeConsumer", subprocess.run([str(Path(paths["consumer"]) / "bin/consumer")],
                 capture_output=True).returncode == 0)
    closure = Path(paths["closure"]).read_text().splitlines()
    archives = [p.name for store in closure for p in Path(store).rglob("libTK*.a")]
    report.check("DynamicOcctClosure", not archives, f"{len(closure)} store paths, {len(archives)} static OCCT archives")
    report.run("PythonAndCubeSmoke", native_script, paths, "testenv/smoke.py",
               Path(paths["fixtures"]) / "testCube.usda")
    report.run("ApiContracts", native_script, paths, "testenv/contracts.py", paths["fixtures"])
    print("== stage: native primitive round trips", flush=True)
    with tempfile.TemporaryDirectory() as directory:
        rows = {row["name"]: row for row in native_primitives(paths, directory)}
    for name in ("cube", "cylinder", "cone", "sphere", "torus", "holed_plate", "filleted_cube", "nurbs_cylinder", "nurbs_holed_plate", "elliptical_prism", "hollow_box", "two_boxes"):
        report.run("RoundTrip_" + name, assert_primitive, rows[name])
    print("== stage: upstream corpus and documented limitations", flush=True)
    def corpus_result():
        return Result("UpstreamCorpusProfile", True, verify(measure(paths)))
    report.run("UpstreamCorpusProfile", corpus_result)
    print("== stage: recorded cache publication", flush=True)
    def cache_receipt():
        receipt = json.loads((ROOT / "docs/cache-receipt.json").read_text())
        assert receipt["pushExitCode"] == 0 and receipt["verifiedClosurePaths"] >= len(closure)
        assert receipt["configuredCachePaths"] + receipt["upstreamCachePaths"] == receipt["verifiedClosurePaths"]
        for key in ("bridge", "schema", "validators", "consumer"):
            artifact = receipt["artifacts"][key]
            assert artifact["store"] == Path(paths[key]).name
            assert artifact["cache"] == "configured" and artifact["narHash"].startswith("sha256:")
            assert artifact["narSize"] > 0
        return Result("CachePushReceipt", True, f"{receipt['verifiedClosurePaths']} narinfos verified at publication; receipt matches this build")
    report.run("CachePushReceipt", cache_receipt)
    return report.finish()


if __name__ == "__main__":
    raise SystemExit(main())
