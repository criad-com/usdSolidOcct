// SPDX-License-Identifier: MIT
#include <usdSolidOcct/bridge.h>
#include <BRepPrimAPI_MakeBox.hxx>
#include <cmath>
#include <iostream>
PXR_NAMESPACE_USING_DIRECTIVE
int main() {
    auto shape=BRepPrimAPI_MakeBox(2,3,4).Shape();
    if (std::abs(UsdSolidOcctVolume(shape)-24)>1e-12) return 1;
    std::cout<<"Installed CMake consumer: volume 24\n";
}
