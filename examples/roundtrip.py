"""Build a BrepArray and write exact geometry plus a plugin-free Mesh twin."""
import argparse
from pxr import Gf, Sdf, Usd, UsdGeom, UsdSolid, UsdSolidOcct, Vt


def convert(source, destination, prim_path=None, brep_index=0):
    stage = Usd.Stage.Open(source)
    if not stage:
        raise ValueError("Cannot open source stage")
    prim = stage.GetPrimAtPath(prim_path) if prim_path else next(
        (p for p in stage.Traverse() if p.IsA(UsdSolid.BrepArray)), None)
    if not prim:
        raise ValueError("No BrepArray found")
    shape = UsdSolidOcct.Build(UsdSolid.BrepArray(prim), brep_index)
    arrays = UsdSolidOcct.Tessellate(shape, 0.01, 0.2)
    result = Usd.Stage.CreateNew(destination)
    UsdGeom.SetStageMetersPerUnit(result, UsdGeom.GetStageMetersPerUnit(stage))
    UsdGeom.SetStageUpAxis(result, UsdGeom.GetStageUpAxis(stage))
    model = UsdGeom.Xform.Define(result, "/Model")
    result.SetDefaultPrim(model.GetPrim())
    result.SetMetadata("fallbackPrimTypes", {"BrepArray": Vt.TokenArray(["Xform"])})
    exact = UsdSolid.BrepArray.Define(result, "/Model/Exact")
    UsdSolidOcct.Write(shape, exact)
    exact.CreatePurposeAttr(UsdGeom.Tokens.render)
    mesh = UsdGeom.Mesh.Define(result, "/Model/Mesh")
    mesh.CreatePointsAttr(Vt.Vec3fArray([Gf.Vec3f(*p) for p in arrays.points]))
    mesh.CreateFaceVertexCountsAttr(arrays.faceVertexCounts)
    mesh.CreateFaceVertexIndicesAttr(arrays.faceVertexIndices)
    mesh.CreateNormalsAttr(arrays.normals)
    mesh.SetNormalsInterpolation(UsdGeom.Tokens.vertex)
    mesh.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)
    mesh.CreatePurposeAttr(UsdGeom.Tokens.proxy)
    mesh.CreateExtentAttr(UsdGeom.PointBased.ComputeExtent(mesh.GetPointsAttr().Get()))
    exact.CreateProxyPrimRel().SetTargets([mesh.GetPath()])
    mesh.GetPrim().CreateRelationship("sourceBrep", custom=True).SetTargets([exact.GetPath()])
    result.GetRootLayer().Save()
    return len(arrays.points), len(arrays.faceVertexCounts)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source")
    parser.add_argument("destination")
    parser.add_argument("--prim")
    parser.add_argument("--brep-index", type=int, default=0)
    args = parser.parse_args()
    vertices, triangles = convert(args.source, args.destination, args.prim, args.brep_index)
    print(f"Wrote exact body and Mesh twin: {vertices} vertices, {triangles} triangles")
