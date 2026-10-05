"""Open at the approach to the fortress; optional repeatable visual review views."""
import unreal
from pathlib import Path
views=[
 ('tower-overview',(2300,-9500,4400),(7330,-3000,2540)),
 ('tower-route',(5900,-4800,2620),(7060,-3330,2740)),
 ('tower-summit',(6730,-4300,5460),(7380,-2760,4930)),
 ('scene-preview',(3400,40,915),(8400,200,1510)),
 ('courtyard-preview',(5690,-770,850),(8300,360,1640)),
 ('cliff-overview',(2600,-5500,2200),(6640,250,450)),
 ('landing-preview',(-2230,-450,210),(1300,0,520)),
]

def view(v):
    name,xyz,target=v
    p=unreal.Vector(*xyz);r=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(*target))
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(p,r)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True)
view(views[0])
unreal.log('COASTAL_REMAKE_EDITOR_READY')
if 'TowerCaptureOnly' in unreal.SystemLibrary.get_command_line(): views=views[:3]
if 'AdventureCapture' in unreal.SystemLibrary.get_command_line():
    state={'elapsed':0.,'index':0,'captured':False}
    def tick(delta):
        state['elapsed']+=delta
        if not state['captured'] and state['elapsed']>=(12 if state['index']==0 else 5):
            output=Path(unreal.Paths.project_dir())/'Docs'/(views[state['index']][0]+'.png')
            unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'HighResShot 1600x1000 filename="'+str(output)+'"')
            state['captured']=True
        if state['elapsed']>=(16 if state['index']==0 else 9):
            state['index']+=1;state['elapsed']=0.;state['captured']=False
            if state['index']>=len(views):
                unreal.unregister_slate_post_tick_callback(handle)
                if 'AdventureCaptureExit' in unreal.SystemLibrary.get_command_line():unreal.SystemLibrary.quit_editor()
                else:view(views[0])
            else:view(views[state['index']])
    handle=unreal.register_slate_post_tick_callback(tick)
