// SPDX-License-Identifier: MIT
#include "usdSolidOcct/bridge.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/metrics.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepBuilderAPI_NurbsConvert.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Geom_Ellipse.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <ShapeUpgrade_ShapeDivideClosed.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <Standard_Failure.hxx>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <map>
PXR_NAMESPACE_USING_DIRECTIVE
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path root(argv[1]);
    std::filesystem::create_directories(root);
    std::map<std::string,TopoDS_Shape> shapes;
    shapes["cube"]=BRepPrimAPI_MakeBox(10,10,10).Shape();
    shapes["cylinder"]=BRepPrimAPI_MakeCylinder(3,10).Shape();
    shapes["cone"]=BRepPrimAPI_MakeCone(5,0,10).Shape();
    shapes["sphere"]=BRepPrimAPI_MakeSphere(5).Shape();
    shapes["torus"]=BRepPrimAPI_MakeTorus(5,1).Shape();
    shapes["holed_plate"]=BRepAlgoAPI_Cut(BRepPrimAPI_MakeBox(10,10,1).Shape(),
        BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(5,5,-1),gp_Dir(0,0,1)),2,3).Shape()).Shape();
    auto box=BRepPrimAPI_MakeBox(10,10,10).Shape();
    BRepFilletAPI_MakeFillet fillet(box);
    for (TopExp_Explorer it(box,TopAbs_EDGE);it.More();it.Next()) fillet.Add(1,TopoDS::Edge(it.Current()));
    shapes["filleted_cube"]=fillet.Shape();
    shapes["nurbs_cylinder"]=BRepBuilderAPI_NurbsConvert(shapes["cylinder"]).Shape();
    shapes["nurbs_holed_plate"]=BRepBuilderAPI_NurbsConvert(shapes["holed_plate"]).Shape();
    auto ellipse=new Geom_Ellipse(gp_Ax2(gp_Pnt(0,0,0),gp_Dir(0,0,1)),3,2);
    auto ellipseWire=BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(ellipse).Edge()).Wire();
    shapes["elliptical_prism"]=BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(ellipseWire).Face(),gp_Vec(0,0,5)).Shape();
    shapes["hollow_box"]=BRepAlgoAPI_Cut(shapes["cube"],
        BRepPrimAPI_MakeBox(gp_Pnt(1,1,1),8,8,8).Shape()).Shape();
    TopoDS_Compound boxes;
    BRep_Builder builder; builder.MakeCompound(boxes);
    builder.Add(boxes,BRepPrimAPI_MakeBox(2,2,2).Shape());
    builder.Add(boxes,BRepPrimAPI_MakeBox(gp_Pnt(5,0,0),2,2,2).Shape());
    shapes["two_boxes"]=boxes;
    std::ofstream report(root/"metrics.json");
    report<<std::setprecision(17)<<"[\n";
    bool first=true;
    for (auto& [name,original]:shapes) {
        if (!first) report<<",\n";
        first=false;
        report<<"{\"name\":\""<<name<<"\"";
        try {
            ShapeUpgrade_ShapeDivideClosed split(original); split.Perform();
            const auto source=split.Result();
            const auto sourceFaces=UsdSolidOcctFaceCount(source);
            const double sourceVolume=UsdSolidOcctVolume(source);
            auto stage=UsdStage::CreateNew((root/(name+".usda")).string());
            UsdGeomSetStageMetersPerUnit(stage,1);
            UsdGeomSetStageUpAxis(stage,TfToken("Z"));
            auto brep=UsdSolidBrepArray::Define(stage,SdfPath("/Body"));
            UsdSolidOcctWrite(source,brep);
            stage->GetRootLayer()->Save();
            auto rebuilt=UsdSolidOcctBuild(brep);
            double volume=UsdSolidOcctVolume(rebuilt);
            auto second=UsdStage::CreateNew((root/(name+"_roundtrip.usda")).string());
            auto next=UsdSolidBrepArray::Define(second,SdfPath("/Body"));
            UsdSolidOcctWrite(rebuilt,next);
            second->GetRootLayer()->Save();
            auto twice=UsdSolidOcctBuild(next);
            report<<",\"sourceFaces\":"<<sourceFaces<<",\"faces\":"<<UsdSolidOcctFaceCount(rebuilt)
                <<",\"inputFaces\":"<<UsdSolidOcctFaceCount(original)
                <<",\"sourceVolume\":"<<sourceVolume<<",\"volume\":"<<volume
                <<",\"roundtripFaces\":"<<UsdSolidOcctFaceCount(twice)
                <<",\"roundtripVolume\":"<<UsdSolidOcctVolume(twice)
                <<",\"roundtripValid\":"<<(BRepCheck_Analyzer(twice).IsValid()?"true":"false")
                <<",\"valid\":"<<(BRepCheck_Analyzer(rebuilt).IsValid()?"true":"false");
        } catch(const Standard_Failure& e) {
            std::cerr<<name<<": "<<e.GetMessageString()<<"\n";
            report<<",\"error\":true";
        } catch(const std::exception& e) {
            std::cerr<<name<<": "<<e.what()<<"\n";
            report<<",\"error\":true";
        }
        report<<"}";
    }
    report<<"\n]\n";
}
