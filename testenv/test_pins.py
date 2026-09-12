"""Release pin regressions run from source without native dependencies."""
import json
from pathlib import Path

import pytest

from pin_checks import check_built_revisions, check_pins

ROOT = Path(__file__).resolve().parents[1]


@pytest.fixture
def inputs():
    return (json.loads((ROOT / "dependencies.json").read_text()),
            (ROOT / "flake.nix").read_text(),
            json.loads((ROOT / "library.json").read_text())["version"])


def test_release_pins(inputs):
    assert check_pins(*inputs)


def test_matching_family_hash_is_rejected(inputs):
    document, flake, version = inputs
    pin = document["repos"]["aeco-toolchain"]
    flake = flake.replace("?ref=" + pin["ref"], "?ref=" + pin["revision"])
    pin["ref"] = pin["revision"]
    with pytest.raises(AssertionError, match="release tags"):
        check_pins(document, flake, version)


def test_stale_flake_version_is_rejected(inputs):
    document, flake, version = inputs
    with pytest.raises(AssertionError, match="version differs"):
        check_pins(document, flake + '\nversion = "0.0.0";', version)


def test_recursive_source_is_rejected(inputs):
    document, flake, version = inputs
    with pytest.raises(AssertionError):
        check_pins(document, flake.replace("usdSolid.flake = false;", ""), version)


def test_measured_public_revisions_and_unknown_build(inputs):
    document, _, _ = inputs
    revisions = {name: pin.get("publicRevision", pin.get("revision", pin["ref"]))
                 for name, pin in document["repos"].items()}
    assert check_built_revisions({"revisions": revisions}, document)
    revisions["aeco-toolchain"] = "0" * 40
    with pytest.raises(AssertionError, match="differs from pins"):
        check_built_revisions({"revisions": revisions}, document)
