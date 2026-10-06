"""Open the tropical island overview; optionally capture repeatable review views."""
import unreal
from pathlib import Path
views=[
 ('island-overview',(-12500,-15500,12200),(0,0,1500)),
 ('island-beach',(-8550,-6350,570),(1600,900,3450)),
 ('island-tower',(-3300,-3550,4300),(1600,900,3430)),
 ('island-grips',(630,680,4130),(1110,1000,4090)),
]

def view(v):
    name,xyz,target=v
    p=unreal.Vector(*xyz);r=unreal.MathLibrary.find_look_at_rotation(p,unreal.Vector(*target))
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(p,r)
if 'TowerCaptureOnly' in unreal.SystemLibrary.get_command_line(): views=views[2:]
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True)
view(views[0])
unreal.log('TROPICAL_ISLAND_EDITOR_READY')
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
