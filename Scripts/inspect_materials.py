import unreal,json
from pathlib import Path
edit=unreal.MaterialEditingLibrary
lines=[]
for name in ['WorldAlignedTexture','WorldAlignedNormal']:
 f=unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions01/Texturing/'+name)
 for e in edit.get_material_function_expressions(f):
  cls=e.get_class().get_name()
  if cls=='MaterialExpressionFunctionInput':lines.append(name+' INPUT '+str(e.get_editor_property('input_name'))+' '+str(e.get_editor_property('input_type')))
for name in ['M_ExpeditionStoneV2','M_ExpeditionRockV2','M_CoastalGroundV2']:
 m=unreal.load_asset('/Game/Materials/'+name)
 for e in edit.get_material_expressions(m):
  cls=e.get_class().get_name();extra=''
  if cls=='MaterialExpressionConstant3Vector':extra=str(e.get_editor_property('constant'))
  if cls=='MaterialExpressionTextureObject':extra=str(e.get_editor_property('texture'))
  if cls=='MaterialExpressionMaterialFunctionCall':extra=str(edit.get_material_expression_input_names(e))+' <- '+str(edit.get_inputs_for_material_expression(m,e))
  lines.append(name+' '+cls+' '+extra)
Path(unreal.Paths.project_dir(),'Docs/material-inspect.txt').write_text('\n'.join(lines))
