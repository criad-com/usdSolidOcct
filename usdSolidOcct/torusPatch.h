// SPDX-License-Identifier: MIT
#ifndef USDSOLIDOCCT_TORUS_PATCH_H
#define USDSOLIDOCCT_TORUS_PATCH_H
#include "pxr/pxr.h"
#include "pxr/base/gf/vec2d.h"
#include <Geom_Surface.hxx>
#include <Geom_Curve.hxx>
#include <TopoDS_Face.hxx>
#include <vector>
#include <utility>
PXR_NAMESPACE_OPEN_SCOPE
TopoDS_Face UsdSolidOcctTorusPatch(const Handle(Geom_Surface)& surface,
    const std::vector<Handle(Geom_Curve)>& curves,
    const std::vector<std::pair<double,double>>& ranges,
    const GfVec2d& lo,const GfVec2d& hi,double tolerance);
PXR_NAMESPACE_CLOSE_SCOPE
#endif
