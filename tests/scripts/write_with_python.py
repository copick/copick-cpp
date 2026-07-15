#!/usr/bin/env python
"""Write a copick project with the reference Python copick, for the reverse parity check
(read back by copick-cpp's examples/read_project).

Usage:
    python write_with_python.py <project_dir>
"""
import json
import os
import sys

import numpy as np

import copick


def main(project_dir: str) -> int:
    os.makedirs(project_dir, exist_ok=True)
    proj = os.path.join(project_dir, "project")
    config_path = os.path.join(project_dir, "config.json")

    config = {
        "name": "py",
        "config_type": "filesystem",
        "user_id": "py",
        "pickable_objects": [
            {"name": "proteasome", "is_particle": True, "label": 1, "color": [255, 0, 0, 255]},
        ],
        "overlay_root": f"local://{proj}",
        "overlay_fs_args": {"auto_mkdir": True},
    }
    with open(config_path, "w") as f:
        json.dump(config, f)

    root = copick.from_file(config_path)
    run = root.new_run("TS_PY")

    tomo = run.new_voxel_spacing(10.0).new_tomogram("wbp")
    tomo.from_numpy(np.arange(64, dtype=np.float32).reshape(4, 4, 4))

    seg = run.new_segmentation(
        voxel_size=10.0, name="proteasome", session_id="1", is_multilabel=False, user_id="py"
    )
    seg.from_numpy((np.arange(64, dtype=np.uint8) % 2).reshape(4, 4, 4))

    from copick.models import CopickLocation, CopickPoint

    picks = run.new_picks(object_name="proteasome", session_id="0", user_id="py")
    picks.points = [
        CopickPoint(location=CopickLocation(x=1.0, y=2.0, z=3.0)),
        CopickPoint(location=CopickLocation(x=11.0, y=12.0, z=13.0)),
    ]
    picks.store()

    print(f"Python copick {copick.__version__} wrote project to {project_dir}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: write_with_python.py <project_dir>", file=sys.stderr)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
