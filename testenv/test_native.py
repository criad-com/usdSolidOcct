from pathlib import Path
import pytest
import subprocess
import sys
from check_support import ROOT, clean_env, native_script, runtime_paths, native_primitives, assert_primitive


def test_installed_cube_bridge():
    paths = runtime_paths()
    assert "passed" in native_script(paths, "testenv/smoke.py", Path(paths["fixtures"]) / "testCube.usda")


def test_api_contracts():
    paths = runtime_paths()
    assert "passed" in native_script(paths, "testenv/contracts.py", paths["fixtures"])


@pytest.fixture(scope="module")
def primitives(tmp_path_factory):
    return {row["name"]: row for row in native_primitives(runtime_paths(), tmp_path_factory.mktemp("primitives"))}


@pytest.mark.parametrize("name", ["cube", "cylinder", "cone", "sphere", "torus", "holed_plate", "filleted_cube", "nurbs_cylinder", "nurbs_holed_plate", "elliptical_prism", "hollow_box", "two_boxes"])
def test_native_primitive_roundtrip(primitives, name):
    assert_primitive(primitives[name])


def test_plugin_free_mesh_twin(tmp_path):
    paths = runtime_paths()
    output = tmp_path / "roundtrip.usda"
    native_script(paths, "examples/roundtrip.py", Path(paths["fixtures"]) / "testCube.usda", output)
    result = subprocess.run([sys.executable, str(ROOT / "tools/vanilla_probe.py"), str(output)],
                            env=clean_env(), capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
