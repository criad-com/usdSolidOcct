// SPDX-License-Identifier: MIT
#include "bridge.h"
#include "pxr/base/tf/pyModule.h"
#include "pxr/external/boost/python.hpp"
#include <Standard_Failure.hxx>
#include <stdexcept>
PXR_NAMESPACE_USING_DIRECTIVE
using namespace pxr_boost::python;
namespace {
void OcctError(const Standard_Failure& e) { PyErr_SetString(PyExc_RuntimeError, e.GetMessageString()); }
void ValueError(const std::invalid_argument& e) { PyErr_SetString(PyExc_ValueError, e.what()); }
void IndexError(const std::out_of_range& e) { PyErr_SetString(PyExc_IndexError, e.what()); }
}
TF_WRAP_MODULE {
    register_exception_translator<Standard_Failure>(&OcctError);
    register_exception_translator<std::invalid_argument>(&ValueError);
    register_exception_translator<std::out_of_range>(&IndexError);
    class_<TopoDS_Shape>("Shape", no_init).def("IsNull", &TopoDS_Shape::IsNull);
    class_<UsdSolidOcctMesh>("Mesh", no_init)
        .def_readonly("points", &UsdSolidOcctMesh::points)
        .def_readonly("faceVertexCounts", &UsdSolidOcctMesh::faceVertexCounts)
        .def_readonly("faceVertexIndices", &UsdSolidOcctMesh::faceVertexIndices)
        .def_readonly("normals", &UsdSolidOcctMesh::normals)
        .def_readonly("sourceFaceIndices", &UsdSolidOcctMesh::sourceFaceIndices);
    def("Build", &UsdSolidOcctBuild, (arg("brep"), arg("brepIndex")=0, arg("sew")=true));
    def("Write", &UsdSolidOcctWrite);
    def("Tessellate", &UsdSolidOcctTessellate,
        (arg("shape"), arg("linearDeflection")=0.1, arg("angularDeflection")=0.5));
    def("Volume", &UsdSolidOcctVolume);
    def("Area", &UsdSolidOcctArea);
    def("Distance", &UsdSolidOcctDistance);
    def("Common", &UsdSolidOcctCommon);
    def("FaceCount", &UsdSolidOcctFaceCount);
    def("SolidCount", &UsdSolidOcctSolidCount);
    def("IsValid", &UsdSolidOcctIsValid);
}
