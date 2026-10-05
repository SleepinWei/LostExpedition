"""Distinct surface scales for walls, limestone trim, soil, paving and sea."""
import unreal
from pathlib import Path
assets=unreal.AssetToolsHelpers.get_asset_tools();edit=unreal.MaterialEditingLibrary;lib=unreal.EditorAssetLibrary


# A small authored chamfer mesh removes razor-sharp edges on broken masonry.
block=unreal.load_asset('/Game/Coastal/Meshes/SM_WeatheredBlock')
if not block:
    task=unreal.AssetImportTask()
    task.filename=str(Path(unreal.Paths.project_dir())/'ArtSource/CoastalRemake/weathered_block.obj')
    task.destination_path='/Game/Coastal/Meshes';task.destination_name='SM_WeatheredBlock';task.automated=True;task.save=True
    opt=unreal.FbxImportUI();opt.import_materials=False;opt.import_textures=False;opt.import_as_skeletal=False
    opt.automated_import_should_detect_type=False;opt.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    opt.static_mesh_import_data.combine_meshes=True;opt.static_mesh_import_data.auto_generate_collision=True
    task.options=opt;task.factory=unreal.FbxFactory();assets.import_asset_tasks([task])

def node(m,c,x=0,y=0):return edit.create_material_expression(m,c,x,y)
def con(a,out,b,pin):
    if not edit.connect_material_expressions(a,out,b,pin):raise RuntimeError(pin)
def val(m,v):
    n=node(m,unreal.MaterialExpressionConstant);n.set_editor_property('r',v);return n
def rgb(m,c):
    n=node(m,unreal.MaterialExpressionConstant3Vector);n.set_editor_property('constant',unreal.LinearColor(*c,1));return n

def projection(m,tex,size,normal=False):
    o=node(m,unreal.MaterialExpressionTextureObject,-800,0);o.set_editor_property('texture',unreal.load_asset(tex))
    if not o.get_editor_property('texture'):raise RuntimeError(tex)
    f=node(m,unreal.MaterialExpressionMaterialFunctionCall,-400,0)
    f.set_material_function(unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/'+('WorldAlignedNormal' if normal else 'WorldAlignedTexture')))
    con(o,'',f,'TextureObject');con(rgb(m,(size,size,size)),'',f,'TextureSize');return f

def fresh(name):
    name+='V2'
    m=unreal.load_asset('/Game/Materials/'+name) or assets.create_asset(name,'/Game/Materials',unreal.Material,unreal.MaterialFactoryNew())
    edit.delete_all_material_expressions(m);return m

for name,folder,ident,res,size,tint in [
 ('M_ExpeditionStone','Coastal/Textures','old_stone_wall_02','2k',450,(.85,.84,.78)),
 ('M_ExpeditionRock','Coastal/Textures','rock_face','2k',620,(.72,.78,.74)),
 ('M_CoastalGround','Coastal/Textures','aerial_grass_rock','2k',700,(.70,.76,.56)),
 ('M_CoastalPlaster','Coastal/Textures','worn_mossy_plasterwall','2k',480,(.80,.77,.65)),
 ('M_CoastalTrim','Coastal/Textures','rock_face','2k',300,(1.18,1.12,.91)),
 ('M_CoastalPaving','Textures/PolyHaven','cobblestone_floor_01','1k',430,(.72,.78,.68)),
 ('M_ExpeditionWood','Textures/PolyHaven','wood_planks','1k',240,(.53,.49,.40)),
]:
    if lib.does_asset_exist('/Game/Materials/'+name+'V2'):continue
    m=fresh(name);m.set_editor_property('tangent_space_normal',False)
    def path(channel):return '/Game/'+folder+'/T_'+ident+'_'+channel+'_'+res
    d=projection(m,path('diff'),size);n=projection(m,path('nor_dx'),size,True);r=projection(m,path('rough'),size)
    mult=node(m,unreal.MaterialExpressionMultiply);con(d,'XYZ Texture',mult,'A');con(rgb(m,tint),'',mult,'B')
    edit.connect_material_property(mult,'',unreal.MaterialProperty.MP_BASE_COLOR)
    edit.connect_material_property(n,'XYZ Texture',unreal.MaterialProperty.MP_NORMAL)
    edit.connect_material_property(r,'XYZ Texture',unreal.MaterialProperty.MP_ROUGHNESS)
    edit.recompile_material(m);lib.save_loaded_asset(m)

# Analytic travelling waves avoid stretching a noisy albedo texture across the sea.
if not lib.does_asset_exist('/Game/Materials/M_CoastalWaterV2'):
    water=fresh('M_CoastalWater')
    world=node(water,unreal.MaterialExpressionWorldPosition)
    time=node(water,unreal.MaterialExpressionTime)
    custom=node(water,unreal.MaterialExpressionCustom)
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs=[]
    for name in ['P','T']:
        item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
    custom.set_editor_property('inputs',inputs)
    custom.set_editor_property('code','float a = P.x*0.0013+P.y*0.0019+T*0.7; float b=P.x*0.004-P.y*0.0025+T*1.1; float c=P.x*0.015+P.y*0.01+T*1.8; return normalize(float3(0.10*cos(a)+0.035*cos(c),0.075*cos(b)+0.03*cos(c),1));')
    con(world,'',custom,'P');con(time,'',custom,'T')
    edit.connect_material_property(custom,'',unreal.MaterialProperty.MP_NORMAL)
    fresnel=node(water,unreal.MaterialExpressionFresnel);fresnel.set_editor_property('exponent',4.5);fresnel.set_editor_property('base_reflect_fraction',.03)
    blend=node(water,unreal.MaterialExpressionLinearInterpolate)
    con(rgb(water,(.008,.047,.055)),'',blend,'A');con(rgb(water,(.12,.21,.23)),'',blend,'B');con(fresnel,'',blend,'Alpha')
    edit.connect_material_property(blend,'',unreal.MaterialProperty.MP_BASE_COLOR)
    edit.connect_material_property(val(water,.25),'',unreal.MaterialProperty.MP_ROUGHNESS)
    edit.connect_material_property(val(water,.8),'',unreal.MaterialProperty.MP_SPECULAR)
    edit.recompile_material(water);lib.save_loaded_asset(water)
# Tune limestone toward a pale neutral stone without rebuilding existing graph objects.
for name,fraction,tint in [('M_CoastalTrimV2',.9,(1.28,1.25,1.15)),('M_ExpeditionRockV2',.65,(.9,.93,.9))]:
    m=unreal.load_asset('/Game/Materials/'+name)
    expressions=edit.get_material_expressions(m)
    if not any(isinstance(e,unreal.MaterialExpressionDesaturation) for e in expressions):
        mult=edit.get_material_property_input_node(m,unreal.MaterialProperty.MP_BASE_COLOR)
        inputs=edit.get_inputs_for_material_expression(m,mult)
        desat=node(m,unreal.MaterialExpressionDesaturation)
        pins=edit.get_material_expression_input_names(desat)
        con(inputs[0],'XYZ Texture',desat,pins[0]);con(val(m,fraction),'',desat,pins[1])
        con(desat,'',mult,'A')
        inputs[1].set_editor_property('constant',unreal.LinearColor(r=tint[0],g=tint[1],b=tint[2],a=1))
        edit.recompile_material(m);lib.save_loaded_asset(m)
unreal.log('COASTAL_MATERIALS_COMPLETE')
