"""Run with stock USD, with all external plugin search paths cleared."""
import sys
from pxr import Plug, Usd, UsdGeom
assert not Plug.Registry().GetPluginWithName("usdSolid")
stage = Usd.Stage.Open(sys.argv[1])
assert stage and not stage.GetCompositionErrors()
mesh = UsdGeom.Mesh.Get(stage, "/Model/Mesh")
assert mesh and len(mesh.GetPointsAttr().Get()) > 0
assert len(mesh.GetFaceVertexIndicesAttr().Get()) > 0
assert stage.GetPrimAtPath("/Model/Exact").IsA(UsdGeom.Xform)
print("Plugin-free Mesh twin composes")
