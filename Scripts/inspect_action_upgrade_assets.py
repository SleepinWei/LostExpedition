"""Inspect local action clips in Unreal before selecting playback and blending rules."""
import json
from pathlib import Path
import unreal

root='/Game/Characters/Mannequins/Anims'
paths=unreal.EditorAssetLibrary.list_assets(root, recursive=True, include_folder=False)
report=[]
for path in paths:
    if not any(token in path for token in ['Reload','Equip','/Jump/','/Jog/','/Walk/','BS_Idle']):
        continue
    asset=unreal.load_asset(path)
    entry={'path':path,'class':asset.get_class().get_name() if asset else None}
    if asset:
        for prop in ['sequence_length','additive_anim_type','ref_pose_type','ref_pose_seq','ref_frame_index','enable_root_motion','skeleton']:
            try:entry[prop]=str(asset.get_editor_property(prop))
            except Exception:pass
    report.append(entry)
destination=Path(unreal.Paths.project_dir())/'Docs/action-assets-inspection.json'
destination.write_text(json.dumps(report,indent=2))
unreal.log('ACTION_ASSET_INSPECTION_COMPLETE '+str(len(report)))
