// SPDX-License-Identifier: MIT
#include "torusPatch.h"
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepLib.hxx>
#include <BRep_Tool.hxx>
#include <Geom_ToroidalSurface.hxx>
#include <Geom_Circle.hxx>
#include <ElSLib.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <cmath>
#include <limits>
PXR_NAMESPACE_OPEN_SCOPE
TopoDS_Face UsdSolidOcctTorusPatch(const Handle(Geom_Surface)& surface,
    const std::vector<Handle(Geom_Curve)>& curves,
    const std::vector<std::pair<double,double>>& ranges,
    const GfVec2d& lo,const GfVec2d& hi,double tolerance) {
    auto torus=Handle(Geom_ToroidalSurface)::DownCast(surface);
    if (torus.IsNull() || curves.size()!=4) return {};
    const double pi=std::acos(-1.0),period=2*pi;
    GfVec2d lower(std::numeric_limits<double>::max()),upper(-std::numeric_limits<double>::max());
    for (size_t i=0;i<curves.size();++i) {
        if (Handle(Geom_Circle)::DownCast(curves[i]).IsNull() ||
            ranges[i].second-ranges[i].first>pi+1e-10) return {};
        for (int k=0;k<3;++k) {
            auto point=curves[i]->Value(ranges[i].first+(ranges[i].second-ranges[i].first)*k/2);
            double u,v; ElSLib::Parameters(torus->Torus(),point,u,v);
            GfVec2d uv(u,v);
            for (int j=0;j<2;++j) {
                uv[j]+=period*std::round(((lo[j]+hi[j])/2-uv[j])/period);
                lower[j]=std::min(lower[j],uv[j]); upper[j]=std::max(upper[j],uv[j]);
            }
        }
    }
    if (upper[0]-lower[0]>=pi+1e-10 || upper[1]-lower[1]>=pi+1e-10) return {};
    BRepBuilderAPI_MakeFace make(surface,lower[0],upper[0],lower[1],upper[1],tolerance);
    if (!make.IsDone()) return {};
    auto face=make.Face(); BRepLib::BuildCurves3d(face);
    bool used[4]={false,false,false,false};
    int count=0;
    for (TopExp_Explorer it(face,TopAbs_EDGE);it.More();it.Next()) {
        double first,last;
        auto curve=BRep_Tool::Curve(TopoDS::Edge(it.Current()),first,last);
        auto circle=Handle(Geom_Circle)::DownCast(curve);
        if (circle.IsNull()) return {};
        bool matched=false;
        for (size_t i=0;i<4 && !matched;++i) {
            if (used[i]) continue;
            auto authored=Handle(Geom_Circle)::DownCast(curves[i]);
            if (circle->Location().Distance(authored->Location())>tolerance ||
                std::abs(circle->Radius()-authored->Radius())>tolerance ||
                std::abs(circle->Axis().Direction().Dot(authored->Axis().Direction()))<1-1e-12) continue;
            auto a=curve->Value(first),b=curve->Value(last);
            auto c=curves[i]->Value(ranges[i].first),d=curves[i]->Value(ranges[i].second);
            if (!((a.Distance(c)<=tolerance&&b.Distance(d)<=tolerance)||
                  (a.Distance(d)<=tolerance&&b.Distance(c)<=tolerance))) continue;
            if (curve->Value((first+last)/2).Distance(curves[i]->Value((ranges[i].first+ranges[i].second)/2))>tolerance) continue;
            matched=used[i]=true; ++count;
        }
        if (!matched) return {};
    }
    return count==4?face:TopoDS_Face();
}
PXR_NAMESPACE_CLOSE_SCOPE
