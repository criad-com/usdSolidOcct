// SPDX-License-Identifier: MIT
#include "bridge.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/sdf/types.h"
#include <BRepAdaptor_Curve.hxx>
#include <BRepTools.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_NurbsConvert.hxx>
#include <BRepLib.hxx>
#include <BRep_Tool.hxx>
#include <BRepBndLib.hxx>
#include <BRepClass3d.hxx>
#include <Bnd_Box.hxx>
#include <GeomAdaptor_Surface.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <GeomConvert.hxx>
#include <Geom2dConvert.hxx>
#include <ShapeUpgrade_ShapeDivideClosed.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Shell.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <gp_Pln.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Cone.hxx>
#include <gp_Sphere.hxx>
#include <gp_Torus.hxx>
#include <gp_Circ.hxx>
#include <gp_Elips.hxx>
#include <gp_Lin.hxx>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE
namespace {
#include "exportHelpers.h"
using UInt = unsigned int;
template<class T> using Arrays = std::map<std::string, std::vector<T>>;
GfVec3d Vec(const gp_XYZ& p) { return {p.X(), p.Y(), p.Z()}; }
GfVec3d Vec(const std::array<double, 3>& p) { return {p[0], p[1], p[2]}; }
TfToken Tok(const char* name) { return TfToken(name); }

struct Writer {
    Arrays<UInt> u;
    Arrays<int> unused;
    Arrays<double> d;
    Arrays<TfToken> t;
    Arrays<GfVec3d> xyz;
    Arrays<GfVec2d> uv;
    Arrays<GfVec2i> pairs;
    std::set<TfToken> apis;
    TopTools_IndexedMapOfShape faces, edges, vertices;
    TopTools_IndexedDataMapOfShapeListOfShape ancestors;
    std::map<int, UInt> edgeIndices, vertexIndices;
    std::map<int, std::vector<UInt>> edgeUses;
    std::vector<int> useFaces;
    std::vector<bool> faceFlips;
    GfVec2d surfaceOffset{0,0};
    double tolerance = 1e-7;
    Bnd_Box bounds;

