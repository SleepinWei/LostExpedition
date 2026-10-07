"""Import Diesel and create Manny -> Diesel runtime IK retarget assets (UE 5.8)."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());assets=unreal.AssetToolsHelpers.get_asset_tools()
path='/Game/Explorer/Character/Explorer/SkeletalMeshes/SK_Diesel'
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    task=unreal.AssetImportTask();task.filename=str(root/'ArtSource/Characters/Explorer.glb');task.destination_path='/Game/Explorer/Character';task.automated=True;task.save=True
    assets.import_asset_tasks([task])
meshes=[unreal.load_asset(p) for p in unreal.EditorAssetLibrary.list_assets('/Game/Explorer/Character',recursive=True) if unreal.EditorAssetLibrary.find_asset_data(p).asset_class_path.asset_name=='SkeletalMesh']
assert len(meshes)==1,repr(meshes)
mesh=meshes[0]
def asset(name,cls,factory):
    return unreal.load_asset('/Game/Explorer/'+name) or assets.create_asset(name,'/Game/Explorer',cls,factory())
source=unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
rigs=[]
for name,skin in [('IK_ExplorerSource',source),('IK_Diesel',mesh)]:
    rig=asset(name,unreal.IKRigDefinition,unreal.IKRigDefinitionFactory);c=unreal.IKRigController.get_controller(rig);c.set_skeletal_mesh(skin)
    assert c.apply_auto_generated_retarget_definition(),name+' auto characterization failed'
    unreal.log('EXPLORER_RIG '+name+' root='+str(c.get_retarget_root())+' chains='+repr([(str(x.chain_name),str(c.get_retarget_chain_start_bone(x.chain_name)),str(c.get_retarget_chain_end_bone(x.chain_name))) for x in c.get_retarget_chains()]))
    rigs.append(rig);unreal.EditorAssetLibrary.save_loaded_asset(rig)
rtg=asset('RTG_Explorer_Diesel',unreal.IKRetargeter,unreal.IKRetargetFactory)
c=unreal.IKRetargeterController.get_controller(rtg)
c.remove_all_ops()
c.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE,rigs[0]);c.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET,rigs[1]);c.add_default_ops();c.auto_map_chains(unreal.AutoMapChainType.EXACT,True)
c.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET,unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
# This Mixamo rig has a pelvis root, not an independent motion root. Root remapping
# would overwrite its pelvis with the source's ground-level root and stretch skin.
# The pelvis op carries source pose offsets; gameplay owns all capsule movement.
for index in range(c.get_num_retarget_ops()):
    op=c.get_op_controller(index)
    if isinstance(op,unreal.IKRetargetRootMotionController):
        unreal.log('EXPLORER_DISABLE_ROOT_REMAP root='+str(op.get_target_root_bone())+' pelvis='+str(op.get_target_pelvis_bone()))
        c.set_retarget_op_enabled(index,False)
unreal.EditorAssetLibrary.save_loaded_asset(rtg)
(root/'Docs/explorer-character-setup.txt').write_text('Explorer character: '+mesh.get_path_name()+'\nRuntime IK retargeter: '+rtg.get_path_name()+'\nEXPLORER_SETUP_COMPLETE\n')
unreal.log('EXPLORER_SETUP_COMPLETE '+mesh.get_path_name())
