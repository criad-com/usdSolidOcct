// SPDX-License-Identifier: MIT
#include "bridge.h"
#include "brepBuilder.h"
#include <BRepAlgoAPI_Common.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <cmath>
#include <stdexcept>
#include <numeric>

PXR_NAMESPACE_OPEN_SCOPE
void UsdSolidOcctValidateInput(const UsdSolidBrepArray&);
namespace {
void RequireShape(const TopoDS_Shape& shape) {
    if (shape.IsNull()) throw std::invalid_argument("Null OCCT shape");
}
template<class T> VtArray<T> Read(const UsdPrim& prim, const char* name) {
    VtArray<T> result;
    if (!prim.GetAttribute(TfToken(name)).Get(&result))
        throw std::invalid_argument(std::string("Missing BrepArray attribute: ")+name);
    return result;
}
template<class T> size_t Sum(const VtArray<T>& a, size_t end) {
    if (end>a.size()) throw std::invalid_argument("Invalid topology count cascade");
    return std::accumulate(a.begin(),a.begin()+end,size_t(0));
}
}

TopoDS_Shape UsdSolidOcctBuild(const UsdSolidBrepArray& brep, size_t index, bool sew) {
    if (!brep) throw std::invalid_argument("Expected a valid BrepArray");
    VtArray<unsigned int> counts;
    brep.GetBrepRegionCountAttr().Get(&counts);
    if (index >= counts.size()) throw std::out_of_range("Brep index out of range");
    VtArray<double> tolerances;
    brep.GetBrepIntersectTol3dAttr().Get(&tolerances);
    if (tolerances.size() != counts.size())
        throw std::invalid_argument("Expected one intersection tolerance per Brep");
    const double tolerance = tolerances[index];
    if (!(tolerance > 0) || !std::isfinite(tolerance))
        throw std::invalid_argument("Invalid Brep intersection tolerance");
    const auto prim=brep.GetPrim();
    UsdSolidOcctValidateInput(brep);
    const auto regionShells=Read<unsigned int>(prim,"region:shellCount");
    const auto regionTypes=Read<TfToken>(prim,"region:type");
    const auto shellUses=Read<unsigned int>(prim,"shell:faceuseCount");
    const auto faceIndices=Read<unsigned int>(prim,"faceuse:faceIndex");
    const auto orientations=Read<TfToken>(prim,"faceuse:orientationType");
    const auto faceLoops=Read<unsigned int>(prim,"face:loopCount");
    const size_t regionStart=Sum(counts,index), regionEnd=regionStart+counts[index];
    if (regionTypes.size()!=regionShells.size() || Sum(counts,counts.size())!=regionTypes.size() ||
        Sum(regionShells,regionShells.size())!=shellUses.size() ||
        Sum(shellUses,shellUses.size())!=faceIndices.size() ||
        faceIndices.size()!=orientations.size() || faceIndices.size()!=2*faceLoops.size())
        throw std::invalid_argument("Invalid region/shell/faceuse array sizes");
    const size_t shellStart=Sum(regionShells,regionStart), shellEnd=Sum(regionShells,regionEnd);
    const size_t useStart=Sum(shellUses,shellStart), useEnd=Sum(shellUses,shellEnd);
    const size_t faceStart=useStart/2, faceCount=(useEnd-useStart)/2;
    for (size_t i=useStart;i<useEnd;++i)
        if (faceIndices[i]<faceStart || faceIndices[i]>=faceStart+faceCount)
            throw std::invalid_argument("Faceuse index outside its Brep");
    auto result = UsdSolidOcctBrepBuilder().BuildSingleBrep(prim,index);
    if (!result || result->IsNull()) throw std::runtime_error("Brep reconstruction failed");
    TopTools_IndexedMapOfShape builtFaces;
    TopExp::MapShapes(*result,TopAbs_FACE,builtFaces);
    if (size_t(builtFaces.Extent())!=faceCount)
        throw std::runtime_error("Brep reconstruction omitted faces");
    if (!sew) return *result;
    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    size_t shellIndex=shellStart, use=useStart;
    bool hasSolids=false;
    for (size_t r=regionStart;r<regionEnd;++r) {
        const bool isSolid=regionTypes[r]==TfToken("solidRegion");
        TopoDS_Solid solid;
        if (isSolid) { builder.MakeSolid(solid); hasSolids=true; }
        for (size_t s=0;s<regionShells[r];++s,++shellIndex) {
            const size_t end=use+shellUses[shellIndex];
            if (!isSolid) { use=end; continue; }
            BRepBuilderAPI_Sewing sewing(tolerance);
            TopTools_IndexedMapOfShape expectedFaces;
            for (;use<end;++use) {
                auto face=TopoDS::Face(builtFaces(faceIndices[use]-faceStart+1));
                // A solid's faceuse points into the solid. OCCT uses the
                // outward orientation, including inward-facing cavity walls.
                face.Orientation(orientations[use]==TfToken("same")?TopAbs_REVERSED:TopAbs_FORWARD);
                expectedFaces.Add(face);
                sewing.Add(face);
            }
            sewing.Perform();
            auto sewn=sewing.SewedShape();
            if (UsdSolidOcctFaceCount(sewn)!=size_t(expectedFaces.Extent()))
                throw std::runtime_error("Sewing changed the authored face count");
            TopTools_IndexedMapOfShape shells;
            TopExp::MapShapes(sewn,TopAbs_SHELL,shells);
            if (sewn.ShapeType()==TopAbs_FACE) {
                TopoDS_Shell shell; builder.MakeShell(shell); builder.Add(shell,sewn);
                shells.Add(shell);
            }
            if (shells.Extent()!=1 || !BRep_Tool::IsClosed(shells(1)))
                throw std::runtime_error("Solid region shell did not sew closed");
            if (UsdSolidOcctFaceCount(shells(1))!=size_t(expectedFaces.Extent()))
                throw std::runtime_error("Sewing left faces outside the solid shell");
            builder.Add(solid,shells(1));
        }
        if (isSolid) {
            if (!BRepLib::OrientClosedSolid(solid))
                throw std::runtime_error("Cannot orient solid region");
            builder.Add(compound,solid);
        }
    }
    if (!hasSolids) {
        BRepBuilderAPI_Sewing sewing(tolerance);
        sewing.Add(*result); sewing.Perform();
        RequireShape(sewing.SewedShape());
        return sewing.SewedShape();
    }
    return compound;
}

