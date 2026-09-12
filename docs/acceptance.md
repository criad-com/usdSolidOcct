# Acceptance results

Measured for v0.1.4 on aarch64-darwin with OCCT 7.9.3 and usdSolid v0.1.4. The [machine-readable record](acceptance.json) identifies the compiled artifact and pinned upstream revisions. All 20 native UsdSolid validators are loaded; missing validators fail the gate.

## Native reference shapes

Each native shape is split at full-period seams once before the reference measurement. Both exported generations preserve this canonical face count. The unsplit input count is shown separately. All 24 exported stages have zero findings and both rebuilt generations are valid OCCT shapes.

| Shape | Input faces | Canonical / first / second faces | Reference volume | Worst relative volume error |
|---|---:|---:|---:|---:|
| cone | 2 | 3 / 3 / 3 | 261.799387799 | 2.17e-16 |
| cube | 6 | 6 / 6 / 6 | 1000 | 0 |
| cylinder | 3 | 4 / 4 / 4 | 282.743338823 | 2.01e-16 |
| elliptical_prism | 3 | 4 / 4 / 4 | 94.2477796077 | 4.37e-15 |
| filleted_cube | 26 | 26 / 26 / 26 | 975.587013891 | 0 |
| holed_plate | 7 | 8 / 8 / 8 | 87.4336293856 | 0 |
| hollow_box | 12 | 12 / 12 / 12 | 488 | 0 |
| nurbs_cylinder | 3 | 4 / 4 / 4 | 282.743338823 | 4.02e-16 |
| nurbs_holed_plate | 7 | 8 / 8 / 8 | 87.4336293856 | 0 |
| sphere | 1 | 2 / 2 / 2 | 523.598775598 | 0 |
| torus | 1 | 4 / 4 / 4 | 98.6960440109 | 1.44e-16 |
| two_boxes | 12 | 12 / 12 / 12 | 16 | 0 |

The threshold is 1e-9 relative volume error. The box tests also verify area 24, separation 3, intersection volume 4, and an empty intersection with volume 0. A packed two-Brep test selects both indices and verifies their separation is 10.

## Toolbox

All eleven components pass with 631 faces. Every exported stage has zero findings, and source and rebuilt shapes are valid solids. Volumes compare the original stage after Build with its Write/Build result; this is not an independent comparison to an external CAD file.

| Prim | Faces before / after | Relative volume error |
|---|---:|---:|
| `/World/lid___lock_pin/brep` | 4 / 4 | 0 |
| `/World/lid___handle/brep` | 20 / 20 | 1.27e-15 |
| `/World/lid___lock/brep` | 26 / 26 | 1.5e-16 |
| `/World/lid___lid_shell/brep` | 335 / 335 | 5.7e-11 |
| `/World/lid___lock_pin_1/brep` | 4 / 4 | 0 |
| `/World/lid___lock_1/brep` | 26 / 26 | 1.5e-16 |
| `/World/base_shell/brep_0` | 151 / 151 | 6.3e-10 |
| `/World/base_shell/brep_1` | 35 / 35 | 8.78e-16 |
| `/World/lid_pin/brep` | 10 / 10 | 3.48e-16 |
| `/World/lid_pin_1/brep` | 10 / 10 | 3.48e-16 |
| `/World/lid_pin_2/brep` | 10 / 10 | 3.48e-16 |

## Tessellation baselines

18 of 21 pinned vertex-count baselines match using `Build(..., sew=False)` and absolute linear deflection 0.1, angular deflection 0.5. The three differences follow the exact singular-boundary repairs described in [the source record](upstream.md). Full baseline parity is **NOT PROVEN**.

| Fixture | Pinned vertices | Measured vertices | Status |
|---|---:|---:|---|
| testAnalyticPlaneWithHole.usda | 30 | 30 | PASS |
| testCone.usda | 104 | 104 | PASS |
| testCube.usda | 24 | 24 | PASS |
| testCubeWithHole.usda | 148 | 148 | PASS |
| testCylinder.usda | 124 | 124 | PASS |
| testDepressedPlane.usda | 122 | 122 | PASS |
| testDepressedPlaneSeamSplit.usda | 136 | 136 | PASS |
| testFilletedCube.usda | 496 | 506 | NOT PROVEN |
| testFilletedCubeWithHole.usda | 620 | 630 | NOT PROVEN |
| testFullPeriodSphereBand.usda | 179 | 179 | PASS |
| testFullPeriodSphereBandSeamSplit.usda | 196 | 196 | PASS |
| testFullPeriodTorus.usda | 547 | 547 | PASS |
| testFullPeriodTorusSeamSplit.usda | 562 | 562 | PASS |
| testMixedCurvesWithPcurves.usda | 18 | 18 | PASS |
| testMixedSurfaces.usda | 108 | 108 | PASS |
| testPlane.usda | 4 | 4 | PASS |
| testPlaneWithHole.usda | 33 | 33 | PASS |
| testProducerCube.usda | 24 | 24 | PASS |
| testRealToleranceCylinder.usda | 108 | 108 | PASS |
| testSphere.usda | 306 | 304 | NOT PROVEN |
| testTwoBoxes.usda | 48 | 48 | PASS |

