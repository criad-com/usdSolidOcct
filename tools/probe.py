"""Measure native exports and upstream round trips using the real validators."""
import ast
import hashlib
import json
from pathlib import Path
import sys
from pxr import Usd, UsdSolid, UsdSolidOcct, UsdValidation


def context():
    registry = UsdValidation.ValidationRegistry()
    metadata = registry.GetValidatorMetadataForKeyword("UsdSolidValidators")
    validators = [registry.GetOrLoadValidatorByName(m.name) for m in metadata]
    assert len(validators) == 20 and all(validators), "All 20 usdSolid validators must load"
    return UsdValidation.ValidationContext(validators)


def findings(ctx, stage):
    return [{"severity": "error" if e.GetType() == UsdValidation.ValidationErrorType.Error else "warning",
             "identifier": e.GetIdentifier(), "message": e.GetMessage()}
            for e in ctx.Validate(stage)]


def primitives(directory):
    root = Path(directory)
    ctx = context()
    rows = json.loads((root / "metrics.json").read_text())
    for row in rows:
        for suffix, key in (("", "findings"), ("_roundtrip", "roundtripFindings")):
            path = root / (row["name"] + suffix + ".usda")
            if path.exists():
                row[key] = findings(ctx, Usd.Stage.Open(str(path)))
    return rows


def corpus(paths, directory, names):
    root = Path(directory)
    root.mkdir(parents=True, exist_ok=True)
    ctx = context()
    tree = ast.parse((Path(paths["upstream"]) / "pxr/imaging/plugin/hdOcct/testenv/testUsdSolidTessellation.py").read_text())
    baselines = next(ast.literal_eval(node.value) for node in tree.body if isinstance(node, ast.Assign)
                     and any(isinstance(t, ast.Name) and t.id == "EXPECTED_VERT_COUNTS" for t in node.targets))
    rows = []
    for name in names:
        stage = Usd.Stage.Open(str(Path(paths["fixtures"]) / name))
        for prim in stage.Traverse():
            if prim.GetTypeName() != "BrepArray":
                continue
            brep = UsdSolid.BrepArray(prim)
            counts = brep.GetBrepRegionCountAttr().Get()
            if counts is None:
                rows.append({"file": name, "prim": str(prim.GetPath()), "index": 0,
                             "error": "Missing brep:regionCount"})
                continue
            for index in range(len(counts)):
                row = {"file": name, "prim": str(prim.GetPath()), "index": index}
                rows.append(row)
                print(f"== stage: fixture {name} {prim.GetName()} {index}", file=sys.stderr, flush=True)
                try:
                    raw = UsdSolidOcct.Build(brep, index, False)
                    row["vertices"] = len(UsdSolidOcct.Tessellate(raw).points)
                    row["baseline"] = baselines.get(name)
                    source = UsdSolidOcct.Build(brep, index)
                    row["faces"] = UsdSolidOcct.FaceCount(source)
                    row["authoredFaces"] = len(brep.GetFaceLoopCountAttr().Get())
                    row["solids"] = UsdSolidOcct.SolidCount(source)
                    row["valid"] = UsdSolidOcct.IsValid(source)
                    row["volume"] = UsdSolidOcct.Volume(source) if row["solids"] else None
                    row["area"] = UsdSolidOcct.Area(source)
                    output = Usd.Stage.CreateInMemory()
                    target = UsdSolid.BrepArray.Define(output, "/Body")
                    UsdSolidOcct.Write(source, target)
                    key=hashlib.sha256(str(prim.GetPath()).encode()).hexdigest()[:12]
                    output.GetRootLayer().Export(str(root / f"{Path(name).stem}_{key}_{index}.usda"))
                    row["findings"] = findings(ctx, output)
                    rebuilt = UsdSolidOcct.Build(target)
                    row["roundtripFaces"] = UsdSolidOcct.FaceCount(rebuilt)
                    row["roundtripSolids"] = UsdSolidOcct.SolidCount(rebuilt)
                    row["roundtripValid"] = UsdSolidOcct.IsValid(rebuilt)
                    row["roundtripVolume"] = UsdSolidOcct.Volume(rebuilt) if row["roundtripSolids"] else None
                    row["roundtripArea"] = UsdSolidOcct.Area(rebuilt)
                except Exception as e:
                    row["error"] = str(e)
    return rows


if __name__ == "__main__":
    mode, runtime, directory, *names = sys.argv[1:]
    paths = json.loads(Path(runtime).read_text())
    result = primitives(directory) if mode == "primitives" else corpus(paths, directory, names)
    print(json.dumps(result, indent=2))
