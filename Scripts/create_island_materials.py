"""Unreal Python: island ground, shallow water, shoreline wash and authored palms."""
import json,unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir())/'ArtSource/Island'
assets=unreal.AssetToolsHelpers.get_asset_tools();edit=unreal.MaterialEditingLibrary;lib=unreal.EditorAssetLibrary
manifest=json.loads((root/'manifest.json').read_text())
for channel,file in manifest['textures'].items():
    name='T_'+Path(file).stem;path='/Game/Island/'+name
    if not lib.does_asset_exist(path):
        task=unreal.AssetImportTask();task.filename=str(root/file);task.destination_path='/Game/Island';task.destination_name=name;task.automated=True;task.save=True;assets.import_asset_tasks([task])
    texture=unreal.load_asset(path)
    if channel!='Diffuse':texture.set_editor_property('srgb',False)
    if channel=='nor_dx':texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
    lib.save_loaded_asset(texture)
def node(m,c):return edit.create_material_expression(m,c,0,0)
def con(a,out,b,pin):
    if not edit.connect_material_expressions(a,out,b,pin):raise RuntimeError('Material pin '+pin)
def scalar(m,v):n=node(m,unreal.MaterialExpressionConstant);n.set_editor_property('r',v);return n
def color(m,v):n=node(m,unreal.MaterialExpressionConstant3Vector);n.set_editor_property('constant',unreal.LinearColor(*v,1));return n
def output(n,pin,prop):edit.connect_material_property(n,pin,prop)
def fresh(name):return assets.create_asset(name,'/Game/Materials',unreal.Material,unreal.MaterialFactoryNew())
def custom(m,code,inputs,kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    c=node(m,unreal.MaterialExpressionCustom);c.set_editor_property('code',code);c.set_editor_property('output_type',kind);pins=[]
    for name,n,out in inputs:
        pin=unreal.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    c.set_editor_property('inputs',pins)
    for name,n,out in inputs:con(n,out,c,name)
    return c
def project(m,path,size,normal=False):
    tex=unreal.load_asset(path)
    if not tex:raise RuntimeError('Missing '+path)
    obj=node(m,unreal.MaterialExpressionTextureObject);obj.set_editor_property('texture',tex)
    f=node(m,unreal.MaterialExpressionMaterialFunctionCall);f.set_material_function(unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/'+('WorldAlignedNormal' if normal else 'WorldAlignedTexture')))
    con(obj,'',f,'TextureObject');con(color(m,(size,size,size)),'',f,'TextureSize');return f
if not lib.does_asset_exist('/Game/Materials/M_IslandTerrain'):
    m=fresh('M_IslandTerrain');m.set_editor_property('tangent_space_normal',False);weights=node(m,unreal.MaterialExpressionVertexColor)
    for channel,prop in [('diff',unreal.MaterialProperty.MP_BASE_COLOR),('nor_dx',unreal.MaterialProperty.MP_NORMAL),('rough',unreal.MaterialProperty.MP_ROUGHNESS)]:
        sources=[]
        for path,size in [('/Game/Coastal/Textures/T_aerial_grass_rock_'+channel+'_2k',750),('/Game/Island/T_coast_sand_01_'+channel+'_2k',650),('/Game/Coastal/Textures/T_rock_face_'+channel+'_2k',900)]:sources.append(project(m,path,size,channel=='nor_dx'))
        first=node(m,unreal.MaterialExpressionLinearInterpolate);con(sources[0],'XYZ Texture',first,'A');con(sources[1],'XYZ Texture',first,'B');con(weights,'R',first,'Alpha')
        second=node(m,unreal.MaterialExpressionLinearInterpolate);con(first,'',second,'A');con(sources[2],'XYZ Texture',second,'B');con(weights,'G',second,'Alpha');output(second,'',prop)
    edit.recompile_material(m);lib.save_loaded_asset(m)
if not lib.does_asset_exist('/Game/Materials/M_IslandWater'):
    m=fresh('M_IslandWater');m.set_editor_property('tangent_space_normal',False);p=node(m,unreal.MaterialExpressionWorldPosition);t=node(m,unreal.MaterialExpressionTime)
    c=custom(m,'float r=length(P.xy/float2(10500,8500)); float shallow=1-smoothstep(1.04,1.7,r); return lerp(float3(.008,.065,.095),float3(.035,.32,.30),shallow);',[('P',p,'')]);output(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
    n=custom(m,'float a=P.x*.002+P.y*.0013+T*.6; float b=P.y*.005-P.x*.0017+sin(a)*.9+T*.9; float fade=1/(1+length(P.xy)*.000018); return normalize(float3((cos(a)*.075+cos(b)*.025)*fade,sin(b)*.04*fade,1));',[('P',p,''),('T',t,'')]);output(n,'',unreal.MaterialProperty.MP_NORMAL)
    output(scalar(m,.24),'',unreal.MaterialProperty.MP_ROUGHNESS);output(scalar(m,.85),'',unreal.MaterialProperty.MP_SPECULAR);edit.recompile_material(m);lib.save_loaded_asset(m)
if not lib.does_asset_exist('/Game/Materials/M_IslandFoam'):
    m=fresh('M_IslandFoam');m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('two_sided',True)
    uv=node(m,unreal.MaterialExpressionTextureCoordinate);t=node(m,unreal.MaterialExpressionTime)
    alpha=custom(m,'float wave=sin(UV.x*2.7+sin(UV.x*1.3)+T*.4)*.5+.5; return saturate(wave-.15)*sin(UV.y*3.14159)*.5;',[('UV',uv,''),('T',t,'')],unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    output(color(m,(.68,.81,.74)),'',unreal.MaterialProperty.MP_BASE_COLOR);output(alpha,'',unreal.MaterialProperty.MP_OPACITY);output(scalar(m,.7),'',unreal.MaterialProperty.MP_ROUGHNESS);edit.recompile_material(m);lib.save_loaded_asset(m)
for name,leaf in [('M_PalmLeaf',True),('M_PalmBark',False)]:
    if lib.does_asset_exist('/Game/Materials/'+name):
        m=unreal.load_asset('/Game/Materials/'+name);m.set_editor_property('used_with_instanced_static_meshes',True)
        edit.recompile_material(m);lib.save_loaded_asset(m,only_if_is_dirty=False);continue
    m=fresh(name);m.set_editor_property('two_sided',True);m.set_editor_property('used_with_instanced_static_meshes',True)
    uv=node(m,unreal.MaterialExpressionTextureCoordinate)
    code='float vein=pow(abs(UV.x-.5)*2,.4); return lerp(float3(.026,.075,.012),float3(.07,.22,.025),vein);' if leaf else 'float ring=pow(sin(UV.y*6.283)*.5+.5,7); return lerp(float3(.24,.16,.085),float3(.12,.08,.04),ring*.55);'
    c=custom(m,code,[('UV',uv,'')]);output(c,'',unreal.MaterialProperty.MP_BASE_COLOR);output(scalar(m,.8),'',unreal.MaterialProperty.MP_ROUGHNESS)
    if leaf:m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE);output(color(m,(.08,.19,.025)),'',unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    edit.recompile_material(m);lib.save_loaded_asset(m)
path='/Game/Island/SM_CoconutPalm'
if not lib.does_asset_exist(path):
    task=unreal.AssetImportTask();task.filename=str(root/'coconut_palm.obj');task.destination_path='/Game/Island';task.destination_name='SM_CoconutPalm';task.automated=True;task.save=True
    opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False;opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH;opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=False;task.options=opt;task.factory=unreal.FbxFactory();assets.import_asset_tasks([task])
mesh=unreal.load_asset(path)
for i,slot in enumerate(mesh.static_materials):mesh.set_material(i,unreal.load_asset('/Game/Materials/'+('M_PalmLeaf' if 'leaf' in str(slot.material_slot_name).lower() else 'M_PalmBark')))
lib.save_loaded_asset(mesh)
unreal.log('ISLAND_MATERIALS_READY')
# Reduce pale foliage glare and keep the jungle canopy distinct from the dry masonry.
for name,prefix,tint,leaf in [
 ('M_IslandCanopy','island_tree_01_leaves',(.38,.62,.30),True),
 ('M_IslandTrunk','island_tree_01',(.48,.46,.36),False),
 ('M_IslandBranches','island_tree_01_branches',(.48,.46,.36),False),
]:
    if lib.does_asset_exist('/Game/Materials/'+name):continue
    m=fresh(name);m.set_editor_property('two_sided',True);m.set_editor_property('used_with_instanced_static_meshes',True)
    def sample(channel,normal=False):
        n=node(m,unreal.MaterialExpressionTextureSample);tex=unreal.load_asset('/Game/Textures/PolyHaven/T_'+prefix+'_'+channel+'_1k')
        if not tex:raise RuntimeError('Missing canopy texture '+prefix+' '+channel)
        n.set_editor_property('texture',tex)
        if normal:n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        elif channel!='diff':n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        return n
    diffuse=sample('diff');multiply=node(m,unreal.MaterialExpressionMultiply);con(diffuse,'RGB',multiply,'A');con(color(m,tint),'',multiply,'B');output(multiply,'',unreal.MaterialProperty.MP_BASE_COLOR)
    output(sample('nor_dx',True),'RGB',unreal.MaterialProperty.MP_NORMAL);output(sample('rough'),'R',unreal.MaterialProperty.MP_ROUGHNESS);output(scalar(m,.15),'',unreal.MaterialProperty.MP_SPECULAR)
    if leaf:
        m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        output(sample('alpha'),'R',unreal.MaterialProperty.MP_OPACITY_MASK);output(color(m,(.045,.09,.02)),'',unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    edit.recompile_material(m);lib.save_loaded_asset(m)
