"""Kit pin contracts, including external upstream sources and public releases."""
import re

FAMILY = {"aeco-toolchain", "usdaeco-toolchain", "usdSolid"}


def check_pins(document, flake, version):
    """Apply S05's tag/version rules while retaining external OpenUSD URLs."""
    flake = re.sub(r"(?m)^\s*#.*$", "", flake)
    for literal in re.findall(r'\bversion\s*=\s*"([^"]*)"\s*;', flake):
        assert "${" in literal or literal == version, "Flake version differs from library.json"
    urls = re.findall(r'\burl\s*=\s*"([^"]+)"', flake)
    expected = []
    pins = document["repos"]
    for name, pin in pins.items():
        if name in FAMILY:
            assert re.fullmatch(r"v\d+\.\d+\.\d+", pin["ref"]), "Family inputs require release tags"
            assert re.fullmatch(r"[a-f0-9]{40}", pin.get("revision", "")), "Record checked revision"
            if "publicRevision" in pin:
                assert re.fullmatch(r"[a-f0-9]{40}", pin["publicRevision"])
            expected.append(f"github:criad-com/{pin['repo']}?ref={pin['ref']}")
        else:
            assert pin["repo"] == "OpenUSD" and re.fullmatch(r"[a-f0-9]{40}", pin["ref"])
            expected.append(f"github:jensjebens/OpenUSD?rev={pin['ref']}")
    assert FAMILY <= pins.keys(), "Missing family input"
    assert sorted(urls) == sorted(expected), "Flake URLs and exact pins differ"
    assert 'nixpkgs.follows = "aeco-toolchain/nixpkgs";' in flake
    assert 'usdSolid.flake = false;' in flake and "usdSolid.inputs." not in flake
    return True


def check_built_revisions(paths, document):
    """Accept only measured forge/public commits; never an arbitrary rebuild."""
    revisions = paths.get("revisions", {})
    pins = document["repos"]
    assert revisions.keys() == pins.keys(), "Build input inventory differs from pins"
    for name, pin in pins.items():
        allowed = {pin.get("revision", pin["ref"])}
        if "publicRevision" in pin:
            allowed.add(pin["publicRevision"])
        assert revisions[name] in allowed, "Build revision differs from pins: " + name
    return True
