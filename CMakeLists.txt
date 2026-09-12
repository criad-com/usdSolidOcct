cmake_minimum_required(VERSION 3.24)
project(usdSolidOcct VERSION 0.1.4 LANGUAGES CXX)
find_package(pxr CONFIG REQUIRED)
find_package(usdSolid CONFIG REQUIRED)
find_package(OpenCASCADE CONFIG REQUIRED COMPONENTS FoundationClasses ModelingData ModelingAlgorithms)
if(NOT OpenCASCADE_BUILD_SHARED_LIBS)
    message(FATAL_ERROR "OCCT must use dynamic linking")
endif()
include("${AECO_NATIVE_CMAKE}/AecoNative.cmake")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/extract.py"
    "${USDSOLIDOCCT_UPSTREAM}" "${CMAKE_BINARY_DIR}/upstream" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/source_manifest.py"
    "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/source-manifest.json" COMMAND_ERROR_IS_FATAL ANY)
add_library(usdSolidOcct SHARED usdSolidOcct/bridge.cpp usdSolidOcct/write.cpp usdSolidOcct/validate.cpp
    usdSolidOcct/torusPatch.cpp
    "${CMAKE_BINARY_DIR}/upstream/brepBuilder.cpp" "${CMAKE_BINARY_DIR}/upstream/tessellate.cpp")
target_compile_definitions(usdSolidOcct PRIVATE USDSOLIDOCCT_EXPORTS)
target_include_directories(usdSolidOcct PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    "$<INSTALL_INTERFACE:include>" "${OpenCASCADE_INCLUDE_DIR}"
    PRIVATE usdSolidOcct "${CMAKE_BINARY_DIR}/upstream")
target_link_libraries(usdSolidOcct PUBLIC usdSolid::usdSolid TKBRep TKMath TKernel
    TKG2d TKG3d TKGeomBase TKGeomAlgo TKTopAlgo TKPrim TKBO TKBool TKFillet TKShHealing TKMesh)
aeco_native_plugin(NAME usdSolidOcct DEPENDENCIES OpenCASCADE)
install(FILES usdSolidOcct/api.h usdSolidOcct/bridge.h DESTINATION include/usdSolidOcct)
install(FILES LICENSE DESTINATION share/licenses/usdSolidOcct)
install(FILES "${USDSOLIDOCCT_UPSTREAM}/LICENSE.txt" DESTINATION share/licenses/usdSolidOcct RENAME UPSTREAM-LICENSE.txt)
install(FILES "${CMAKE_BINARY_DIR}/source-manifest.json" DESTINATION share/usdSolidOcct)
install(FILES "${CMAKE_BINARY_DIR}/Testing/Temporary/LastTest.log" DESTINATION share/usdSolidOcct RENAME ctest.log)

add_library(_usdSolidOcct MODULE usdSolidOcct/module.cpp)
target_compile_features(_usdSolidOcct PRIVATE cxx_std_17)
target_compile_definitions(_usdSolidOcct PRIVATE MFB_PACKAGE_NAME=usdSolidOcct
    MFB_ALT_PACKAGE_NAME=usdSolidOcct MFB_PACKAGE_MODULE=UsdSolidOcct)
target_link_libraries(_usdSolidOcct PRIVATE usdSolidOcct tf Python3::Module)
set_target_properties(_usdSolidOcct PROPERTIES PREFIX "" SUFFIX ".so"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/pxr/UsdSolidOcct")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/python/pxr/UsdSolidOcct")
file(WRITE "${CMAKE_BINARY_DIR}/python/pxr/__init__.py"
    "from pkgutil import extend_path\n__path__ = extend_path(__path__, __name__)\n")
configure_file(usdSolidOcct/__init__.py "${CMAKE_BINARY_DIR}/python/pxr/UsdSolidOcct/__init__.py" COPYONLY)
install(FILES "${CMAKE_BINARY_DIR}/python/pxr/__init__.py" DESTINATION "${AECO_PYTHON_INSTALL_DIR}/pxr")
install(TARGETS _usdSolidOcct LIBRARY DESTINATION "${AECO_PYTHON_INSTALL_DIR}/pxr/UsdSolidOcct")
install(FILES usdSolidOcct/__init__.py DESTINATION "${AECO_PYTHON_INSTALL_DIR}/pxr/UsdSolidOcct")

include(CTest)
add_executable(testUsdSolidOcct testenv/testUsdSolidOcct.cpp)
target_link_libraries(testUsdSolidOcct PRIVATE usdSolidOcct)
add_executable(usdSolidOcctFixtures testenv/fixtures.cpp)
target_link_libraries(usdSolidOcctFixtures PRIVATE usdSolidOcct)
add_executable(usdSolidOcctDiagnose testenv/diagnose.cpp)
target_link_libraries(usdSolidOcctDiagnose PRIVATE usdSolidOcct)
install(TARGETS usdSolidOcctDiagnose RUNTIME DESTINATION libexec/usdSolidOcct)
install(TARGETS usdSolidOcctFixtures testUsdSolidOcct RUNTIME DESTINATION libexec/usdSolidOcct)
add_test(NAME native-smoke COMMAND testUsdSolidOcct "${USDSOLIDOCCT_FIXTURES}/testCube.usda")
add_test(NAME python-smoke COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/testenv/smoke.py"
    "${USDSOLIDOCCT_FIXTURES}/testCube.usda")
add_test(NAME native-primitives COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/testenv/primitives.py"
    "$<TARGET_FILE:usdSolidOcctFixtures>")
add_test(NAME api-contracts COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/testenv/contracts.py"
    "${USDSOLIDOCCT_FIXTURES}")
set_tests_properties(native-smoke python-smoke native-primitives api-contracts PROPERTIES TIMEOUT 180 ENVIRONMENT
    "PXR_PLUGINPATH_NAME=${USDSOLIDOCCT_PLUGINS};PYTHONPATH=${CMAKE_BINARY_DIR}/python:${USDSOLIDOCCT_SCHEMA_PYTHON}:${AECO_USD_PYTHON}")
