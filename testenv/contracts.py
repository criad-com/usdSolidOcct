"""Invalid packed data must raise, and replacing geometry must preserve identity."""
from pathlib import Path
import sys
from pxr import Gf, Sdf, Usd, UsdSolid, UsdSolidOcct, Vt


def raises(error, function, *args):
    try:
        function(*args)
    except error:
        return
    raise AssertionError(f"Expected {error.__name__}")


def multi_brep(shape):
    stage = Usd.Stage.CreateInMemory()
    first = UsdSolid.BrepArray.Define(stage, "/First")
    UsdSolidOcct.Write(shape, first)
    source = first.GetPrim()
    both = UsdSolid.BrepArray.Define(stage, "/Both")
    prim = both.GetPrim()
    for api in source.GetAppliedSchemas():
        prim.AddAppliedSchema(api)
    vertex_count = len(source.GetAttribute("vertex:pointType").Get())
    index_offsets = {
        "faceuse:faceIndex": len(source.GetAttribute("face:surfaceType").Get()),
        "edgeuse:edgeIndex": len(source.GetAttribute("edge:curveType").Get()),
        "edgeuse:nextRadialEUIndex": len(source.GetAttribute("edgeuse:edgeIndex").Get()),
        "loop:vertexIndex": vertex_count,
    }
    for attr in source.GetAuthoredAttributes():
        name, values = attr.GetName(), list(attr.Get())
        if name == "extent":
            continue
        second = values[:]
        if name in index_offsets:
            second = [x + index_offsets[name] for x in values]
        elif name == "edge:vertexIndices":
            second = [x + Gf.Vec2i(vertex_count) for x in values]
        elif name.endswith(":controlVertices") and attr.GetTypeName() == Sdf.ValueTypeNames.Point3dArray:
            second = [x + Gf.Vec3d(20, 0, 0) for x in values]
        elif name in ("brep:extent", "brep:vertexPoint:point:position"):
            second = [x + Gf.Vec3d(20, 0, 0) for x in values]
        prim.CreateAttribute(name, attr.GetTypeName(), False, attr.GetVariability()).Set(values + second)
    a, b = UsdSolidOcct.Build(both, 0), UsdSolidOcct.Build(both, 1)
    assert UsdSolidOcct.FaceCount(a) == UsdSolidOcct.FaceCount(b) == 6
    assert abs(UsdSolidOcct.Volume(b) - 1000) < 1e-9
    assert abs(UsdSolidOcct.Distance(a, b) - 10) < 1e-9
    # Fail loudly if validator discovery stops seeing the native plugin.
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
    from probe import context, findings
    assert not findings(context(), stage)


def main(fixtures):
    source = Usd.Stage.Open(str(Path(fixtures) / "testCube.usda"))
    shape = UsdSolidOcct.Build(UsdSolid.BrepArray(source.GetPrimAtPath("/World/Cube")))
    multi_brep(shape)
    output = Usd.Stage.CreateInMemory()
    target = UsdSolid.BrepArray.Define(output, "/Body")
    prim = target.GetPrim()
    prim.CreateAttribute("example:identity", Sdf.ValueTypeNames.String).Set("body-01")
    # Simulate obsolete geometry from a previous analytic export.
    prim.AddAppliedSchema("BrepSurfaceSphereAPI")
    prim.CreateAttribute("brep:surface:sphere:radius", Sdf.ValueTypeNames.DoubleArray).Set([3.0])
    UsdSolidOcct.Write(shape, target)
    assert prim.GetAttribute("example:identity").Get() == "body-01"
    assert "BrepSurfaceSphereAPI" not in prim.GetAppliedSchemas()
    assert prim.GetAttribute("brep:surface:sphere:radius").Get() is None
    assert abs(UsdSolidOcct.Volume(UsdSolidOcct.Build(target)) - 1000) < 1e-9
    assert UsdSolidOcct.Distance(shape, shape) == 0
    assert abs(UsdSolidOcct.Volume(UsdSolidOcct.Common(shape, shape)) - 1000) < 1e-9
    cases = [
        ("edgeuse:edgeIndex", Vt.UIntArray([999999])),
        ("edge:vertexIndices", Vt.Vec2iArray([Gf.Vec2i(-1, 0)])),
        ("brep:surface:nurb:controlVertices", Vt.Vec3dArray()),
        ("brep:curveUv:nurb:order", Vt.UIntArray([2])),
        ("region:shellCount", Vt.UIntArray([999999])),
        ("brep:intersectTol3d", Vt.DoubleArray([-1.0])),
    ]
    for name, bad in cases:
        attr = prim.GetAttribute(name)
        original = attr.Get()
        attr.Set(bad)
        raises(ValueError, UsdSolidOcct.Build, target)
        if original is None:
            attr.Clear()
        else:
            attr.Set(original)
    # Also exercise a correctly sized array with an invalid reference.
    attr = prim.GetAttribute("edgeuse:edgeIndex")
    indices = attr.Get()
    indices[0] = 999999
    attr.Set(indices)
    raises(ValueError, UsdSolidOcct.Build, target)
    for linear, angular in ((0, .5), (-1, .5), (.1, 0), (float("nan"), .5)):
        raises(ValueError, UsdSolidOcct.Tessellate, shape, linear, angular)
    sheet = Usd.Stage.Open(str(Path(fixtures) / "testPlane.usda"))
    sheet_prim = next(p for p in sheet.Traverse() if p.GetTypeName() == "BrepArray")
    plane = UsdSolidOcct.Build(UsdSolid.BrepArray(sheet_prim))
    assert UsdSolidOcct.Area(plane) > 0
    raises(ValueError, UsdSolidOcct.Volume, plane)
    print("API contracts passed: replacement, seven malformed arrays, four deflections, sheet volume")


if __name__ == "__main__":
    main(sys.argv[1])
