import unreal
from pathlib import Path
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for a in actors:
 if isinstance(a,unreal.DirectionalLight):
  a.set_actor_rotation(unreal.Rotator(pitch=-32,yaw=40,roll=0),False)
  a.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('light_color',unreal.Color(r=255,g=241,b=222,a=255))
 if a.get_class().get_name()=='ExpeditionGuard':a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=90,roll=0),False)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
p=Path(unreal.Paths.project_dir())/'Scripts/inspect_materials.py';exec(compile(p.read_text(),str(p),'exec'))
unreal.log('LIGHTING_ROTATION_FIXED')
