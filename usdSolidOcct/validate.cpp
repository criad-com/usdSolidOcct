// SPDX-License-Identifier: MIT
// Bounds checks at the library boundary protect the extracted packed reader.
#include "bridge.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
PXR_NAMESPACE_OPEN_SCOPE
namespace {
template<class T> VtArray<T> Read(const UsdPrim& prim,const std::string& name) {
    VtArray<T> values;
    prim.GetAttribute(TfToken(name)).Get(&values);
    return values;
}
void Require(bool condition,const std::string& property) {
    if (!condition) throw std::invalid_argument("Malformed BrepArray property: "+property);
}
template<class T> VtArray<T> Sized(const UsdPrim& p,const std::string& name,size_t count) {
    auto values=Read<T>(p,name);
    Require(values.size()==count,name);
    return values;
}
template<class T> size_t Sum(const VtArray<T>& values) {
    return std::accumulate(values.begin(),values.end(),size_t(0));
}
void Finite(double v,const std::string& name) { Require(std::isfinite(v),name); }
template<class T> void Finite(const T& v,const std::string& name) {
    for (size_t i=0;i<T::dimension;++i) Finite(v[i],name);
}
template<class T> void Coordinates(const UsdPrim& p,const std::string& name,size_t count) {
    for (const auto& v:Sized<T>(p,name,count)) Finite(v,name);
}
void Knots(const UsdPrim& p,const std::string& name,const VtArray<unsigned int>& counts,
           const VtArray<unsigned int>& orders) {
    auto knots=Sized<double>(p,name,Sum(counts)+Sum(orders));
    size_t offset=0;
    for (size_t i=0;i<counts.size();++i) {
        const size_t end=offset+counts[i]+orders[i];
        for (size_t k=offset;k<end;++k) {
            Finite(knots[k],name);
            if (k>offset) Require(knots[k]>=knots[k-1],name);
        }
        if (orders[i]) Require(knots[offset+counts[i]]>knots[offset+orders[i]-1],name);
        offset=end;
    }
}
void Weights(const UsdPrim& p,const std::string& prefix,size_t count) {
    auto weights=Read<double>(p,prefix+"weights");
    Require(weights.empty()||weights.size()==count,prefix+"weights");
    for (double w:weights) Require(std::isfinite(w)&&w>0,prefix+"weights");
}
template<class T> void Curves(const UsdPrim& p,const std::string& prefix,size_t count,bool sparse=false) {
    if (!count) return;
    const auto orders=Sized<unsigned int>(p,prefix+"order",count);
    const auto counts=Sized<unsigned int>(p,prefix+"vertexCount",count);
    for (size_t i=0;i<count;++i)
        Require((sparse&&orders[i]==0&&counts[i]==0)||(orders[i]>=2&&counts[i]>=orders[i]),prefix+"order");
    Coordinates<T>(p,prefix+"controlVertices",Sum(counts));
    Knots(p,prefix+"knots",counts,orders);
    Weights(p,prefix,Sum(counts));
}
void Surfaces(const UsdPrim& p,size_t count) {
    if (!count) return;
    const std::string prefix="brep:surface:nurb:";
    const auto nu=Sized<unsigned int>(p,prefix+"uVertexCount",count);
    const auto nv=Sized<unsigned int>(p,prefix+"vVertexCount",count);
    const auto ou=Sized<unsigned int>(p,prefix+"uOrder",count);
    const auto ov=Sized<unsigned int>(p,prefix+"vOrder",count);
    size_t points=0;
    for (size_t i=0;i<count;++i) {
        Require(ou[i]>=2&&ov[i]>=2&&nu[i]>=ou[i]&&nv[i]>=ov[i],prefix+"order");
        points+=size_t(nu[i])*nv[i];
    }
    Coordinates<GfVec3d>(p,prefix+"controlVertices",points);
    Knots(p,prefix+"uKnots",nu,ou); Knots(p,prefix+"vKnots",nv,ov);
    Weights(p,prefix,points);
}
size_t Count(const VtArray<TfToken>& types,const std::string& token) {
    return std::count(types.begin(),types.end(),TfToken(token));
}
void Analytic(const UsdPrim& p,const std::string& prefix,size_t count,
              const std::vector<std::string>& vectors,const std::vector<std::string>& scalars) {
    if (!count) return;
    for (const auto& name:vectors) Coordinates<GfVec3d>(p,prefix+name,count);
    for (const auto& name:scalars) Coordinates<double>(p,prefix+name,count);
}
}
void UsdSolidOcctValidateInput(const UsdSolidBrepArray& brep) {
    const auto p=brep.GetPrim();
    const auto faces=Read<TfToken>(p,"face:surfaceType");
    Require(!faces.empty(),"face:surfaceType");
    const auto loops=Sized<unsigned int>(p,"face:loopCount",faces.size());
    Sized<TfToken>(p,"face:trimType",faces.size());
    Coordinates<GfVec2d>(p,"face:range",2*faces.size());
    const auto uses=Sized<unsigned int>(p,"loop:edgeuseCount",Sum(loops));
    const auto loopVertices=Sized<unsigned int>(p,"loop:vertexIndex",uses.size());
    const auto indices=Sized<unsigned int>(p,"edgeuse:edgeIndex",Sum(uses));
    Sized<TfToken>(p,"edgeuse:orientationType",indices.size());
    Sized<TfToken>(p,"edgeuse:thisRadialEntryType",indices.size());
    for (auto i:Sized<unsigned int>(p,"edgeuse:nextRadialEUIndex",indices.size()))
        Require(i<indices.size(),"edgeuse:nextRadialEUIndex");
    const auto edges=Read<TfToken>(p,"edge:curveType");
    Coordinates<double>(p,"edge:range",2*edges.size());
    for (auto i:indices) Require(i<edges.size(),"edgeuse:edgeIndex");
    const auto vertices=Read<TfToken>(p,"vertex:pointType");
    Coordinates<GfVec3d>(p,"brep:vertexPoint:point:position",vertices.size());
    for (const auto& pair:Sized<GfVec2i>(p,"edge:vertexIndices",edges.size()))
        Require(pair[0]>=0&&pair[1]>=0&&size_t(pair[0])<vertices.size()&&size_t(pair[1])<vertices.size(),"edge:vertexIndices");
    for (size_t i=0;i<uses.size();++i)
        if (!uses[i]) Require(loopVertices[i]<vertices.size(),"loop:vertexIndex");
    for (auto count:Read<unsigned int>(p,"shell:wireEdgeCount"))
        Require(count==0,"shell:wireEdgeCount (free wires are not supported)");
    size_t surfaceCount=Count(faces,"BrepSurfaceNurbAPI");
    Surfaces(p,surfaceCount);
    for (const auto& kind:std::vector<std::string>{"Plane","Cylinder","Cone","Sphere","Torus"}) {
        size_t count=Count(faces,"BrepSurface"+kind+"API"); surfaceCount+=count;
        auto lower=kind; lower[0]=std::tolower(lower[0]);
        std::vector<std::string> scalars;
        if (kind=="Cylinder"||kind=="Cone"||kind=="Sphere") scalars.push_back("radius");
        if (kind=="Cone") scalars.push_back("semiAngle");
        if (kind=="Torus") scalars={"majorRadius","minorRadius"};
        Analytic(p,"brep:surface:"+lower+":",count,{kind=="Sphere"?"center":"origin","axis","refDirection"},scalars);
    }
    Require(surfaceCount==faces.size(),"face:surfaceType");
    size_t edgeCount=Count(edges,"BrepCurve3dNurbAPI");
    Curves<GfVec3d>(p,"brep:edge3dNurb:curve3d:nurb:",edgeCount);
    for (const auto& kind:std::vector<std::string>{"Line","Circle","Ellipse"}) {
        size_t count=Count(edges,"BrepCurve3d"+kind+"API"); edgeCount+=count;
        auto lower=kind; lower[0]=std::tolower(lower[0]);
        const std::vector<std::string> vectors=kind=="Line"?std::vector<std::string>{"origin","direction"}:
            std::vector<std::string>{"center","axis","refDirection"};
        const std::vector<std::string> scalars=kind=="Circle"?std::vector<std::string>{"radius"}:
            kind=="Ellipse"?std::vector<std::string>{"xRadius","yRadius"}:std::vector<std::string>{};
        Analytic(p,"brep:edge3d"+kind+":curve3d:"+lower+":",count,vectors,scalars);
    }
    Require(edgeCount==edges.size(),"edge:curveType");
    if (!Read<unsigned int>(p,"brep:curveUv:nurb:order").empty())
        Curves<GfVec2d>(p,"brep:curveUv:nurb:",indices.size(),true);
}
PXR_NAMESPACE_CLOSE_SCOPE
