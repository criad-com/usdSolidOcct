"""Extract the pinned builder and mesh routine without the Hydra adapter."""
from pathlib import Path
import sys


def extract(source, target):
    source = Path(source) / "pxr/imaging/plugin/hdOcct"
    target = Path(target)
    target.mkdir(parents=True, exist_ok=True)
    for name in ("brepBuilder.h", "brepBuilder.cpp"):
        text = (source / name).read_text()
        text = text.replace("UsdSolidBrepBuilder", "UsdSolidOcctBrepBuilder")
        text = text.replace("USDSOLIDTESSELLATOR_API", "USDSOLIDOCCT_API")
        # Sewing belongs to the bridge contract, not process environment state.
        start = text.find("    // EXPERIMENTAL (env HDOCCT_SEW=1)")
        if start >= 0:
            end = text.index("    return compound;", start)
            text = text[:start] + text[end:]
        if name == "brepBuilder.cpp":
            # The rendering implementation enlarged vertex tolerances tenfold,
            # which can consume legitimate short edges during exact sewing.
            text = text.replace("std::max(1.0e-4, intersectTol3d * 10.0)", "intersectTol3d")
            # Curve synthesis needs tighter accuracy than the sewing tolerance
            # to retain the 1e-9 relative volume contract across NURBS trims.
            text = text.replace("BRepLib::BuildCurves3d(face, intersectTol3d)",
                                "BRepLib::BuildCurves3d(face, std::min(intersectTol3d, 1e-10))")
            text = text.replace("BRepLib::SameParameter(face, intersectTol3d,",
                                "BRepLib::SameParameter(face, std::min(intersectTol3d, 1e-10),")
            projection = '''                                Handle(Geom2d_Curve) pc =
                                    GeomProjLib::Curve2d(c3, f, l, surface);'''
            assert text.count(projection)==1
            text = text.replace(projection, '''                                Handle(Geom2d_Curve) pc;
                                const auto lo=data.faceRange[2*faceIdx];
                                const auto hi=data.faceRange[2*faceIdx+1];
                                if (surface->IsKind(STANDARD_TYPE(Geom_ToroidalSurface)) &&
                                    hi[0]-lo[0]<3.141592653589793 && hi[1]-lo[1]<3.141592653589793) {
                                    double tolerance=intersectTol3d;
                                    pc=GeomProjLib::Curve2d(c3,f,l,surface,
                                        lo[0],hi[0],lo[1],hi[1],tolerance);
                                } else pc=GeomProjLib::Curve2d(c3,f,l,surface);''')
            # Explicit rectangular charts include natural singular boundaries.
            # Constructing them from 3D edges loses collapsed UV boundary lines.
            marker = "    if (!hasTrimCurves) {"
            helper = '''    auto rangedFace = [&](const Handle(Geom_Surface)& surface, size_t faceIdx) {
        if (2*faceIdx+1 >= data.faceRange.size() ||
            data.faceLoopCount[faceIdx] > 1) return TopoDS_Face();
        const auto lo=data.faceRange[2*faceIdx];
        const auto hi=data.faceRange[2*faceIdx+1];
        bool rectangular=faceIdx<data.faceTrimType.size() &&
            data.faceTrimType[faceIdx]==TfToken("rectangular");
        const double vLo=_ConeAxialToSlantV(surface,lo[1]);
        const double vHi=_ConeAxialToSlantV(surface,hi[1]);
        auto collapsed=[&](double u0,double v0,double u1,double v1) {
            auto p=surface->Value(u0,v0);
            return p.Distance(surface->Value(u1,v1))<intersectTol3d &&
                p.Distance(surface->Value((u0+u1)/2,(v0+v1)/2))<intersectTol3d;
        };
        const bool singular=collapsed(lo[0],vLo,hi[0],vLo)||collapsed(lo[0],vHi,hi[0],vHi)||
            collapsed(lo[0],vLo,lo[0],vHi)||collapsed(hi[0],vLo,hi[0],vHi);
        if (!rectangular && singular && data.faceLoopCount[faceIdx]==1) {
            size_t loop=0,eu=0,cp=0;
            for (size_t f=0;f<faceIdx;++f) loop+=data.faceLoopCount[f];
            for (size_t l=0;l<loop;++l) eu+=data.loopEdgeuseCount[l];
            for (size_t i=0;i<eu && i<data.trimCurveVertexCount.size();++i)
                cp+=data.trimCurveVertexCount[i];
            rectangular=loop<data.loopEdgeuseCount.size() && data.loopEdgeuseCount[loop]>0;
            for (size_t i=0;rectangular && i<data.loopEdgeuseCount[loop];++i,++eu) {
                if (eu>=data.trimCurveVertexCount.size()) { rectangular=false; break; }
                const size_t n=data.trimCurveVertexCount[eu];
                if (!n || cp+n>data.trimCurveControlVertices.size()) { rectangular=false; break; }
                bool u0=true,u1=true,v0=true,v1=true;
                for (size_t j=0;j<n;++j) {
                    const auto p=data.trimCurveControlVertices[cp++];
                    u0 &= std::abs(p[0]-lo[0])<1e-8; u1 &= std::abs(p[0]-hi[0])<1e-8;
                    v0 &= std::abs(p[1]-lo[1])<1e-8; v1 &= std::abs(p[1]-hi[1])<1e-8;
                }
                rectangular=u0||u1||v0||v1;
            }
        }
        if (!rectangular) return TopoDS_Face();
        BRepBuilderAPI_MakeFace make(surface,lo[0],hi[0],
            _ConeAxialToSlantV(surface,lo[1]),_ConeAxialToSlantV(surface,hi[1]),intersectTol3d);
        if (!make.IsDone()) throw std::runtime_error("Rectangular face construction failed");
        return make.Face();
    };

'''
            assert text.count(marker) == 1
            text = text.replace(marker, helper + marker)
            marker = "            // Build a trimmed face from the loops' 3D edge wires."
            text = text.replace(marker, '''            if (auto face=rangedFace(surface,faceIdx); !face.IsNull()) {
                faces.push_back(applyFaceuseOrientation(face,fi));
                faceNeedsFlip.push_back(false);
                continue;
            }
            if (numLoops==1 && data.loopEdgeuseCount[loopStartIdx]==4) {
                std::vector<Handle(Geom_Curve)> curves;
                std::vector<std::pair<double,double>> ranges;
                for (size_t i=0;i<4;++i) {
                    const size_t e=data.edgeuseEdgeIndex[euStartIdx+i];
                    curves.push_back(edge3dCurves[e]);
                    ranges.emplace_back(data.edgeRange[2*e],data.edgeRange[2*e+1]);
                }
                auto face=UsdSolidOcctTorusPatch(surface,curves,ranges,
                    data.faceRange[2*faceIdx],data.faceRange[2*faceIdx+1],intersectTol3d);
                if (!face.IsNull()) {
                    faces.push_back(applyFaceuseOrientation(face,fi));
                    faceNeedsFlip.push_back(false);
                    continue;
                }
            }
''' + marker)
            marker = "            bool faceOk = true;"
            text = text.replace(marker, '''            if (auto face=rangedFace(surface,faceIdx); !face.IsNull()) {
                for (int li=0;li<numLoops;++li) {
                    currentEdgeuse+=data.loopEdgeuseCount[currentLoop++];
                }
                faces.push_back(applyFaceuseOrientation(face,fi));
                continue;
            }
''' + marker)
            marker = "                        if (!c2d.IsNull()) {"
            replacement = '''                        if (!c2d.IsNull()) {
                            auto cone=Handle(Geom_ConicalSurface)::DownCast(surface);
                            if (!cone.IsNull()) {
                                for (int pole=1;pole<=c2d->NbPoles();++pole) {
                                    auto p=c2d->Pole(pole);
                                    p.SetY(p.Y()/std::cos(cone->SemiAngle()));
                                    c2d->SetPole(pole,p);
                                }
                            }
'''
            assert text.count(marker) == 1
            text = text.replace(marker, replacement)
            text = '#include <stdexcept>\n' + text
            text = text.replace("PXR_NAMESPACE_OPEN_SCOPE", '#include "torusPatch.h"\nPXR_NAMESPACE_OPEN_SCOPE', 1)
        (target / name).write_text(text)
    text = (source / "tessellator.cpp").read_text()
    headers = text[text.index("// OpenCascade meshing"):text.index("PXR_NAMESPACE_OPEN_SCOPE")]
    function = text[text.index("UsdSolidTessellationResult _ExtractMesh("):text.index("/// Merge multiple")]
    function = function.replace("UsdSolidTessellationResult", "UsdSolidOcctMesh")
    function = function.replace("    result.success = false;", "")
    function = function.replace("    result.success = true;", "")
    import re
    function = re.sub(r'        result.errorMessage = "([^"]+)";\n        return result;',
                      r'        throw std::runtime_error("\1");', function)
    function = function.replace("    const UsdSolidTessellationParams& params,\n    size_t brepIndex)",
        "    double linearDeflection, double angularDeflection)")
    a = function.index("    // Run BRepMesh tessellation")
    b = function.index("    BRepMesh_IncrementalMesh mesher(")
    function = function[:a] + '''    if (!(linearDeflection > 0) || !(angularDeflection > 0) ||
        !std::isfinite(linearDeflection) || !std::isfinite(angularDeflection))
        throw std::invalid_argument("Deflections must be finite and positive");
    const double deflection = linearDeflection;
''' + function[b:]
    function = function.replace("_ExtractMesh(", "UsdSolidOcctTessellate(")
    function = function.replace("params.angularDeflection", "angularDeflection")
    function = function.replace("params.computeNormals", "true").replace("params.computeUVs", "false")
    function = re.sub(r"^.*result\.(uvs|faceBrepIndices).*$", "", function, flags=re.M)
    function = function.replace("                    GfVec2f((float)uv.X(), (float)uv.Y());", "")
    function = function.replace("faceSolidFaceIndices", "sourceFaceIndices")
    (target / "tessellate.cpp").write_text(
        '// Copyright 2024 NVIDIA Corporation\n// SPDX-License-Identifier: Apache-2.0\n'
        '#include "bridge.h"\n#include <stdexcept>\n' + headers +
        "PXR_NAMESPACE_OPEN_SCOPE\n" + function + "PXR_NAMESPACE_CLOSE_SCOPE\n")
    exporter = (source / "testenv/tools/brepExport.cpp").read_text()
    structs = exporter[exporter.index("struct Out {"):exporter.index("// Normalize a gp_Dir")]
    helpers = exporter[exporter.index("static Out::Surf extractSurface("):
                       exporter.index("// Build a 2-pole placeholder")]
    (target / "exportHelpers.h").write_text(
        "// SPDX-License-Identifier: Apache-2.0\n" + structs + helpers)


if __name__ == "__main__":
    extract(*sys.argv[1:])
