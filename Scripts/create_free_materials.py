"""Author UE world-aligned PBR materials from the locally available CC0 textures."""
import unreal
assets=unreal.AssetToolsHelpers.get_asset_tools()
edit=unreal.MaterialEditingLibrary
library=unreal.EditorAssetLibrary

def node(mat,cls,x,y): return edit.create_material_expression(mat,cls,x,y)
def connect(a,out,b,pin):
    if not edit.connect_material_expressions(a,out,b,pin): raise RuntimeError('Material connection failed: '+pin)

def projected(mat,texture_path,size,normal,row):
    texture=unreal.load_asset(texture_path)
    if not texture: raise RuntimeError('Missing texture: '+texture_path)
    obj=node(mat,unreal.MaterialExpressionTextureObject,-700,row)
    obj.set_editor_property('texture',texture)
    function=node(mat,unreal.MaterialExpressionMaterialFunctionCall,-350,row)
    function_name='WorldAlignedNormal' if normal else 'WorldAlignedTexture'
    if not function.set_material_function(unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/'+function_name)):
        raise RuntimeError('Could not load world-aligned material function')
    scale=node(mat,unreal.MaterialExpressionConstant3Vector,-700,row+160)
    scale.set_editor_property('constant',unreal.LinearColor(size,size,size,1))
    connect(obj,'',function,'TextureObject');connect(scale,'',function,'TextureSize')
    return function

for name,texture_id,size,tint in [
    ('Stone','cobblestone_floor_01',160,(.7,.64,.5)),
    ('Rock','rocky_terrain',240,(.55,.57,.51)),
    ('Wood','wood_planks',150,(.55,.42,.29)),
]:
    path='/Game/Materials/M_Expedition'+name
    material=unreal.load_asset(path) or assets.create_asset('M_Expedition'+name,'/Game/Materials',unreal.Material,unreal.MaterialFactoryNew())
    edit.delete_all_material_expressions(material)
    material.set_editor_property('tangent_space_normal',False)
    diffuse=projected(material,'/Game/Textures/PolyHaven/T_'+texture_id+'_diff_1k',size,False,-500)
    tone=node(material,unreal.MaterialExpressionConstant3Vector,0,-550)
    tone.set_editor_property('constant',unreal.LinearColor(*tint,1))
    mul=node(material,unreal.MaterialExpressionMultiply,220,-500)
    connect(diffuse,'XYZ Texture',mul,'A');connect(tone,'',mul,'B')
    edit.connect_material_property(mul,'',unreal.MaterialProperty.MP_BASE_COLOR)
    normal=projected(material,'/Game/Textures/PolyHaven/T_'+texture_id+'_nor_dx_1k',size,True,50)
    edit.connect_material_property(normal,'XYZ Texture',unreal.MaterialProperty.MP_NORMAL)
    rough=projected(material,'/Game/Textures/PolyHaven/T_'+texture_id+'_rough_1k',size,False,650)
    edit.connect_material_property(rough,'XYZ Texture',unreal.MaterialProperty.MP_ROUGHNESS)
    edit.recompile_material(material)
    library.save_loaded_asset(material)
unreal.log('FREE_ASSET_PBR_MATERIALS_READY')
