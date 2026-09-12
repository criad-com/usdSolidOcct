"""Publish the runtime closure and verify every resulting cache narinfo.

Deployment endpoints and credentials stay in the external registry and the
cache client's own configuration. The checked-in receipt contains store names.
"""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
from urllib.request import urlopen
from urllib.error import HTTPError
from check_support import ROOT, runtime_paths
from pin_checks import check_built_revisions


def publish():
    registry = json.loads(Path(os.environ["AECO_NIX_REGISTRY"]).read_text())
    endpoint = registry.get("cacheUrl") or next(
        url for url in registry["substituters"] if "cache.nixos.org" not in url)
    runtime = Path(os.environ.get("USD_SOLID_OCCT_RUNTIME", ROOT / "result-runtime")).resolve()
    paths = runtime_paths()
    pins = json.loads((ROOT / "dependencies.json").read_text())
    check_built_revisions(paths, pins)
    roots = {key: paths[key] for key in ("bridge", "schema", "validators", "consumer")}
    roots["runtime"] = str(runtime)
    root_stores = set(roots.values())
    print("== stage: publish native runtime closure", flush=True)
    options = ["--ignore-upstream-cache-filter"] if registry.get("pushFullClosure") else []
    subprocess.run(["attic", "push", "--jobs", "4", *options, registry["cache"], *roots.values()], check=True)
    closure = subprocess.check_output(["nix-store", "--query", "--requisites", *roots.values()], text=True).splitlines()

    def verify(store):
        name = Path(store).name
        sources = [endpoint] if store in root_stores else [endpoint, *registry["substituters"]]
        fields = None
        for url in dict.fromkeys(sources):
            try:
                with urlopen(url.rstrip("/") + "/" + name.split("-", 1)[0] + ".narinfo", timeout=30) as response:
                    fields = dict(line.split(": ", 1) for line in response.read().decode().splitlines() if ": " in line)
                break
            except HTTPError as exc:
                if exc.code != 404:
                    raise
        assert fields is not None, "Missing narinfo: " + name
        assert fields["StorePath"] == store, name
        local_hash = subprocess.check_output(["nix-store", "--query", "--hash", store],
                                            text=True, stderr=subprocess.DEVNULL).strip()
        remote_hash = subprocess.check_output(["nix", "hash", "convert", "--hash-algo", "sha256",
            "--to", "nix32", fields["NarHash"]], text=True, stderr=subprocess.DEVNULL).strip()
        assert "sha256:" + remote_hash == local_hash, name
        return {"store": name, "narHash": local_hash, "narSize": int(fields["NarSize"]),
                "cache": "configured" if url == endpoint else "upstream"}

    print(f"== stage: verify {len(closure)} published narinfos", flush=True)
    with ThreadPoolExecutor(max_workers=4) as workers:
        verified = {row["store"]: row for row in workers.map(verify, closure)}
    receipt = {
        "version": json.loads((ROOT / "library.json").read_text())["version"],
        "pins": pins["repos"],
        "publishedAt": datetime.now(timezone.utc).isoformat(),
        "platform": "aarch64-darwin" if os.uname().sysname == "Darwin" else "x86_64-linux",
        "pushExitCode": 0, "verifiedClosurePaths": len(verified),
        "configuredCachePaths": sum(row["cache"] == "configured" for row in verified.values()),
        "upstreamCachePaths": sum(row["cache"] == "upstream" for row in verified.values()),
        "verification": "All closure narinfos matched local StorePath and NarHash; roots verified in the configured cache after recursive push",
        "artifacts": {role: verified[Path(store).name] for role, store in roots.items()},
    }
    (ROOT / "docs/cache-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"Published and verified {len(verified)} store paths")


if __name__ == "__main__":
    publish()