double UsdSolidOcctVolume(const TopoDS_Shape& shape) {
    RequireShape(shape);
    if (TopExp_Explorer(shape,TopAbs_FACE,TopAbs_SOLID).More())
        throw std::invalid_argument("Volume requires closed solids; use Area for sheets");
    GProp_GProps properties;
    const double error=BRepGProp::VolumePropertiesGK(shape,properties,1e-12,false,true);
    if (error<0 || !std::isfinite(properties.Mass()))
        throw std::runtime_error("Volume integration failed");
    return properties.Mass();
}
double UsdSolidOcctArea(const TopoDS_Shape& shape) {
    RequireShape(shape);
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(shape, properties, 1e-12);
    return properties.Mass();
}
double UsdSolidOcctDistance(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    RequireShape(a); RequireShape(b);
    BRepExtrema_DistShapeShape distance(a, b);
    if (!distance.IsDone()) throw std::runtime_error("Distance calculation failed");
    return distance.Value();
}
TopoDS_Shape UsdSolidOcctCommon(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    RequireShape(a); RequireShape(b);
    BRepAlgoAPI_Common common(a, b);
    if (!common.IsDone()) throw std::runtime_error("Boolean common failed");
    return common.Shape();
}
size_t UsdSolidOcctFaceCount(const TopoDS_Shape& shape) {
    RequireShape(shape);
    TopTools_IndexedMapOfShape faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    return faces.Extent();
}
size_t UsdSolidOcctSolidCount(const TopoDS_Shape& shape) {
    RequireShape(shape);
    TopTools_IndexedMapOfShape solids;
    TopExp::MapShapes(shape, TopAbs_SOLID, solids);
    return solids.Extent();
}
bool UsdSolidOcctIsValid(const TopoDS_Shape& shape) {
    return !shape.IsNull() && BRepCheck_Analyzer(shape).IsValid();
}
PXR_NAMESPACE_CLOSE_SCOPE
