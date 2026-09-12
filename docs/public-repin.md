# v0.1.4 public re-pin verification

The native runtime was rebuilt from the requested tags on aarch64-darwin.
All geometry and validation measurements are unchanged. The regenerated
acceptance record changes provenance only; findings retain their previous
order after comparison as an unordered collection with multiplicity.

| Input | Tag | Checked forge revision | Public revision |
|---|---|---|---|
| aeco-toolchain | v0.4.0 | `71c86d54aa1e4be180a301a089d9a40e727758cb` | `afbea7ce2b012e55af54047311fad37339eb56df` |
| usdaeco-toolchain | v0.3.10 | `59d3da5ff5114089b54efaaae38efdd7fe1b8e73` | `6328e1b5e63f89ea87984ad240b5c3ca37974795` |
| usdSolid | v0.1.4 | `8d03bd615744ffbade4527fd9c8fd7dfee99f21f` | Not verified: anonymous access failed |

Annotated forge tags were peeled to their commits. Public releases have
independent history: the gate accepts only the measured forge or public
revision listed in dependencies.json. The missing public usdSolid revision
must be verified and recorded before claiming public native acceptance.

usdSolid is a source input (`flake = false`). Its package outputs are imported
with the root flake's two toolchains and explicit OpenUSD schema, validator
and fixture sources. These three upstream revisions are unchanged from
usdSolid v0.1.0/v0.1.4. The requirement remains `>=0.1,<0.2`.
The [toolchain changelog](https://github.com/criad-com/usdaeco-toolchain/blob/v0.3.10/CHANGELOG.md)
records the unchanged OpenUSD output and the new tag-only pin checks.

| Acceptance | Measured result |
|---|---|
| Release metadata | 0.1.4 in library.json, pyproject.toml, CMake and the source Python package |
| Family input refs | 3 matching release tags; 0 family commit-hash URL refs |
| Forge tags | 3/3 verified at the requested releases |
| Public tags | 2/3 independently verified; usdSolid access remains unverified |
| Native rebuild | PASS; 12 local derivations built with offline input overrides |
| check.py | 31 checks, 0 failed, 0 not run |
| Shared structure lint | S01, S04, S25, S26 pass; kit S05 equivalent passes |
| pytest | 61 passed with the rebuilt runtime |
| Native / installed consumer CTest | 4 / 1 passed |
| Native primitive round trips | 12/12; 24 exported generations, 0 findings; relative volume tolerance 1e-9 |
| Upstream corpus | 40/50 strict round trips; 10 existing limitations; 18/21 tessellation baselines |
| Regenerated layers | 73/73 byte-identical to the v0.1.3 runtime |
| Installed fixture layers | 40/40 byte-identical |
| Generated schema | Byte-identical |
| Geometry measurements | All 12 native and 50 corpus records unchanged |
| Cache publication | 90/90 closure paths verified in the configured cache; 5 root artifacts |
| Static OCCT archives | 0 in the 77-path bridge closure |
| Nix flake check | 1 offline attempt, exit 1 after interruption; 3 declared outputs evaluated, 464 checks reported |
| Public-name / whitespace sweep | 0 obsolete public-org refs; git diff --check clean |

The [comparison inventory](repin-comparison.json) contains hashes for the
24 native exports, 48 corpus output layers and one exact/Mesh example.
The previous committed acceptance record originated with v0.1.0; its measured
geometry remains identical in this fresh v0.1.4 record. No native geometry
source changed. Versioned binaries and plugin descriptors have new provenance.
This kit has no committed example result tree or vanilla render to republish;
the exact/Mesh example was regenerated and tested in a plugin-free process.

Reproduce the build and publications using the README commands:

```sh
nix build .#runtime --out-link result-runtime
env -u PYTHONPATH python3 tools/record_acceptance.py --publish
env -u PYTHONPATH python3 tools/push_cache.py
env -u PYTHONPATH python3 check.py
env -u PYTHONPATH python3 -m pytest -q
```

Use the documented external registry or local input overrides as needed.
The cache publication used `pushFullClosure: true` and verified every NAR
hash against the local store. This proves publication, not a cold restore.

The single `nix flake check --offline --no-write-lock-file` attempt used
local checkout overrides at the exact revisions above, plus the toolchain's
core v0.9.2 test input. Substituters and remote builders were disabled.
Nix evaluated `native`, `installed-consumer` and `acceptance`, then reported
464 flake checks and began additional bootstrap/source derivations. The
attempt was interrupted rather than allowing that wider build to continue.
The wrapper recorded exit 1; no second flake-check attempt was made. The
complete flake check is not proven despite the successful native runtime
build and direct acceptance gate. Offline mode alone does not prevent
source-fetch derivations from running.

## Deviations

- Anonymous access to the public usdSolid repository failed, so v0.1.4's
  public tag and orphan revision could not be independently verified.
  The requested tag is retained. Public resolution and recording that revision
  remain unverified. The v0.1.4 runtime built from the checked forge revisions
  passed 31 checks and 61 tests; its cache receipt records 90/90 verified
  closure paths.
- Shared S05 assumes family-owned URLs for every input, including external
  OpenUSD sources. KitFlakeS05 preserves those fork URLs while enforcing the
  release-tag and version rules; shared S04 checks the pin/range contract.
  Shared S02/S03 still have no native kit form.
- The full offline flake check was interrupted as described above. Its
  broader build and online resolution remain for review; this release makes
  no claim that the full flake check passed.
- The two upstream source inputs made explicit at the root retain exactly
  the revisions previously resolved recursively by usdSolid. They introduce
  no geometry or dependency behavior change.
- Linux execution and a cold cache restore were not attempted. The existing
  ten strict corpus limitations and three tessellation baseline differences
  remain explicitly not proven.
