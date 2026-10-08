"""Create Manny retarget assets and bake selected Epic mocap into the local project.

Run after migrate_game_animation_sample.py. Re-running refreshes generated
retargeters and preserves valid baked clips. UE 5.8 batch overwrite cannot replace
clips already referenced by native class defaults during a commandlet.
"""
from pathlib import Path
import json
import unreal

assets = unreal.AssetToolsHelpers.get_asset_tools()
editor = unreal.EditorAssetLibrary
root = Path(unreal.Paths.project_dir())

def required(path):
    value = unreal.load_asset(path)
    if not value:
        raise RuntimeError('Missing local asset; run sample migration first: ' + path)
    return value

def generated(name, folder, cls, factory):
    return unreal.load_asset(folder + '/' + name) or assets.create_asset(name, folder, cls, factory())

def rig(name, mesh):
    value = generated(name, '/Game/Explorer', unreal.IKRigDefinition, unreal.IKRigDefinitionFactory)
    controller = unreal.IKRigController.get_controller(value)
    controller.set_skeletal_mesh(mesh)
    assert controller.apply_auto_generated_retarget_definition(), name
    editor.save_loaded_asset(value)
    return value

def retargeter(name, source, target):
    value = generated(name, '/Game/Explorer', unreal.IKRetargeter, unreal.IKRetargetFactory)
    controller = unreal.IKRetargeterController.get_controller(value)
    controller.remove_all_ops()
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source)
    controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target)
    controller.add_default_ops()
    controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    controller.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
    editor.save_loaded_asset(value)
    return value

manny = required('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
uefn = required('/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin')
twin = required('/Game/Characters/Paragon/Heroes/TwinBlast/Meshes/SKM_TwinBlast_ActionHero')
manny_rig = rig('IK_ExplorerSource', manny)
uefn_rig = rig('IK_ExplorerMocapSource', uefn)
twin_rig = rig('IK_ExplorerTwinBlast', twin)
retargeter('RTG_Explorer_TwinBlast', manny_rig, twin_rig)
mocap_retarget = retargeter('RTG_Explorer_Mocap', uefn_rig, manny_rig)
source_root = '/Game/Characters/UEFN_Mannequin/Animations/'
clips = [
    'Jump/M_Neutral_Jump_F_Start_Stand_Lfoot',
    'Traversal/Climb/M_Neutral_Traversal_Climb_Start_2_5_stand_F_Lfoot',
    'Traversal/Climb/M_Neutral_Traversal_Climb_Start_2_5_stand_F_Rfoot',
    'Traversal/Mantle/M_Neutral_Traversal_Mantle_1_0_stand_F_Lfoot',
]
inputs = unreal.IKRetargetBatchOperationInputs()
missing = [name for name in clips if not editor.does_asset_exist('/Game/Animation/Mocap/MC_' + name.rsplit('/', 1)[-1])]
inputs.assets_to_retarget = [editor.find_asset_data(source_root + name) for name in missing]
inputs.source_mesh = uefn
inputs.target_mesh = manny
inputs.ik_retarget_asset = mocap_retarget
inputs.prefix = 'MC_'
inputs.target_path = '/Game/Animation/Mocap'
inputs.use_source_path = False
inputs.include_referenced_assets = False
inputs.overwrite_existing_files = False
if missing:
    unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
report = {'source': 'Epic Game Animation Sample 5.8', 'clips': [], 'character': twin.get_path_name()}
for name in clips:
    path = '/Game/Animation/Mocap/MC_' + name.rsplit('/', 1)[-1]
    clip = required(path)
    assert clip.get_editor_property('skeleton') == manny.get_editor_property('skeleton'), path + ': wrong skeleton'
    assert abs(clip.get_play_length() - required(source_root + name).get_play_length()) < .001, path + ': wrong duration'
    # Keep root motion data for inspection. Runtime evaluates in place while the
    # existing collision-aware CharacterMovement path owns capsule displacement.
    clip.set_editor_property('enable_root_motion', True)
    clip.set_editor_property('force_root_lock', True)
    clip.set_editor_property('root_motion_root_lock', unreal.RootMotionRootLock.REF_POSE)
    editor.save_loaded_asset(clip)
    report['clips'].append({'source': source_root + name, 'retargeted': path,
                            'duration_seconds': clip.get_play_length()})
(root / 'Docs/mocap-setup.json').write_text(json.dumps(report, indent=2) + '\n')
wall_report = unreal.ExpeditionMotionMatchingLibrary.build_wall_climb_content()
assert 'WALL_CLIMB_SETUP_COMPLETE' in wall_report, wall_report
(root / 'Docs/wall-climb-setup.txt').write_text(wall_report)
unreal.log('MOCAP_SETUP_COMPLETE clips=' + str(len(report['clips'])))
