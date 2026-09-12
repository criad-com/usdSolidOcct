# Changelog

## 0.1.5

- Make public re-pin verification self-contained with the recorded v0.1.4
  gate results and cache publication counts.
- Align package, CMake and gate versions; identify the existing native evidence
  as v0.1.4. Native rebuild and cache receipt regeneration remain pending review.

## 0.1.4

- public re-pin: aeco-toolchain v0.4.0; usdaeco-toolchain v0.3.10; usdSolid v0.1.4.
- Consume usdSolid as a source input with explicit, unchanged upstream schema,
  validator and fixture revisions; retain the existing requirement range.
- Record checked forge revisions and observed public release revisions.
  Add shared S04 and a kit S05 equivalent for release tags and version agreement,
  preserving the external OpenUSD fork URLs.
- Advance package and CMake metadata together; update the local core override
  to the toolchain's v0.9.2 test input. Record the new native receipt and
  remaining public-resolution and full-flake-check limits.
- Rebuild the native runtime, republish all 90 closure paths and regenerate its
  receipt and acceptance record. Confirm 73 exported layers, 40 fixture layers
  and the generated schema are byte-identical; geometry measurements are unchanged.
- Add an explicit acceptance publication command and an external-registry option
  to publish the complete closure without relying on an upstream cache.

## 0.1.3

- Re-pin usdaeco-toolchain to v0.3.9 for its Python package version fix; other dependency pins and requirement ranges unchanged.
- Native rebuild and cache receipt regeneration remain pending review.

## 0.1.2

- Public names → github.com/criad-com.
- Update the usdaeco-toolchain pin for the public repository names; other dependency pins and requirement ranges unchanged.

## 0.1.1

- Re-pin to train aeco-0.7.0: usdaeco-toolchain v0.3.5; other kit pins and requirement ranges unchanged.

## 0.1.0

- Licence: MIT for our code; extracted upstream files keep their notices; OCCT LGPL-2.1 shared linking only.
- Extract the pinned OCCT builder and tessellation routine behind an exported C++ library.
- Add Python bindings, measurement, distance and common operations.
- Add native and installed Python smoke checks.
- Write analytic and NURBS surfaces, analytic edges and pcurves with periodic splitting.
- Preserve solid regions, cavities and multiple Breps; reject incomplete solid assembly.
- Verify twelve native fixtures and all eleven toolbox components against the native validators.
- Add an installed CMake consumer, plugin-free Mesh example and explicit corpus limitations.
