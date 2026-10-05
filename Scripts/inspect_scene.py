import unreal,json
from pathlib import Path
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
report=[]
for a in actors:
 row={'actor':a.get_actor_label(),'class':a.get_class().get_name(),'position':str(a.get_actor_location()),'rotation':str(a.get_actor_rotation()),'hidden':a.is_hidden_ed(),'components':len(a.get_components_by_class(unreal.ActorComponent))}
 for cls,fields in [(unreal.DirectionalLightComponent,['intensity','visible','light_color']),(unreal.SkyLightComponent,['intensity','visible','real_time_capture']),(unreal.ExponentialHeightFogComponent,['fog_density','fog_height_falloff'])]:
  c=a.get_component_by_class(cls)
  if c:
   row['light']={f:str(c.get_editor_property(f)) for f in fields}
 if isinstance(a,unreal.PostProcessVolume):
  s=a.settings;row['exposure']=[s.auto_exposure_min_brightness,s.auto_exposure_max_brightness]
 report.append(row)
Path(unreal.Paths.project_dir(),'Docs/scene-inspect.json').write_text(json.dumps(report,indent=2))
