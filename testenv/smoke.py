import sys
from pxr import Usd, UsdSolid, UsdSolidOcct, UsdValidation
stage = Usd.Stage.Open(sys.argv[1])
brep = UsdSolid.BrepArray(stage.GetPrimAtPath("/World/Cube"))
shape = UsdSolidOcct.Build(brep)
assert UsdSolidOcct.FaceCount(shape) == 6
assert abs(UsdSolidOcct.Volume(shape) - 1000) < 1e-9
assert len(UsdSolidOcct.Tessellate(shape).points) == 24
output = Usd.Stage.CreateInMemory()
exported = UsdSolid.BrepArray.Define(output, "/Body")
UsdSolidOcct.Write(shape, exported)
rebuilt = UsdSolidOcct.Build(exported)
assert UsdSolidOcct.FaceCount(rebuilt) == 6
assert abs(UsdSolidOcct.Volume(rebuilt) - 1000) < 1e-9
registry = UsdValidation.ValidationRegistry()
metadata = registry.GetValidatorMetadataForKeyword("UsdSolidValidators")
validators = [registry.GetOrLoadValidatorByName(m.name) for m in metadata]
assert len(validators) == 20 and all(validators)
assert not UsdValidation.ValidationContext(validators).Validate(output)
try:
    UsdSolidOcct.Build(brep, 1)
except IndexError:
    pass
else:
    raise AssertionError("Invalid index was accepted")
print("Python bridge smoke passed")