## Wider corpus and deviations

40 of 50 upstream Brep cases satisfy zero findings, valid kernel shapes, unchanged face and solid counts, and the volume threshold (area for sheets). The live regression profile checks all 50 cases and rejects any change to the recorded limitations or mesh counts. Matching an expected limitation does not count as geometry acceptance.

| Fixture | Strict acceptance status | Reason |
|---|---|---|
| derivedTrims/testConeNoPcurves.usda | NOT PROVEN | Sewing left faces outside the solid shell |
| derivedTrims/testDepressedPlaneNoPcurves.usda | NOT PROVEN | Validator findings; Write split periodic faces |
| testAnalyticCylinder.usda | NOT PROVEN | Write split periodic faces |
| testCubeBrep.usda | NOT PROVEN | Missing brep:regionCount |
| testDepressedPlane.usda | NOT PROVEN | Write split periodic faces |
| testFullPeriodSphereBand.usda | NOT PROVEN | Write split periodic faces |
| testFullPeriodTorus.usda | NOT PROVEN | Write split periodic faces |
| testFullSphere.usda | NOT PROVEN | Write split periodic faces |
| testRealToleranceCylinder.usda | NOT PROVEN | Relative volume exceeds 1e-9 |
| testVoidOnlySphere.usda | NOT PROVEN | Write split periodic faces |

Seven cases change face counts because writing splits full-period faces; one of these also has invalid derived trims. The legacy cube lacks the required region-count attribute. The pcurve-free cone cannot retain every face in its solid shell and raises an error. The tolerance-stressed cylinder misses the volume threshold. These are distinct from the eleven passing toolbox components.

The toolchain v0.3.10 generator has no kit kind. This repository runs shared structure rules S01/S04/S25/S26 and explicit kit manifest, README and install checks. KitFlakeS05 enforces release-tag inputs and version agreement while retaining the external OpenUSD fork URLs. Native Python uses the hub ABI; the source test runner delegates binding checks to it. No package installation or setuptools is required.

Only aarch64-darwin execution is proven. Linux build and execution were not tested for this release. The current flake-check outcome is recorded in [public re-pin verification](public-repin.md).

## Usage boundaries

Build reads one Brep at default time in local coordinates. Stage transforms and unit conversion remain the caller’s responsibility. Write replaces the target BrepArray’s geometry in the current edit target, preserving unrelated identity attributes. Full-period faces are split. Free sheets retain area semantics; Volume rejects them. Export rejects non-manifold radial topology, free wires/points, and mixtures of solids with free faces.

The public shared library and CMake export are exercised by a separate installed consumer. The Python example writes a Mesh twin and fallback prim type; a separate process verifies composition without the schema plugin. The bridge closure is scanned for static OCCT archives.

## Verification and cache

| Check | Result |
|---|---|
| Source gate | See the current [verification record](verification.json) |
| Source pytest | See the current [verification record](verification.json) |
| Native CTest | 4 passed |
| Installed consumer CTest | 1 passed |
| Nix flake check | See [public re-pin verification](public-repin.md); one offline attempt |
| Static OCCT archives | 0 across 77 bridge closure paths |
| Cache publication | 90 closure narinfos verified after recursive push |
| Linux | Not tested for this release |

The [cache receipt](cache-receipt.json) records matching StorePath and NAR hashes for all 90 closure entries, all served by the configured cache. All five root artifacts were verified there. This proves publication metadata and hash agreement, not a cold restore. The gate rejects a receipt whose version, pins or native artifact names differ from the current runtime.

The [verification record](verification.json) captures these totals. The public flake URLs were resolved through the documented external deployment registry; direct public resolution was not tested.
