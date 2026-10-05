import unreal
from pathlib import Path
assets=unreal.AssetToolsHelpers.get_asset_tools()
mesh=unreal.load_asset('/Game/Coastal/Meshes/SM_CliffCore')
if True:
 task=unreal.AssetImportTask();task.filename=str(Path(unreal.Paths.project_dir())/'ArtSource/CoastalRemake/cliff_core.obj')
 task.destination_path='/Game/Coastal/Meshes';task.destination_name='SM_CliffCore';task.automated=True;task.save=True;task.replace_existing=True
 opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False
 opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
 opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=True
 task.options=opt;task.factory=unreal.FbxFactory();assets.import_asset_tasks([task]);mesh=unreal.load_asset('/Game/Coastal/Meshes/SM_CliffCore')
for i,s in enumerate(mesh.static_materials):
 name=str(s.material_slot_name)
 mesh.set_material(i,unreal.load_asset('/Game/Materials/'+('M_CoastalGroundV2' if name=='Earth' else 'M_ExpeditionRockV2')))
mesh_editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
mesh_editor.remove_collisions(mesh)
mesh_editor.add_simple_collisions(mesh,unreal.ScriptCollisionShapeType.BOX)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
unreal.log('CLIFF_CORE_BOUNDS '+str(mesh.get_bounds()))
