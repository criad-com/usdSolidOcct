// SPDX-License-Identifier: MIT
#ifndef USDSOLIDOCCT_BRIDGE_H
#define USDSOLIDOCCT_BRIDGE_H
#include "api.h"
#include "pxr/pxr.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/vt/array.h"
#include "pxr/usd/usdSolid/brepArray.h"
#include <TopoDS_Shape.hxx>

PXR_NAMESPACE_OPEN_SCOPE

/// Triangle arrays in the input shape's coordinate system. Normals are per
/// vertex; sourceFaceIndices maps each triangle to its OCCT face traversal index.
struct USDSOLIDOCCT_API UsdSolidOcctMesh {
    VtArray<GfVec3d> points;
    VtArray<int> faceVertexCounts;
    VtArray<int> faceVertexIndices;
    VtArray<GfVec3f> normals;
    VtArray<int> sourceFaceIndices;
};

/// Build one Brep in local coordinates at default time. By default sew shared
/// boundaries and assemble closed shells into solids. sew=false is provided
/// for comparison with the upstream tessellator's disconnected face output.
USDSOLIDOCCT_API TopoDS_Shape UsdSolidOcctBuild(
    const UsdSolidBrepArray& brep, size_t brepIndex = 0, bool sew = true);
/// Write one shape as one Brep. Closed periodic faces are split before export.
/// Author in the caller's edit target; unrelated prim properties are preserved.
USDSOLIDOCCT_API void UsdSolidOcctWrite(
    const TopoDS_Shape& shape, UsdSolidBrepArray& brep);
USDSOLIDOCCT_API UsdSolidOcctMesh UsdSolidOcctTessellate(
    const TopoDS_Shape& shape, double linearDeflection = 0.1,
    double angularDeflection = 0.5);
/// Measurements use shape coordinates (SI when the stage uses metres).
/// Volume rejects free sheets; the volume of an empty Boolean result is zero.
USDSOLIDOCCT_API double UsdSolidOcctVolume(const TopoDS_Shape& shape);
USDSOLIDOCCT_API double UsdSolidOcctArea(const TopoDS_Shape& shape);
USDSOLIDOCCT_API double UsdSolidOcctDistance(
    const TopoDS_Shape& a, const TopoDS_Shape& b);
USDSOLIDOCCT_API TopoDS_Shape UsdSolidOcctCommon(
    const TopoDS_Shape& a, const TopoDS_Shape& b);
USDSOLIDOCCT_API size_t UsdSolidOcctFaceCount(const TopoDS_Shape& shape);
USDSOLIDOCCT_API size_t UsdSolidOcctSolidCount(const TopoDS_Shape& shape);
USDSOLIDOCCT_API bool UsdSolidOcctIsValid(const TopoDS_Shape& shape);

PXR_NAMESPACE_CLOSE_SCOPE
#endif
