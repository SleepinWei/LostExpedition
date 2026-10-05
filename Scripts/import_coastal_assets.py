"""Import the selected licensed scans; generate reduced LODs for this Mac."""
import unreal, json
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'ArtSource/CoastalRemake'
manifest=json.loads((root/'manifest.json').read_text())
assets=unreal.AssetToolsHelpers.get_asset_tools()
edit=unreal.MaterialEditingLibrary
lib=unreal.EditorAssetLibrary
mesh_editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)

def texture(relative,channel):
    file=root/relative; name='T_'+file.stem; path='/Game/Coastal/Textures/'+name
    tex=unreal.load_asset(path)
    if not tex:
        task=unreal.AssetImportTask();task.filename=str(file);task.destination_path='/Game/Coastal/Textures';task.destination_name=name;task.automated=True;task.save=True
        assets.import_asset_tasks([task]);tex=unreal.load_asset(path)
    if channel=='nor_dx':tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
    if channel!='Diffuse':tex.set_editor_property('srgb',False)
    lib.save_loaded_asset(tex)
    return tex

def material(entry):
    name='M_'+entry['id'];path='/Game/Coastal/Materials/'+name
    mat=unreal.load_asset(path) or assets.create_asset(name,'/Game/Coastal/Materials',unreal.Material,unreal.MaterialFactoryNew())
    edit.delete_all_material_expressions(mat)
    foliage=entry['id']=='fern_02'
    mat.set_editor_property('two_sided',True)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    if foliage:
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    props={'Diffuse':unreal.MaterialProperty.MP_BASE_COLOR,'nor_dx':unreal.MaterialProperty.MP_NORMAL,'Rough':unreal.MaterialProperty.MP_ROUGHNESS,'Alpha':unreal.MaterialProperty.MP_OPACITY_MASK}
    for i,(channel,file) in enumerate(entry['files'].items()):
        sample=edit.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-500,i*180)
        sample.texture=texture(file,channel)
        if channel=='nor_dx':sample.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        elif channel!='Diffuse':sample.sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
        edit.connect_material_property(sample,'RGB' if channel in ['Diffuse','nor_dx'] else 'R',props[channel])
        if foliage and channel=='Diffuse':edit.connect_material_property(sample,'RGB',unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    edit.recompile_material(mat);lib.save_loaded_asset(mat)
    return mat

for entry in manifest['textures']: material(entry)
report=[]
for entry in manifest['models']:
    if not entry['files']:continue
    name='SM_'+entry['id'];path='/Game/Coastal/Meshes/'+name
    mesh=unreal.load_asset(path)
    if not mesh:
        task=unreal.AssetImportTask();task.filename=str(root/entry['file']);task.destination_path='/Game/Coastal/Meshes';task.destination_name=name;task.automated=True;task.save=True
        options=unreal.FbxImportUI();options.import_mesh=True;options.import_materials=False;options.import_textures=False;options.import_as_skeletal=False
        options.automated_import_should_detect_type=False;options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
        options.static_mesh_import_data.combine_meshes=True;options.static_mesh_import_data.auto_generate_collision=False
        options.static_mesh_import_data.import_mesh_lo_ds=False
        task.options=options;task.factory=unreal.FbxFactory();assets.import_asset_tasks([task]);mesh=unreal.load_asset(path)
    if not mesh:raise RuntimeError('Could not import '+name)
    mat=material(entry)
    for i in range(len(mesh.static_materials)):mesh.set_material(i,mat)
    if mesh_editor.get_lod_count(mesh)<3:
        ratios=[.16,.05,.012] if 'cliff' in name else [.35,.12,.03] if 'rock' in name else [1,.5,.2]
        opts=unreal.StaticMeshReductionOptions(auto_compute_lod_screen_size=False)
        opts.reduction_settings=[unreal.StaticMeshReductionSettings(percent_triangles=p,screen_size=s) for p,s in zip(ratios,[1,.22,.06])]
        mesh_editor.set_lods(mesh,opts)
    lib.save_loaded_asset(mesh)
    bounds=mesh.get_bounds()
    report.append({'mesh':path,'origin':str(bounds.origin),'extent':str(bounds.box_extent),'vertices':mesh_editor.get_number_verts(mesh,0)})
(root/'import-report.json').write_text(json.dumps(report,indent=2))
unreal.log('COASTAL_ASSET_IMPORT_COMPLETE')
