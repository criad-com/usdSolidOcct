// SPDX-License-Identifier: MIT
#include "usdSolidOcct/bridge.h"
#include "pxr/usd/usd/stage.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <cmath>
#include <iostream>
#include <stdexcept>
PXR_NAMESPACE_USING_DIRECTIVE
void require(bool value) { if (!value) throw std::runtime_error("Acceptance failed"); }
int main(int argc, char** argv) {
    auto stage = UsdStage::Open(argv[1]);
    auto shape = UsdSolidOcctBuild(UsdSolidBrepArray(stage->GetPrimAtPath(SdfPath("/World/Cube"))));
    require(UsdSolidOcctFaceCount(shape) == 6);
    require(std::abs(UsdSolidOcctVolume(shape) - 1000) < 1e-9);
    require(UsdSolidOcctTessellate(shape).points.size() == 24);
    auto output=UsdStage::CreateInMemory();
    auto brep=UsdSolidBrepArray::Define(output,SdfPath("/Body"));
    UsdSolidOcctWrite(shape,brep);
    auto roundtrip=UsdSolidOcctBuild(brep);
    require(UsdSolidOcctFaceCount(roundtrip)==6);
    require(std::abs(UsdSolidOcctVolume(roundtrip)-1000)<1e-9);
    auto a = BRepPrimAPI_MakeBox(2, 2, 2).Shape();
    auto b = BRepPrimAPI_MakeBox(gp_Pnt(1, 0, 0), 2, 2, 2).Shape();
    auto c = BRepPrimAPI_MakeBox(gp_Pnt(5, 0, 0), 2, 2, 2).Shape();
    require(std::abs(UsdSolidOcctVolume(UsdSolidOcctCommon(a, b)) - 4) < 1e-12);
    require(std::abs(UsdSolidOcctDistance(a, c) - 3) < 1e-12);
    require(std::abs(UsdSolidOcctArea(a) - 24) < 1e-12);
    auto empty=UsdSolidOcctCommon(a,c);
    require(UsdSolidOcctFaceCount(empty)==0 && UsdSolidOcctVolume(empty)==0);
    std::cout << "cube: 6 faces, volume 1000, 24 vertices; distance 3; common 4\n";
}
