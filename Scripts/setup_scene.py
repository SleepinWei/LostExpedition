"""Build native UE materials and the editable adventure level. Run inside Unreal Python."""
import unreal
from pathlib import Path

assets=unreal.AssetToolsHelpers.get_asset_tools()
edit=unreal.MaterialEditingLibrary
library=unreal.EditorAssetLibrary

def node(mat, cls, x, y):
    return edit.create_material_expression(mat, cls, x, y)

def link(a, output, b, input_name):
    if not edit.connect_material_expressions(a, output, b, input_name):
        raise RuntimeError('Failed material connection: '+input_name)

master=unreal.load_asset('/Game/Materials/M_Expedition')
if not master:
    master=assets.create_asset('M_Expedition','/Game/Materials',unreal.Material,unreal.MaterialFactoryNew())
    color=node(master,unreal.MaterialExpressionVectorParameter,-600,-100)
    color.set_editor_property('parameter_name','Color')
    color.set_editor_property('default_value',unreal.LinearColor(.6,.55,.4,1))
    pos=node(master,unreal.MaterialExpressionWorldPosition,-1000,150)
    scale=node(master,unreal.MaterialExpressionMultiply,-800,150)
    scale.set_editor_property('const_b',.014)
    link(pos,'',scale,'A')
    noise=node(master,unreal.MaterialExpressionNoise,-600,150)
    noise.set_editor_property('quality',2)
    noise.set_editor_property('levels',3)
    noise.set_editor_property('output_min',.65)
    noise.set_editor_property('output_max',1.12)
    link(scale,'',noise,edit.get_material_expression_input_names(noise)[0])
    multiply=node(master,unreal.MaterialExpressionMultiply,-280,-100)
    link(color,'',multiply,'A');link(noise,'',multiply,'B')
    edit.connect_material_property(multiply,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=node(master,unreal.MaterialExpressionScalarParameter,-300,300)
    rough.set_editor_property('parameter_name','Roughness')
    rough.set_editor_property('default_value',.86)
    edit.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    edit.recompile_material(master)
    library.save_loaded_asset(master)

palette={
    'Stone':((.50,.45,.31),.94), 'Rock':((.16,.20,.17),.94),
    'Foliage':((.055,.22,.085),.9), 'Wood':((.26,.12,.045),.86),
    'Ledge':((.9,.65,.22),.72), 'Water':((.035,.29,.40),.23),
    'Supply':((.8,.12,.05),.65),
}
for name,(rgb,roughness) in palette.items():
    path='/Game/Materials/MI_'+name
    mat=unreal.load_asset(path)
    if not mat: mat=assets.create_asset('MI_'+name,'/Game/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    edit.set_material_instance_parent(mat,master)
    edit.set_material_instance_vector_parameter_value(mat,'Color',unreal.LinearColor(*rgb,1))
    edit.set_material_instance_scalar_parameter_value(mat,'Roughness',roughness)
    library.save_loaded_asset(mat)

material_script=Path(unreal.Paths.project_dir())/'Scripts/create_coastal_materials.py'
exec(compile(material_script.read_text(),str(material_script),'exec'))

core_script=Path(unreal.Paths.project_dir())/'Scripts/import_cliff_core.py'
exec(compile(core_script.read_text(),str(core_script),'exec'))

levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
map_path='/Game/Maps/CliffSanctuary'
# Explicit regeneration: this script owns the generated map.
if library.does_asset_exist(map_path):
    levels.load_level(map_path)
    for actor in actors.get_all_level_actors(): actors.destroy_actor(actor)
else: levels.new_level(map_path)

def spawn(cls, name, xyz, rot=None):
    actor=actors.spawn_actor_from_class(cls,unreal.Vector(*xyz),rot or unreal.Rotator())
    if not actor: raise RuntimeError('Could not spawn '+name)
    actor.set_actor_label(name)
    return actor

world_cls=unreal.load_class(None,'/Script/LostExpedition.ExpeditionWorld')
world=spawn(world_cls,'Coastal sanctuary | cliff and fortress', (0,0,0))
world.rebuild_scene()
start=spawn(unreal.PlayerStart,'Expedition landing / player start',(-1950,0,110))
sun=spawn(unreal.DirectionalLight,'Warm late-afternoon sun',(0,0,5000),unreal.Rotator(pitch=-32,yaw=40,roll=0))
light=sun.get_component_by_class(unreal.DirectionalLightComponent)
light.set_mobility(unreal.ComponentMobility.MOVABLE)
light.set_editor_property('atmosphere_sun_light',True)
light.set_editor_property('intensity',36000)
light.set_editor_property('light_color',unreal.Color(r=255,g=241,b=222,a=255))
light.set_editor_property('light_source_angle',1.3)
sky=spawn(unreal.SkyLight,'Ocean skylight',(0,0,1800))
sc=sky.get_component_by_class(unreal.SkyLightComponent)
sc.set_mobility(unreal.ComponentMobility.MOVABLE)
sc.set_editor_property('real_time_capture',True)
sc.set_editor_property('intensity',1.6)
spawn(unreal.SkyAtmosphere,'Island sky',(0,0,0))
cloud=spawn(unreal.VolumetricCloud,'Coastal cloud bank',(0,0,0))
cc=cloud.get_component_by_class(unreal.VolumetricCloudComponent)
cc.set_editor_property('material',unreal.load_asset('/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst'))
cc.set_editor_property('layer_bottom_altitude',2.0)
cc.set_editor_property('layer_height',4.0)
fog=spawn(unreal.ExponentialHeightFog,'Sea haze',(0,0,-600))
fc=fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fc.set_editor_property('fog_density',.026)
fc.set_editor_property('fog_height_falloff',.12)
fc.set_editor_property('start_distance',1000)
pp=spawn(unreal.PostProcessVolume,'Adventure color grade',(0,0,0))
pp.set_editor_property('unbound',True)
settings=pp.get_editor_property('settings')
for field,value in {
    'override_auto_exposure_min_brightness':True,'override_auto_exposure_max_brightness':True,
    'auto_exposure_min_brightness':12.8,'auto_exposure_max_brightness':12.8,
    'override_bloom_intensity':True,'bloom_intensity':.25,
    'override_vignette_intensity':True,'vignette_intensity':.12,
}.items(): settings.set_editor_property(field,value)
pp.set_editor_property('settings',settings)
loot_cls=unreal.load_class(None,'/Script/LostExpedition.ExpeditionLoot')
# Enum names are reflected from C++ and available to the Unreal Python runtime.
items=[
 ('LandingAmmo','AMMO',(-1680,350,55),'Landing ammunition'),
 ('LandingMedkit','MEDKIT',(-1630,-340,45),'Landing medical supplies'),
 ('OutlookCheckpoint','CHECKPOINT',(1800,-320,650),'Outlook checkpoint'),
 ('OutlookRelic','RELIC',(2350,550,675),'Relic I / navigator seal'),
 ('OutlookAmmo','AMMO',(3100,-400,665),'Bridge ammunition'),
 ('CourtyardCheckpoint','CHECKPOINT',(5370,-790,650),'Courtyard checkpoint'),
 ('CourtyardAmmo','AMMO',(5640,-760,665),'Courtyard ammunition'),
 ('CourtyardMedkit','MEDKIT',(6020,-780,665),'Courtyard medkit'),
 ('CourtyardGrenade','GRENADE',(5790,680,665),'Courtyard grenade'),
 ('SanctuaryKey','KEY',(6580,520,690),'Sanctuary key'),
 ('CourtyardRelic','RELIC',(6950,-740,675),'Relic II / captain seal'),
 ('SanctuaryGate','GATE',(8110,0,1100),'Sanctuary gate'),
 ('SanctuaryRelic','RELIC',(8750,-500,970),'Relic III / temple seal'),
 ('ExitBeacon','EXIT',(9620,0,865),'Expedition exit'),
]
for identity,kind,xyz,title in items:
    a=spawn(loot_cls,title,xyz)
    a.set_editor_property('kind',getattr(unreal.LootKind,kind))
    a.set_editor_property('item_id',identity)
    a.set_editor_property('display_name',title)
    # Trigger construction again after editable values are set.
    a.refresh_visuals()

guard_cls=unreal.load_class(None,'/Script/LostExpedition.ExpeditionGuard')
for name,xyz,patrol,training in [
 ('Outlook training sentry',(2750,660,620),(0,0,0),True),
 ('Courtyard sentry A',(5900,620,620),(400,0,0),False),
 ('Courtyard sentry B',(6550,-400,620),(0,450,0),False),
 ('Courtyard sentry C',(7040,350,620),(-300,0,0),False),
]:
    a=spawn(guard_cls,name,xyz,unreal.Rotator(pitch=0,yaw=90,roll=0))
    a.set_editor_property('patrol_offset',unreal.Vector(*patrol))
    a.set_editor_property('training_target',training)

levels.save_current_level()
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
unreal.log('LOST_EXPEDITION_SETUP_SUCCESS: coastal remake saved')
