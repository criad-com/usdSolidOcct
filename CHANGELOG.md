# Changelog

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
