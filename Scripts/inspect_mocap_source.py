"""Inventory source characters/actions in Unreal without changing any assets.

Run in the downloaded sample project, or after migration into LostExpedition:
  -run=pythonscript -script="/absolute/path/inspect_mocap_source.py --root /Game"
Use --mesh /Game/... for detailed bone names after choosing a mesh from the inventory.
The report identifies candidates; names alone do not establish mocap provenance.
"""
import argparse
import json
from pathlib import Path
import re

import unreal


def vector(value):
    return [round(float(value.x), 4), round(float(value.y), 4), round(float(value.z), 4)]


def inspect_sequence(asset):
    library = unreal.AnimationLibrary
    duration = float(asset.get_play_length())
    start = library.extract_root_track_transform(asset, 0.0).translation
    end = library.extract_root_track_transform(asset, duration).translation
    skeleton = asset.get_editor_property("skeleton")
    curves = library.get_animation_curve_names(asset, unreal.RawCurveTrackTypes.RCT_FLOAT)
    return {
        "path": asset.get_path_name(),
        "skeleton": skeleton.get_path_name() if skeleton else None,
        "duration_seconds": duration,
        "frames": library.get_num_frames(asset),
        "root_motion_enabled": bool(asset.get_editor_property("enable_root_motion")),
        "root_start_cm": vector(start),
        "root_end_cm": vector(end),
        "root_displacement_cm": vector(end - start),
        "additive_type": str(asset.get_editor_property("additive_anim_type")),
        "tracks": [str(name) for name in library.get_animation_track_names(asset)],
        "float_curves": [str(name) for name in curves],
    }


def inspect_mesh(path):
    mesh = unreal.load_asset(path)
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise ValueError("Expected a SkeletalMesh: " + path)
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    names = [component.get_bone_name(index) for index in range(component.get_num_bones())]
    skeleton = mesh.get_editor_property("skeleton")
    return {
        "path": mesh.get_path_name(),
        "skeleton": skeleton.get_path_name() if skeleton else None,
        "bones": [{"name": str(name), "parent": str(component.get_parent_bone(name))}
                  for name in names],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", action="append", help="Mounted Unreal content root; repeatable")
    parser.add_argument("--name", default="jump|climb|hang|reach|vault|mantle|fall|land",
                        help="Case-insensitive regex for detailed animation inspection")
    parser.add_argument("--detail-limit", type=int, default=60)
    parser.add_argument("--mesh", action="append", default=[])
    parser.add_argument("--output")
    args = parser.parse_args()
    if args.detail_limit < 0:
        parser.error("--detail-limit must be nonnegative")
    roots = args.root or ["/Game"]
    pattern = re.compile(args.name, re.IGNORECASE)
    project = Path(unreal.Paths.project_dir()).resolve()
    output = Path(args.output).resolve() if args.output else project / "Docs/mocap-source-inspection.json"
    classes = {"AnimSequence", "SkeletalMesh", "Skeleton", "IKRigDefinition", "IKRetargeter"}
    report = {"project": str(project), "roots": roots, "inventory": [],
              "animations": [], "meshes": [], "errors": [],
              "note": "Candidate names do not prove motion capture or suitability for wall leaps."}
    paths = set()
    for root in roots:
        if not unreal.EditorAssetLibrary.does_directory_exist(root):
            report["errors"].append({"path": root, "error": "Content directory does not exist"})
            continue
        paths.update(unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False))
    for path in sorted(paths):
        data = unreal.EditorAssetLibrary.find_asset_data(path)
        kind = str(data.asset_class_path.asset_name)
        if kind not in classes:
            continue
        report["inventory"].append({"path": path, "class": kind})
        if kind != "AnimSequence" or not pattern.search(str(data.asset_name)) or len(report["animations"]) >= args.detail_limit:
            continue
        try:
            report["animations"].append(inspect_sequence(unreal.load_asset(path)))
        except Exception as error:
            report["errors"].append({"path": path, "error": str(error)})
    for path in args.mesh:
        try:
            report["meshes"].append(inspect_mesh(path))
        except Exception as error:
            report["errors"].append({"path": path, "error": str(error)})
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    unreal.log("MOCAP_SOURCE_INSPECTION assets={} animations={} meshes={} errors={} output={}".format(
        len(report["inventory"]), len(report["animations"]), len(report["meshes"]),
        len(report["errors"]), output))
    if report["errors"]:
        raise RuntimeError("Source inspection failed; see errors in " + str(output))


if __name__ == "__main__":
    main()
