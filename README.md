# usdSolidOcct — UsdSolid BrepArray and OCCT bridge

## Purpose

Convert UsdSolid boundary representations into OCCT shapes, tessellate them,
and calculate geometric measurements. This generic kit has no AECO schema
dependency. All operations use local shape coordinates; use metres for SI results.

## The library on an index card

| C++ function (prefix `UsdSolidOcct`) | Python (`pxr.UsdSolidOcct`) | Result |
|---|---|---|
| Build | Build | OCCT shape, sewn by default |
| Write | Write | Author geometry into a BrepArray in the current edit target |
| Tessellate | Tessellate | Double precision points, triangle indices, normals, source faces |
| Volume / Area | Volume / Area | Cubic / square shape units |
| Distance | Distance | Minimum separation |
| Common | Common | Boolean intersection shape |
| FaceCount | FaceCount | Unique face count |
| SolidCount / IsValid | SolidCount / IsValid | Kernel topology diagnostics |

## Build

```sh
nix build .#runtime --out-link result-runtime
env -u PYTHONPATH python3 check.py
env -u PYTHONPATH python3 -m pytest -q
nix flake check
```

The gate prints `N checks, M failed`. It launches the pinned native Python
runtime for bindings: the ordinary usd-core wheel has a different ABI.
No editable installation or setuptools is needed for source tests.

Inside `nix develop`, run `python examples/roundtrip.py input.usda output.usda`
to create an exact body and a Mesh twin. Add `--prim /Model/Body` and
`--brep-index 1` to select a particular body. The Mesh twin composes with stock
USD and does not require this library.

For a deployment mirror, use an external registry JSON following the family
toolchain's registry format, including `usdSolid`:

```sh
export AECO_NIX_REGISTRY="$HOME/.config/aeco/nix-registry.json"
env -u PYTHONPATH python3 tools/nix_local.py build .#runtime --out-link result-runtime --no-write-lock-file
```

The helper passes explicit input overrides; deployment addresses and lockfiles
remain outside version control. The public inputs use the published repository
names. See the [native build contract](https://github.com/criad-com/usdaeco-toolchain/blob/v0.3.9/docs/native.md).

## Upstream pin

Requires `usdSolid >=0.1,<0.2`; tested against v0.1.0, OCCT 7.9.3 and the
hub's development OpenUSD. The [source record](docs/upstream.md) identifies
the extracted code and its limitations. Exact refs are in `dependencies.json`.

## Layout

`usdSolidOcct/` contains the public C++ API and Python wrapper; `tools/` holds
source extraction and checks; `testenv/` contains native and Python tests.
Installed artifacts use `lib/libusdSolidOcct.dylib` (`.so` on Linux),
`lib/cmake/usdSolidOcct/`, `include/usdSolidOcct/`, and `pxr.UsdSolidOcct`.

## Status

Version 0.1.3 pins toolchain v0.3.9 with its Python package version fix.
Native verification remains pending; see the [prior build blocker](BLOCKED.md). The following
geometry results were rechecked using the cached 0.1.1 runtime; they do not
establish a successful 0.1.3 build.

Twelve native fixtures pass two round trips with all 20 validators,
equal face counts after periodic splitting, and relative volume error below 1e-9.
All eleven toolbox components pass: 631 faces and zero validator findings.
Tests also cover a hollow solid, multiple solids, multiple Breps, and an
installed CMake consumer.

The wider upstream corpus has 40 of 50 strict round trips proven; 18 of 21
tessellation vertex baselines match. The remaining cases are recorded in the
[acceptance results](docs/acceptance.md), including the three changed mesh
counts and cases whose face count changes when full-period faces are split.
The green regression gate checks these known limitations explicitly and does
not count them as successful geometry acceptance.

## Licence

Our code is MIT (see `LICENSE`). Files extracted from the upstream OpenUSD
fork at build time keep their own notices and licence; the complete upstream
licence is installed beside ours. OCCT is LGPL-2.1 with its exception and is
only ever linked as shared libraries: the native builder rejects static OCCT
archives in the dependency closure. UsdSolid (the schema kit this bridge
targets) carries the upstream licence for its extracted content.
