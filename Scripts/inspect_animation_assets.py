"""Unreal Python: inspect the mannequin animation assets used by the prototype."""
import json,unreal
from pathlib import Path
paths=[
 '/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire',
 '/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire_Montage',
 '/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS',
 '/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire',
 '/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS',
]
report=[]
for path in paths:
 asset=unreal.load_asset(path);data={'path':path,'class':asset.get_class().get_name() if asset else None}
 if asset:
  for prop in ['sequence_length','additive_anim_type','ref_pose_type','ref_pose_seq','ref_frame_index','skeleton']:
   try:data[prop]=str(asset.get_editor_property(prop))
   except Exception:pass
 report.append(data)
unreal.log('ANIMATION_ASSET_INSPECTION '+json.dumps(report))
(Path(unreal.Paths.project_dir())/'Docs/animation-inspection.json').write_text(json.dumps(report,indent=2))
