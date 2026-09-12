"""Apply deployment overrides from an external registry; never commit addresses."""
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def arguments():
    registry = json.loads(Path(os.environ["AECO_NIX_REGISTRY"]).read_text())
    urls = {entry["from"]["repo"]: "git+" + entry["to"]["url"]
            for entry in registry["flakes"]}
    pins = json.loads((ROOT / "dependencies.json").read_text())["repos"]
    args = ["--max-jobs", "4", "--cores", "6"]
    if registry.get("substituters"):
        args += ["--option", "substituters", " ".join(registry["substituters"])]
    for name in ("aeco-toolchain", "usdaeco-toolchain", "usdSolid"):
        ref = pins[name]["ref"]
        query = "ref=refs/tags/" + ref if ref.startswith("v") else "rev=" + ref
        args += ["--override-input", name, urls[name] + "?" + query]
    args += ["--override-input", "usdaeco-toolchain/core", urls["usdaeco-core"] + "?ref=refs/tags/v0.9.2",
             "--override-input", "aeco-toolchain/openusd", urls["openusd"] + "?rev=47154dc7b5e28df623745495a7a508b69535ba24"]
    return args


if __name__ == "__main__":
    cli = sys.argv[1:]
    pos = cli.index("-c") if "-c" in cli else len(cli)
    raise SystemExit(subprocess.call(["nix", *cli[:pos], *arguments(), *cli[pos:]], cwd=ROOT))
