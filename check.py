#!/usr/bin/env python3
"""Acceptance gate: N checks, M failed."""
import json
import os
from pathlib import Path
import re
import sys
import subprocess
import tempfile
import tomllib

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "tools"))
from check_support import native_script, runtime_paths, native_primitives, assert_primitive
from source_manifest import manifest
from corpus import measure, verify
from pin_checks import check_pins, check_built_revisions


def main():
    try:
        paths = runtime_paths()
    except (OSError, ValueError):
        print("FAIL NativeRuntime: build .#runtime --out-link result-runtime first")
        print("1 checks, 1 failed")
        return 1
    sys.path.insert(0, str(Path(os.environ.get("USDAECO_TOOLCHAIN_DIR", paths["toolchain"])) / "tools"))
    from usdaeco_check import Report, Result
    from usdaeco_check.structure import check_structure
    report = Report()
    print("== stage: repository contracts", flush=True)
    # Shared S05 assumes family-owned URLs even for external OpenUSD sources.
    # KitFlakeS05 enforces its tag/version rules with the upstream fork URLs.
    for row in check_structure(ROOT, only=["S01", "S04", "S25", "S26"]):
        report.add(row)
    report.check("KitManifest", json.loads((ROOT / "library.json").read_text()) == {
        "name": "usdSolidOcct", "version": "0.1.4", "kind": "kit", "tier": "toolchain", "licence": "MIT",
        "requires": {"usdSolid": ">=0.1,<0.2"}})
    report.check("KitReadme", re.findall(r"(?m)^## (.+)$", (ROOT / "README.md").read_text()) ==
        ["Purpose", "The library on an index card", "Build", "Upstream pin", "Layout", "Status", "Licence"])
    document = json.loads((ROOT / "dependencies.json").read_text())
    version = json.loads((ROOT / "library.json").read_text())["version"]
    report.check("PackageVersions", tomllib.loads((ROOT / "pyproject.toml").read_text())["project"]["version"] == version
                 and f'__version__ = "{version}"' in (ROOT / "usdSolidOcct/__init__.py").read_text()
                 and f'project(usdSolidOcct VERSION {version} ' in (ROOT / "CMakeLists.txt").read_text())
    report.run("KitFlakeS05", check_pins, document, (ROOT / "flake.nix").read_text(), version)
    report.run("BuiltRevisions", check_built_revisions, paths, document)
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
        assert receipt["version"] == version and receipt["pins"] == document["repos"]
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
