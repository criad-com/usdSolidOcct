# Upstream extraction and boundaries

The extraction input is [the hdOcct branch](https://github.com/jensjebens/OpenUSD/pull/62),
revision `d618f8ac62cefa02f6765d8e3784ea503195ce86`.
The companion [edge display work](https://github.com/jensjebens/OpenUSD/pull/65)
belongs to the imaging adapter and is outside this library.

`tools/extract.py` extracts `pxr/imaging/plugin/hdOcct/brepBuilder.{h,cpp}`
and the mesh extraction routine in `tessellator.cpp` during CMake configure.
The builder is renamed `UsdSolidOcctBrepBuilder`; exported symbols use the
library macro. The environment-controlled sewing block is removed, because
sewing is an explicit library argument. Mesh extraction retains face traversal
and normal/winding handling. No Hydra plugin is built.

The fork's `testenv/tools/brepExport.cpp` is the reference for the writer.
The fixture corpus and tessellation counts come from
`testenv/testUsdSolidTessellation/fixtures` and `testenv/testUsdSolidTessellation.py`
at the same revision. Upstream source and fixtures are inputs, not vendored files.

## Deviations

- The v0.3.0 skeleton generator and lint have no `kit` kind. As in usdSolid,
  the gate invokes applicable S01/S25/S26 rules and explicit kit checks;
  semantic schema and use-case rules are inapplicable.
- Native Python uses the hub's Python ABI, separately from source tests.
- Upstream builds a compound of disconnected faces. The default bridge Build
  sews faces and promotes closed shells to solids; `sew=False` preserves the
  upstream representation for tessellation comparisons.
- The writer extracts NURBS curve packing helpers from `brepExport.cpp`.
  It authors schema attributes directly, classifies analytics, preserves shared
  vertices/edges, walks solids and cavity shells, emits outer wires first,
  and builds manifold radial cycles from the edge-to-face ancestor map.
- Full-period faces are split before writing. Native reference tests apply
  the same split once before counting faces. Counts against unsplit input
  faces are a separate, unproven acceptance row.
- The writer normalizes indirect analytic frames and cone axial coordinates,
  preserving the corresponding UV and faceuse orientation. Circle parameter
  origins and analytic surface U origins are rotated into the primary period.
- Rectangular patches with a collapsed pole boundary carry exact `face:range`
  and omit pcurves on that face (zero packed entries in a mixed array). The
  degenerate boundary is not a 3D edge. Build reconstructs the natural
  rectangular boundary. Regular faces retain their authored pcurves. This
  avoids the pinned validator's documented BA.763 singularity disagreement
  without suppressing findings or inventing degenerate edges.
- Build converts authored cone pcurve V from axial to slant distance; the
  pinned builder performed this conversion for ranges but omitted pcurves.
- Build also recognizes rectangular singular charts whose authored pcurves
  lie on their UV range boundaries. This restores their collapsed boundaries
  and changes three of the thirteen core tessellation baselines. Counts are
  reported separately from exact geometry acceptance.
- Solid assembly follows region and shell membership, preserving bounded
  cavities. A solid region whose shell cannot sew closed raises an error.
  Free sheets remain sheets. Non-manifold export and mixtures of solids and
  free faces are explicitly rejected.
- Unsupported surface families are converted with OCCT's topology converter
  before packing NURBS, preserving the pcurve parameterization. Converting a
  surface alone is insufficient. The elliptical extrusion and NURBS holed
  plate exercise this path and packed trims through two generations.
- Pcurve-to-3D reconstruction uses an accuracy ceiling of 1e-10 shape units,
  separately from the authored sewing tolerance. Volume uses adaptive
  Gauss–Kronrod integration with NURBS knot spans and a 1e-12 target.
- Sewing must retain every authored face inside the declared solid shell;
  orphan faces are an error. A closed single-face surface is wrapped in a
  shell before solid assembly. Free sheets have no volume operation.
- A torus patch bounded by four short circular arcs can recover its exact
  rectangular UV domain from those arcs. Each candidate boundary must match
  a source circle's centre, radius, axis, endpoints and midpoint within the
  authored tolerance. Partial torus projections use the authored UV window
  to avoid choosing a different branch of a spindle torus. These paths close
  both large toolbox shells without increasing sewing tolerances.

## Licensing

The extracted files have Apache-2.0 SPDX notices. The fork's full
`LICENSE.txt` is installed verbatim alongside our MIT licence;
its title is Tomorrow Open Source Technology License 1.0. No licence notice
is replaced. OCCT remains dynamically linked under LGPL-2.1 with its exception.
