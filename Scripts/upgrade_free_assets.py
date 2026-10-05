"""Apply locally available free assets to the existing adventure level."""
import unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir())
material_script=root/'Scripts/create_coastal_materials.py'
exec(compile(material_script.read_text(),str(material_script),'exec'))
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels.load_level('/Game/Maps/CliffSanctuary')
count=0
for actor in actors.get_all_level_actors():
    if actor.get_class().get_name()=='ExpeditionWorld':
        actor.rebuild_scene();count+=1
    if isinstance(actor,unreal.PostProcessVolume):
        settings=actor.get_editor_property('settings')
        settings.set_editor_property('auto_exposure_min_brightness',12.8)
        settings.set_editor_property('auto_exposure_max_brightness',12.8)
        actor.set_editor_property('settings',settings)
if count!=1: raise RuntimeError('Expected one authored environment actor')
# Assert the official resource dependencies are available, rather than silently using placeholders.
for path in ['/Game/Weapons/Pistol/Meshes/SM_Pistol','/Game/Weapons/Rifle/Meshes/SM_Rifle',
             '/Game/ArchVis/SampleScene/Tree/HillTree_02','/Game/Nature/SM_TownTree','/Game/Nature/SM_TownGrass','/Game/Coastal/Meshes/SM_coastal_cliff_02',
             '/Game/Coastal/Meshes/SM_fern_02','/Game/Coastal/Meshes/SM_CliffCore']:
    if not unreal.load_asset(path): raise RuntimeError('Missing free asset: '+path)
levels.save_current_level()
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
unreal.log('FREE_ASSET_UPGRADE_SUCCESS: official UE weapons/tree and CC0 foliage/PBR materials saved')
