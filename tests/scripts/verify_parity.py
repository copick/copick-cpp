#!/usr/bin/env python
"""Cross-implementation parity check.

Reads a project written by copick-cpp (examples/write_project) using the reference Python
copick and asserts the data matches — proving the on-disk format is byte-compatible.

Usage:
    python verify_parity.py <project_dir>/config.json
"""
import sys

import numpy as np

import copick


def main(config_path: str) -> int:
    root = copick.from_file(config_path)

    run = root.get_run("TS_001")
    assert run is not None, "run TS_001 not found"

    # Tomogram: 4x4x4 float32 ramp.
    tomo = run.get_voxel_spacing(10.0).get_tomogram("wbp")
    assert tomo is not None, "tomogram wbp not found"
    arr = tomo.numpy()
    assert arr.shape == (4, 4, 4), f"tomogram shape {arr.shape}"
    assert arr[0, 0, 0] == 0.0 and arr[3, 3, 3] == 63.0, "tomogram values differ"

    # Segmentation: 4x4x4 uint8 checkerboard.
    segs = run.get_segmentations(name="proteasome", voxel_size=10.0)
    assert len(segs) == 1, f"expected 1 segmentation, got {len(segs)}"
    seg = segs[0]
    assert not seg.is_multilabel
    mask = seg.numpy()
    assert mask.shape == (4, 4, 4) and mask.dtype == np.uint8, f"seg {mask.shape} {mask.dtype}"
    assert mask.flatten()[1] == 1 and mask.flatten()[0] == 0, "segmentation values differ"

    # Picks: two points.
    picks = run.get_picks(object_name="proteasome", user_id="cpp", session_id="0")
    assert len(picks) == 1, f"expected 1 pick set, got {len(picks)}"
    pts, transforms = picks[0].numpy()
    assert pts.shape == (2, 3), f"picks shape {pts.shape}"
    assert list(pts[0]) == [1.0, 2.0, 3.0], f"pick 0 = {pts[0]}"
    assert list(pts[1]) == [11.0, 12.0, 13.0], f"pick 1 = {pts[1]}"

    # Mesh: a tetrahedron (4 vertices, 4 faces).
    meshes = run.get_meshes(object_name="proteasome")
    assert len(meshes) == 1, f"expected 1 mesh, got {len(meshes)}"
    geom = meshes[0].mesh
    # trimesh may load a Scene or a Trimesh; normalize to a single mesh.
    if hasattr(geom, "geometry"):  # Scene
        tm = list(geom.geometry.values())[0]
    else:
        tm = geom
    assert len(tm.vertices) == 4, f"mesh has {len(tm.vertices)} vertices"
    assert len(tm.faces) == 4, f"mesh has {len(tm.faces)} faces"

    print("PARITY OK — Python copick", copick.__version__, "read the copick-cpp project correctly.")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: verify_parity.py <config.json>", file=sys.stderr)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
