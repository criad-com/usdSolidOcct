// SPDX-License-Identifier: MIT
// Bounded diagnostic for a Brep whose reconstructed shell fails to sew closed.
#include "usdSolidOcct/bridge.h"
#include "pxr/usd/usd/stage.h"
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepLib.hxx>
#include <BRep_Tool.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopExp_Explorer.hxx>
#include <Standard_Failure.hxx>
#include <iomanip>
#include <iostream>
PXR_NAMESPACE_USING_DIRECTIVE
int main(int argc,char** argv) {
    if (argc!=3) return 2;
    auto stage=UsdStage::Open(argv[1]);
    if (!stage) { std::cerr<<"Cannot open stage\n"; return 1; }
    auto brep=UsdSolidBrepArray(stage->GetPrimAtPath(SdfPath(argv[2])));
    auto raw=UsdSolidOcctBuild(brep,0,false);
    VtArray<double> t; brep.GetBrepIntersectTol3dAttr().Get(&t);
    int invalid=0;
    for (TopExp_Explorer it(raw,TopAbs_FACE);it.More();it.Next())
        invalid+=!BRepCheck_Analyzer(it.Current()).IsValid();
    std::cout<<"faces="<<UsdSolidOcctFaceCount(raw)<<" invalidFaces="<<invalid<<"\n"<<std::setprecision(17);
    for (double multiplier:{1.,2.,5.,10.,20.}) {
        BRepBuilderAPI_Sewing sew(t[0]*multiplier);
        sew.Add(raw); sew.Perform();
        auto sewn=sew.SewedShape();
        if (multiplier==1) for (int i=1;i<=sew.NbFreeEdges();++i) {
            auto edge=sew.FreeEdge(i);
            GProp_GProps props; BRepGProp::LinearProperties(edge,props);
            auto point=props.CentreOfMass();
            std::cout<<"freeEdge length="<<props.Mass()<<" center="<<point.X()<<","<<point.Y()<<","<<point.Z()<<" faces=";
            int fi=0;
            for (TopExp_Explorer f(raw,TopAbs_FACE);f.More();f.Next(),++fi) {
                auto face=sew.IsModified(f.Current())?sew.Modified(f.Current()):f.Current();
                for (TopExp_Explorer e(face,TopAbs_EDGE);e.More();e.Next())
                    if (e.Current().IsSame(edge)) std::cout<<fi<<",";
            }
            std::cout<<"\n";
        }
        std::cout<<"tolerance="<<t[0]*multiplier<<" freeEdges="<<sew.NbFreeEdges()
            <<" multipleEdges="<<sew.NbMultipleEdges()<<" valid="<<UsdSolidOcctIsValid(sewn);
        int shells=0,closed=0;
        for (TopExp_Explorer it(sewn,TopAbs_SHELL);it.More();it.Next()) {
            ++shells; closed+=BRep_Tool::IsClosed(it.Current());
            if (BRep_Tool::IsClosed(it.Current())) {
                auto solid=BRepBuilderAPI_MakeSolid(TopoDS::Shell(it.Current())).Solid();
                BRepLib::OrientClosedSolid(solid);
                std::cout<<" volume="<<UsdSolidOcctVolume(solid);
                try {
                    auto out=UsdStage::CreateInMemory();
                    auto target=UsdSolidBrepArray::Define(out,SdfPath("/Body"));
                    UsdSolidOcctWrite(solid,target);
                    auto rebuilt=UsdSolidOcctBuild(target);
                    std::cout<<" roundtripVolume="<<UsdSolidOcctVolume(rebuilt);
                } catch (const Standard_Failure& e) { std::cout<<" error="<<e.GetMessageString(); }
                  catch (const std::exception& e) { std::cout<<" error="<<e.what(); }
            }
        }
        std::cout<<" shells="<<shells<<" closed="<<closed<<"\n";
    }
}