    void Api(const std::string& name) { apis.insert(TfToken(name)); }
    UInt Vertex(const TopoDS_Vertex& vertex) {
        if (vertex.IsNull()) throw std::runtime_error("Edge has no endpoint vertex");
        int key = vertices.Add(vertex);
        auto found = vertexIndices.find(key);
        if (found != vertexIndices.end()) return found->second;
        UInt index = vertexIndices.size();
        vertexIndices[key] = index;
        gp_Pnt point = BRep_Tool::Pnt(vertex);
        xyz["brep:vertexPoint:point:position"].push_back(Vec(point.XYZ()));
        t["vertex:pointType"].push_back(Tok("BrepPointAPI"));
        Api("BrepPointAPI:vertexPoint");
        bounds.Add(point);
        tolerance = std::max(tolerance, BRep_Tool::Tolerance(vertex));
        return index;
    }
    void Frame(const std::string& prefix, const gp_Ax3& frame, const std::string& location="origin") {
        xyz[prefix + location].push_back(Vec(frame.Location().XYZ()));
        xyz[prefix + "axis"].push_back(Vec(frame.Direction().XYZ()));
        xyz[prefix + "refDirection"].push_back(Vec(frame.XDirection().XYZ()));
    }
    void Curve3(const Out::Crv3& curve) {
        const std::string p = "brep:edge3dNurb:curve3d:nurb:";
        u[p+"order"].push_back(curve.order);
        u[p+"vertexCount"].push_back(curve.n);
        d[p+"knots"].insert(d[p+"knots"].end(), curve.k.begin(), curve.k.end());
        d[p+"weights"].insert(d[p+"weights"].end(), curve.w.begin(), curve.w.end());
        for (const auto& point : curve.cp) {
            xyz[p+"controlVertices"].push_back(Vec(point));
            bounds.Add(gp_Pnt(point[0], point[1], point[2]));
        }
    }
    UInt Edge(TopoDS_Edge edge) {
        edge.Orientation(TopAbs_FORWARD);
        int key = edges.Add(edge);
        auto found = edgeIndices.find(key);
        if (found != edgeIndices.end()) return found->second;
        UInt index = edgeIndices.size();
        edgeIndices[key] = index;
        TopoDS_Vertex start, end;
        TopExp::Vertices(edge, start, end);
        pairs["edge:vertexIndices"].push_back(GfVec2i(Vertex(start), Vertex(end)));
        tolerance = std::max(tolerance, BRep_Tool::Tolerance(edge));
        BRepAdaptor_Curve curve(edge);
        double first = curve.FirstParameter(), last = curve.LastParameter();
        std::string kind;
        switch (curve.GetType()) {
        case GeomAbs_Line: {
            kind = "Line";
            const auto line = curve.Line();
            xyz["brep:edge3dLine:curve3d:line:origin"].push_back(Vec(line.Location().XYZ()));
            xyz["brep:edge3dLine:curve3d:line:direction"].push_back(Vec(line.Direction().XYZ()));
            break;
        }
        case GeomAbs_Circle: {
            kind = "Circle";
            const auto circle = curve.Circle();
            auto frame=gp_Ax3(circle.Position());
            gp_Vec ref=std::cos(first)*gp_Vec(frame.XDirection())+std::sin(first)*gp_Vec(frame.YDirection());
            frame=gp_Ax3(frame.Location(),frame.Direction(),gp_Dir(ref));
            Frame("brep:edge3dCircle:curve3d:circle:", frame, "center");
            d["brep:edge3dCircle:curve3d:circle:radius"].push_back(circle.Radius());
            last-=first; first=0;
            break;
        }
        case GeomAbs_Ellipse: {
            kind = "Ellipse";
            const auto ellipse = curve.Ellipse();
            Frame("brep:edge3dEllipse:curve3d:ellipse:", gp_Ax3(ellipse.Position()), "center");
            d["brep:edge3dEllipse:curve3d:ellipse:xRadius"].push_back(ellipse.MajorRadius());
            d["brep:edge3dEllipse:curve3d:ellipse:yRadius"].push_back(ellipse.MinorRadius());
            break;
        }
        default: {
            kind = "Nurb";
            auto base = BRep_Tool::Curve(edge, first, last);
            auto spline = GeomConvert::CurveToBSplineCurve(new Geom_TrimmedCurve(base, first, last));
            spline->SetNotPeriodic();
            Curve3(extractCurve3d(spline));
            first = spline->FirstParameter(); last = spline->LastParameter();
            break;
        }
        }
        Api("BrepCurve3d"+kind+"API:edge3d"+kind);
        t["edge:curveType"].push_back(TfToken("BrepCurve3d"+kind+"API"));
        d["edge:range"].insert(d["edge:range"].end(), {first, last});
        return index;
    }
    // UV transform from OCCT to the schema. Cone v is axial distance; a
    // negative cone angle is represented by reversing its axis and u/v.
    GfVec2d Surface(const TopoDS_Face& face) {
        auto surface = BRep_Tool::Surface(face);
        auto trimmed = Handle(Geom_RectangularTrimmedSurface)::DownCast(surface);
        if (!trimmed.IsNull()) surface = trimmed->BasisSurface();
        GeomAdaptor_Surface adapter(surface);
        std::string kind;
        GfVec2d scale(1, 1);
        surfaceOffset=GfVec2d(0,0);
        switch (adapter.GetType()) {
        case GeomAbs_Plane:
            kind = "Plane"; Frame("brep:surface:plane:", adapter.Plane().Position());
            scale[1]=adapter.Plane().Position().Direct()?1:-1; break;
        case GeomAbs_Cylinder:
            kind = "Cylinder"; Frame("brep:surface:cylinder:", adapter.Cylinder().Position());
            scale[0]=adapter.Cylinder().Position().Direct()?1:-1;
            d["brep:surface:cylinder:radius"].push_back(adapter.Cylinder().Radius()); break;
        case GeomAbs_Cone: {
            kind = "Cone";
            auto cone = adapter.Cone();
            auto frame = cone.Position();
            const double angle = cone.SemiAngle();
            if (angle < 0) frame = gp_Ax3(frame.Location(), frame.Direction().Reversed(), frame.XDirection());
            Frame("brep:surface:cone:", frame);
            d["brep:surface:cone:radius"].push_back(cone.RefRadius());
            d["brep:surface:cone:semiAngle"].push_back(std::abs(angle));
            scale = GfVec2d((angle < 0 ? -1 : 1)*(cone.Position().Direct()?1:-1), (angle < 0 ? -1 : 1) * std::cos(angle));
            break;
        }
        case GeomAbs_Sphere:
            kind = "Sphere"; Frame("brep:surface:sphere:", adapter.Sphere().Position(), "center");
            scale[0]=adapter.Sphere().Position().Direct()?1:-1;
            d["brep:surface:sphere:radius"].push_back(adapter.Sphere().Radius()); break;
        case GeomAbs_Torus:
            kind = "Torus"; Frame("brep:surface:torus:", adapter.Torus().Position());
            scale[0]=adapter.Torus().Position().Direct()?1:-1;
            d["brep:surface:torus:majorRadius"].push_back(adapter.Torus().MajorRadius());
            d["brep:surface:torus:minorRadius"].push_back(adapter.Torus().MinorRadius()); break;
        default: {
            kind = "Nurb";
            auto spline = Handle(Geom_BSplineSurface)::DownCast(surface);
            if (spline.IsNull()) {
                throw std::runtime_error("Surface conversion did not produce a supported NURBS chart");
            } else spline = Handle(Geom_BSplineSurface)::DownCast(spline->Copy());
            spline->SetUNotPeriodic(); spline->SetVNotPeriodic();
            const std::string p = "brep:surface:nurb:";
            u[p+"uOrder"].push_back(spline->UDegree()+1); u[p+"vOrder"].push_back(spline->VDegree()+1);
            u[p+"uVertexCount"].push_back(spline->NbUPoles()); u[p+"vVertexCount"].push_back(spline->NbVPoles());
            const auto& uk = spline->UKnotSequence(); const auto& vk = spline->VKnotSequence();
            for (int i=uk.Lower(); i<=uk.Upper(); ++i) d[p+"uKnots"].push_back(uk(i));
            for (int i=vk.Lower(); i<=vk.Upper(); ++i) d[p+"vKnots"].push_back(vk(i));
            for (int i=1; i<=spline->NbUPoles(); ++i) for (int j=1; j<=spline->NbVPoles(); ++j) {
                gp_Pnt point = spline->Pole(i,j);
                xyz[p+"controlVertices"].push_back(Vec(point.XYZ()));
                d[p+"weights"].push_back(spline->Weight(i,j)); bounds.Add(point);
            }
            break;
        }
        }
        Api("BrepSurface"+kind+"API");
        t["face:surfaceType"].push_back(TfToken("BrepSurface"+kind+"API"));
        double u0,u1,v0,v1; BRepTools::UVBounds(face,u0,u1,v0,v1);
        if (kind!="Plane" && kind!="Nurb") {
            std::string lower=kind;
            lower[0]=std::tolower(lower[0]);
            const std::string p="brep:surface:"+lower+":";
            auto& ref=xyz[p+"refDirection"].back();
            const auto axis=xyz[p+"axis"].back();
            double start=std::min(u0*scale[0],u1*scale[0]);
            ref=std::cos(start)*ref+std::sin(start)*GfCross(axis,ref);
            surfaceOffset[0]=-start;
        }
        uv["face:range"].emplace_back(std::min(u0*scale[0],u1*scale[0])+surfaceOffset[0],std::min(v0*scale[1],v1*scale[1]));
        uv["face:range"].emplace_back(std::max(u0*scale[0],u1*scale[0])+surfaceOffset[0],std::max(v0*scale[1],v1*scale[1]));
        faceFlips.push_back(scale[0]*scale[1]<0);
        t["face:trimType"].push_back(Tok("general"));
        return scale;
    }
    void Face(TopoDS_Face face, int faceIndex) {
        tolerance = std::max(tolerance, BRep_Tool::Tolerance(face));
        const auto scale = Surface(face);
        const auto offset = surfaceOffset;
        // Loop topology is stored in natural surface orientation.
        face.Orientation(faceFlips.back()?TopAbs_REVERSED:TopAbs_FORWARD);
        auto outer = BRepTools::OuterWire(face);
        if (outer.IsNull()) throw std::runtime_error("Face has no finite outer wire");
        std::vector<TopoDS_Wire> wires{outer};
        for (TopExp_Explorer it(face, TopAbs_WIRE); it.More(); it.Next())
            if (!it.Current().IsSame(outer)) wires.push_back(TopoDS::Wire(it.Current()));
        u["face:loopCount"].push_back(wires.size());
        // A collapsed isoparameter boundary is a singularity, not a 3D edge.
        // Rectangular singular charts are represented by their exact UV range;
        // omit pcurves on that face rather than invent a zero-length edge.
        double u0,u1,v0,v1; BRepTools::UVBounds(face,u0,u1,v0,v1);
        auto surface=BRep_Tool::Surface(face);
        auto collapsed=[&](double a,double b,double c,double e) {
            const auto p=surface->Value(a,b);
            return p.Distance(surface->Value(c,e))<tolerance &&
                p.Distance(surface->Value((a+c)/2,(b+e)/2))<tolerance;
        };
        bool singular=collapsed(u0,v0,u1,v0)||collapsed(u0,v1,u1,v1)||
            collapsed(u0,v0,u0,v1)||collapsed(u1,v0,u1,v1);
        bool rectangular=wires.size()==1;
        for (TopExp_Explorer it(face,TopAbs_EDGE);it.More()&&rectangular;it.Next()) {
            double first,last;
            auto pc=BRep_Tool::CurveOnSurface(TopoDS::Edge(it.Current()),face,first,last);
            if (pc.IsNull()) { rectangular=false; break; }
            auto spline=Geom2dConvert::CurveToBSplineCurve(new Geom2d_TrimmedCurve(pc,first,last));
            bool onU0=true,onU1=true,onV0=true,onV1=true;
            for (int i=1;i<=spline->NbPoles();++i) {
                auto p=spline->Pole(i);
                onU0&=std::abs(p.X()-u0)<1e-8; onU1&=std::abs(p.X()-u1)<1e-8;
                onV0&=std::abs(p.Y()-v0)<1e-8; onV1&=std::abs(p.Y()-v1)<1e-8;
            }
            rectangular=onU0||onU1||onV0||onV1;
        }
        const bool rangedPole=singular&&rectangular;
        if (rangedPole) t["face:trimType"].back()=Tok("rectangular");
        for (const auto& wire : wires) {
            UInt count = 0, loopVertex = 0;
            for (BRepTools_WireExplorer it(wire, face); it.More(); it.Next()) {
                const auto edge = it.Current();
                if (BRep_Tool::Degenerated(edge)) {
                    loopVertex = Vertex(TopExp::FirstVertex(edge));
                    continue;
                }
                UInt index = Edge(edge);
                UInt use = u["edgeuse:edgeIndex"].size();
                u["edgeuse:edgeIndex"].push_back(index);
                t["edgeuse:orientationType"].push_back(Tok(edge.Orientation()==TopAbs_REVERSED ? "opposite" : "same"));
                edgeUses[edges.FindIndex(edge)].push_back(use);
                useFaces.push_back(faceIndex);
                const std::string p = "brep:curveUv:nurb:";
                if (rangedPole) {
                    u[p+"order"].push_back(0); u[p+"vertexCount"].push_back(0);
                    ++count; continue;
                }
                double first,last;
                auto pc = BRep_Tool::CurveOnSurface(edge, face, first, last);
                if (pc.IsNull()) throw std::runtime_error("Edge has no pcurve on its face");
                auto spline = Geom2dConvert::CurveToBSplineCurve(new Geom2d_TrimmedCurve(pc, first, last));
                spline->SetNotPeriodic();
                if (edge.Orientation()==TopAbs_REVERSED) spline->Reverse();
                auto curve = extractCurve2d(spline);
                u[p+"order"].push_back(curve.order); u[p+"vertexCount"].push_back(curve.n);
                d[p+"knots"].insert(d[p+"knots"].end(),curve.k.begin(),curve.k.end());
                d[p+"weights"].insert(d[p+"weights"].end(),curve.w.begin(),curve.w.end());
                for (const auto& point:curve.cp) uv[p+"controlVertices"].emplace_back(point[0]*scale[0]+offset[0],point[1]*scale[1]+offset[1]);
                Api("BrepCurveUvNurbAPI");
                ++count;
            }
            u["loop:edgeuseCount"].push_back(count);
            u["loop:vertexIndex"].push_back(loopVertex);
        }
    }
    void Shell(const TopoDS_Shape& shell, bool reverse) {
        UInt count = 0;
        for (TopExp_Explorer it(shell,TopAbs_FACE);it.More();it.Next()) {
            int index = faces.FindIndex(it.Current())-1;
            if (index<0) throw std::runtime_error("Shell references an unknown face");
            u["faceuse:faceIndex"].push_back(index);
            bool opposite = it.Current().Orientation()==TopAbs_REVERSED;
            opposite=opposite!=faceFlips[index];
            t["faceuse:orientationType"].push_back(Tok(opposite!=reverse ? "opposite" : "same"));
            ++count;
        }
        u["shell:faceuseCount"].push_back(count);
        u["shell:wireEdgeCount"].push_back(0);
        t["shell:pointType"].push_back(Tok("none"));
    }
    template<class T> void Flush(const Arrays<T>& arrays, const UsdPrim& prim) {
        for (const auto& [name, values]:arrays) {
            auto attr=prim.GetAttribute(TfToken(name));
            if (!attr || !attr.Set(VtArray<T>(values.begin(),values.end())))
                throw std::runtime_error("Cannot author "+name);
        }
    }
    UsdStageRefPtr Export(TopoDS_Shape shape) {
        if (TopExp_Explorer(shape,TopAbs_EDGE,TopAbs_FACE).More() ||
            TopExp_Explorer(shape,TopAbs_VERTEX,TopAbs_EDGE).More())
            throw std::invalid_argument("Free wires and isolated points are not supported");
        // Copy before healing/splitting: callers retain ownership of their shape.
        shape = BRepBuilderAPI_Copy(shape).Shape();
        // Convert the topology with its pcurves: converting a surface alone
        // can change its parameterization and invalidate every existing trim.
        bool convert=false;
        for (TopExp_Explorer it(shape,TopAbs_FACE);it.More();it.Next()) {
            GeomAdaptor_Surface surface(BRep_Tool::Surface(TopoDS::Face(it.Current())));
            const auto type=surface.GetType();
            convert |= type!=GeomAbs_Plane && type!=GeomAbs_Cylinder && type!=GeomAbs_Cone &&
                type!=GeomAbs_Sphere && type!=GeomAbs_Torus && type!=GeomAbs_BSplineSurface;
        }
        if (convert) shape=BRepBuilderAPI_NurbsConvert(shape,true).Shape();
        ShapeUpgrade_ShapeDivideClosed split(shape); split.Perform(); shape=split.Result();
        BRepLib::BuildCurves3d(shape);
        TopExp::MapShapes(shape,TopAbs_FACE,faces);
        if (faces.IsEmpty()) throw std::invalid_argument("Write requires faces");
        TopExp::MapShapesAndAncestors(shape,TopAbs_EDGE,TopAbs_FACE,ancestors);
        for (int i=1;i<=faces.Extent();++i) Face(TopoDS::Face(faces(i)),i-1);
        // Manifold topology: one exterior region, one region per solid, and
        // one bounded void region per cavity. Shell order is outer-first.
        std::vector<std::vector<TopoDS_Shape>> solids;
        for (TopExp_Explorer it(shape,TopAbs_SOLID);it.More();it.Next()) {
            std::vector<TopoDS_Shape> shells;
            auto outer=BRepClass3d::OuterShell(TopoDS::Solid(it.Current()));
            shells.push_back(outer);
            for (TopExp_Explorer sh(it.Current(),TopAbs_SHELL);sh.More();sh.Next())
                if (!sh.Current().IsSame(outer)) shells.push_back(sh.Current());
            solids.push_back(shells);
        }
        UInt regions=1;
        t["region:type"].push_back(Tok("voidRegion"));
        if (solids.empty()) {
            u["region:shellCount"].push_back(1);
            Shell(shape,false);
            auto count=u["shell:faceuseCount"].back();
            u["shell:faceuseCount"].back()=2*count;
            for (UInt i=0;i<count;++i) {
                u["faceuse:faceIndex"].push_back(u["faceuse:faceIndex"][i]);
                t["faceuse:orientationType"].push_back(t["faceuse:orientationType"][i]==Tok("same") ? Tok("opposite") : Tok("same"));
            }
        } else {
            if (TopExp_Explorer(shape,TopAbs_FACE,TopAbs_SOLID).More())
                throw std::invalid_argument("Mixed solids and free faces are not supported");
            u["region:shellCount"].push_back(solids.size());
            for (const auto& shells:solids) Shell(shells.front(),false);
            for (const auto& shells:solids) {
                ++regions; t["region:type"].push_back(Tok("solidRegion"));
                u["region:shellCount"].push_back(shells.size());
                for (const auto& shell:shells) Shell(shell,true);
                for (size_t i=1;i<shells.size();++i) {
                    ++regions; t["region:type"].push_back(Tok("voidRegion"));
                    u["region:shellCount"].push_back(1); Shell(shells[i],false);
                }
            }
        }
        u["brep:regionCount"].push_back(regions);
        auto& next=u["edgeuse:nextRadialEUIndex"];
        auto& entry=t["edgeuse:thisRadialEntryType"];
        next.resize(useFaces.size()); entry.resize(useFaces.size());
        for (const auto& [key, uses]:edgeUses) {
            const auto& connected=ancestors.FindFromKey(edges(key));
            if (connected.Extent()>2) throw std::invalid_argument("Non-manifold radial ordering is not supported");
            for (size_t i=0;i<uses.size();++i) {
                next[uses[i]]=uses[(i+1)%uses.size()];
                entry[uses[i]]=t["edgeuse:orientationType"][uses[i]]==Tok("same") ? Tok("topEntry") : Tok("bottomEntry");
            }
        }
        BRepBndLib::AddOptimal(shape,bounds,false,false);
        double x0,y0,z0,x1,y1,z1; bounds.Get(x0,y0,z0,x1,y1,z1);
        xyz["brep:extent"]={GfVec3d(x0,y0,z0),GfVec3d(x1,y1,z1)};
        d["brep:intersectTol3d"]={tolerance};
        if (!apis.count(Tok("BrepCurveUvNurbAPI"))) {
            u.erase("brep:curveUv:nurb:order");
            u.erase("brep:curveUv:nurb:vertexCount");
        }
        auto stage=UsdStage::CreateInMemory();
        auto prim=UsdSolidBrepArray::Define(stage,SdfPath("/Body")).GetPrim();
        for (const auto& api:apis) prim.AddAppliedSchema(api);
        Flush(u,prim); Flush(d,prim); Flush(t,prim); Flush(xyz,prim); Flush(uv,prim); Flush(pairs,prim);
        prim.GetAttribute(Tok("extent")).Set(VtArray<GfVec3f>{GfVec3f(x0,y0,z0),GfVec3f(x1,y1,z1)});
        return stage;
    }
};
bool GeometryProperty(const std::string& name) {
    return name=="extent" || name.rfind("brep:",0)==0 || name.rfind("region:",0)==0 ||
        name.rfind("shell:",0)==0 || name.rfind("face:",0)==0 || name.rfind("faceuse:",0)==0 ||
        name.rfind("edge:",0)==0 || name.rfind("edgeuse:",0)==0 || name.rfind("wireEdge:",0)==0 ||
        name.rfind("loop:",0)==0 || name.rfind("vertex:",0)==0;
}
}
void UsdSolidOcctWrite(const TopoDS_Shape& shape, UsdSolidBrepArray& brep) {
    if (shape.IsNull() || !brep) throw std::invalid_argument("Write requires a shape and BrepArray");
    auto temporary=Writer().Export(shape);
    const auto source=temporary->GetPrimAtPath(SdfPath("/Body"));
    auto target=brep.GetPrim();
    for (const auto& api:target.GetAppliedSchemas())
        if (api.GetString().rfind("Brep",0)==0) target.RemoveAppliedSchema(api);
    for (const auto& api:source.GetAppliedSchemas()) target.AddAppliedSchema(api);
    for (const auto& attr:target.GetAuthoredAttributes())
        if (GeometryProperty(attr.GetName()) && !source.GetAttribute(attr.GetName()).HasAuthoredValueOpinion()) attr.Block();
    for (const auto& attr:source.GetAuthoredAttributes()) {
        VtValue value; attr.Get(&value);
        if (!target.CreateAttribute(attr.GetName(),attr.GetTypeName(),false,attr.GetVariability()).Set(value))
            throw std::runtime_error("Cannot write BrepArray geometry");
    }
}
PXR_NAMESPACE_CLOSE_SCOPE
